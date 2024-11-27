#include <just_annotate/video_file.h>

#include <cmath>
#include <chrono>
#include <cstring>
#include <deque>

#include <GL/gl.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/app.h>
#include <png.h>
#include <spdlog/spdlog.h>

namespace just_annotate {

struct VideoFile::Impl {
    GstElement* pipeline = nullptr;
    GstElement* uridecodebin = nullptr;
    GstElement* videoconvert = nullptr;
    GstElement* framesink = nullptr;
    GMainLoop* loop = nullptr;
    std::deque<GstBuffer*> frame_buffer;
    bool new_frame = false;
    bool wait_for_next_frame = true;
    bool end_of_stream = false;
    bool is_forward = true;
};

void sync_bus_call(GstBus* /* bus */, GstMessage * msg, gpointer data)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
    {
        g_print("End of stream\n");
        VideoFile::Impl* video_file_impl = static_cast<VideoFile::Impl*>(data);
        if (video_file_impl->is_forward) {
            auto seek_event = gst_event_new_seek (1, GST_FORMAT_TIME,
                GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE), GST_SEEK_TYPE_SET,
                0, GST_SEEK_TYPE_END, 0);
            gst_element_send_event(video_file_impl->pipeline, seek_event);

            video_file_impl->end_of_stream = true;
            gst_element_set_state(video_file_impl->pipeline, GST_STATE_PAUSED);
        }
        else {
            auto seek_event = gst_event_new_seek (1, GST_FORMAT_TIME,
                GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE), GST_SEEK_TYPE_SET,
                0, GST_SEEK_TYPE_END, 0);
            gst_element_send_event(video_file_impl->pipeline, seek_event);
            video_file_impl->is_forward = true;
        }
    }
    break;
    case GST_MESSAGE_ERROR:
    {
        gchar *debug;
        GError *error;
        gst_message_parse_error(msg, &error, &debug);
        g_printerr("Error: %s\n", error->message);
        g_error_free(error);
        g_free(debug);
        break;
    }
    break;
    default:
      break;
  }
}

void save_frame_to_png(GstBuffer* buffer, const char* filename)
{
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        spdlog::error("Failed to map GstBuffer!");
        return;
    }

    auto v_meta = gst_buffer_get_video_meta(buffer);
    GstVideoInfo v_info;
    gst_video_info_set_format(&v_info, v_meta->format, v_meta->width,
        v_meta->height);
    int width = v_meta->width;
    int height = v_meta->height;

    // Use libpng to write the data
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        spdlog::error("Failed to open file for writing: {}", filename);
        gst_buffer_unmap(buffer, &map);
        return;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        spdlog::error("Failed to create PNG write structure!");
        fclose(fp);
        gst_buffer_unmap(buffer, &map);
        return;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        spdlog::error("Failed to create PNG info structure!");
        png_destroy_write_struct(&png_ptr, nullptr);
        fclose(fp);
        gst_buffer_unmap(buffer, &map);
        return;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        spdlog::error("Error during PNG creation!");
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        gst_buffer_unmap(buffer, &map);
        return;
    }

    png_init_io(png_ptr, fp);

    // Set PNG header
    png_set_IHDR(
        png_ptr,
        info_ptr,
        width,
        height,
        8,                // Bit depth
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );

    png_write_info(png_ptr, info_ptr);

    // Write image data row by row
    const guint8* data = map.data;
    for (int y = 0; y < height; ++y) {
        png_write_row(png_ptr, data + y * width * 4); // 4 bytes per pixel (RGBA)
    }

    // Finish writing
    png_write_end(png_ptr, nullptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    gst_buffer_unmap(buffer, &map);
}

void on_gst_buffer(GstElement* sink, gpointer data)
{
    VideoFile::Impl* video_file_impl = static_cast<VideoFile::Impl*>(data);
    if (video_file_impl->end_of_stream) {
        return;
    }

    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) {
        return;
    }

    // decrement reference counts of expired frames
    while (video_file_impl->frame_buffer.size() > 30) {
        gst_buffer_unref(video_file_impl->frame_buffer.front());
        video_file_impl->frame_buffer.pop_front();
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);

    // increment the reference count for the new frame
    video_file_impl->frame_buffer.push_back(buffer);
    gst_buffer_ref(video_file_impl->frame_buffer.back());

    video_file_impl->new_frame = true;
}

VideoFile::VideoFile() : impl_(std::make_unique<Impl>()) {
}

VideoFile::~VideoFile() {
    // set pipeline state to null
    pause(true);
    gst_element_set_state(impl_->pipeline, GST_STATE_NULL);

    // remove bus watch
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE (impl_->pipeline));
    gst_bus_remove_watch(bus);
    gst_object_unref(bus);

    // unreference pipeline resources
    gst_object_unref(impl_->pipeline);
    gst_object_unref(impl_->uridecodebin);
    gst_object_unref(impl_->videoconvert);
    gst_object_unref(impl_->framesink);
    for (auto frame: impl_->frame_buffer) {
        gst_buffer_unref(frame);
    }
    impl_->frame_buffer.clear();
    g_main_loop_unref(impl_->loop);
}

VideoFile::Ptr VideoFile::open(const std::string& path) {
    auto video_file = std::shared_ptr<VideoFile>(new VideoFile());

    video_file->path_ = path;
    video_file->impl_->loop = g_main_loop_new(nullptr, false);

    std::string config = "uridecodebin name=source uri=file://";
    config += path;
    config += " ! videoconvert ! video/x-raw,format=RGBA ! appsink name=framesink sync=1";
    video_file->impl_->pipeline = gst_parse_launch(config.c_str(), nullptr);
    video_file->impl_->uridecodebin = gst_bin_get_by_name(GST_BIN(video_file->impl_->pipeline), "source");
    video_file->impl_->videoconvert = gst_bin_get_by_name(GST_BIN(video_file->impl_->pipeline), "convert");
    video_file->impl_->framesink = gst_bin_get_by_name(GST_BIN(video_file->impl_->pipeline), "framesink");

    g_object_set(video_file->impl_->framesink, "emit-signals", TRUE, nullptr);
    g_signal_connect(video_file->impl_->framesink, "new-sample", G_CALLBACK(on_gst_buffer), video_file->impl_.get());

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE (video_file->impl_->pipeline));
    gst_bus_add_signal_watch(bus);
    gst_bus_enable_sync_message_emission(bus);
    g_signal_connect(bus, "sync-message", G_CALLBACK(sync_bus_call), video_file->impl_.get());
    gst_object_unref(bus);

    gst_element_set_state(video_file->impl_->pipeline, GST_STATE_PLAYING);
    video_file->impl_->wait_for_next_frame = true;
    video_file->update();

    return video_file;
}

const std::string& VideoFile::getPath() const {
    return path_;
}

void VideoFile::init(int argc, char* argv[]) {
    gst_init(&argc, &argv);
}


float VideoFile::getDuration() const {
    return duration_;
}

int VideoFile::getWidth() const {
    return width_;
}

int VideoFile::getHeight() const {
    return height_;
}

uint32_t VideoFile::getTextureId() {
    return texture_id_;
}

void VideoFile::update() {
    g_main_context_iteration(g_main_loop_get_context(impl_->loop), false);

    if (impl_->end_of_stream) {
        pause(true);
        return;
    }

    if (duration_ == 0) {
        // Query the duration
        gint64 duration_ns = GST_CLOCK_TIME_NONE;
        if (gst_element_query_duration(impl_->pipeline, GST_FORMAT_TIME, &duration_ns)) {
            duration_ = duration_ns * 1e-9;
        }
    }

    gint64 position_ns = GST_CLOCK_TIME_NONE;
    if (gst_element_query_position(impl_->pipeline, GST_FORMAT_TIME, &position_ns)) {
        position_ = position_ns * 1e-9;
    }

    if (impl_->new_frame) {
        impl_->new_frame = false;

        if (impl_->wait_for_next_frame) {
            impl_->wait_for_next_frame = false;
            if (is_paused_) {
                pause(true);
            }
            setDirection(true);
        }

        auto frame = impl_->frame_buffer.back();
        auto mem = gst_buffer_peek_memory(frame, 0);

        auto v_meta = gst_buffer_get_video_meta(frame);
        GstVideoInfo v_info;
        gst_video_info_set_format(&v_info, v_meta->format, v_meta->width,
            v_meta->height);
        width_ = v_meta->width;
        height_ = v_meta->height;

        GstMapInfo map;
        if (!gst_buffer_map(frame, &map, GST_MAP_READ)) {
            spdlog::error("Failed to map GstBuffer!");
            return;
        }

        if (texture_id_ == 0) {
            glGenTextures(1, &texture_id_);
        }

        glBindTexture(GL_TEXTURE_2D, texture_id_);

        // Set texture parameters (filtering, wrapping)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Upload pixel data to the GPU
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, map.data);

        gst_buffer_unmap(frame, &map);

        // Unbind the texture
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

bool VideoFile::isPaused() const {
    return is_paused_;
}

void VideoFile::play() {
    pause(false);
}

void VideoFile::stop() {
}

void VideoFile::pause(bool is_paused) {
    // don't unpause if the end of the stream has been reached
    if (!is_paused && impl_->end_of_stream) {
        return;
    }

    is_paused_ = is_paused;
    if (is_paused) {
        if (!impl_->wait_for_next_frame) {
            gst_element_set_state(impl_->pipeline, GST_STATE_PAUSED);
        }
    }
    else {
        gst_element_set_state(impl_->pipeline, GST_STATE_PLAYING);
        impl_->wait_for_next_frame = false;
        last_seek_ = -1;
        next_seek_ = -1;
    }
}

void VideoFile::seek(double position) {
    // ignore seeks to the end if the end of stream has already been reached
    if (impl_->end_of_stream && position >= duration_) {
        return;
    }

    // wait for existing seeking to be done
    if (impl_->wait_for_next_frame && !impl_->end_of_stream) {
        next_seek_ = position;
        return;
    }

    // ignore repeat seeks to the same position
    if (position == last_seek_ && !impl_->end_of_stream) {
        return;
    }

    position = std::max(0.0, std::min(duration_, position));

    impl_->end_of_stream = false;

    gint64 position_ns = static_cast<gint64>(std::round(position * 1e9));
    if (!gst_element_seek(impl_->pipeline, 1.0, GST_FORMAT_TIME,
                          GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                          GST_SEEK_TYPE_SET, position_ns, GST_SEEK_TYPE_NONE,
                          GST_CLOCK_TIME_NONE))
    {
        spdlog::error("Failed to seek.");
        return;
    }
    gst_element_set_state(impl_->pipeline, GST_STATE_PLAYING);
    impl_->wait_for_next_frame = true;
    next_seek_ = -1;
    last_seek_ = position;
}

void VideoFile::seekRelative(double offset) {
    gint64 position_ns = GST_CLOCK_TIME_NONE;
    if (gst_element_query_position(impl_->pipeline, GST_FORMAT_TIME, &position_ns)) {
        double seek_pos = position_ns / 1e9 + offset;
        seek_pos = std::max(0.0, std::min(seek_pos, duration_));
        seek(seek_pos);
    }
}

void VideoFile::setDirection(bool forward) {
    gint64 position_ns = GST_CLOCK_TIME_NONE;
    if (gst_element_query_position(impl_->pipeline, GST_FORMAT_TIME, &position_ns)) {
        if (forward) {
            auto seek_event = gst_event_new_seek (1, GST_FORMAT_TIME,
                GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE), GST_SEEK_TYPE_SET,
                position_ns, GST_SEEK_TYPE_END, 0);
            gst_element_send_event(impl_->pipeline, seek_event);
        }
        else{
            auto seek_event = gst_event_new_seek (-1, GST_FORMAT_TIME,
                GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE), GST_SEEK_TYPE_SET,
                0, GST_SEEK_TYPE_SET, position_ns);
            gst_element_send_event(impl_->pipeline, seek_event);
        }
    }

    impl_->is_forward = forward;
}

void VideoFile::step(bool forward) {
    if (!forward && position_ <= 0.2) {
        seek(0);
        gst_element_set_state(impl_->pipeline, GST_STATE_PLAYING);
        impl_->wait_for_next_frame = true;
        return;
    }

    if (forward && position_ >= duration_) {
        return;
    }

    if (!forward) {
        setDirection(false);
    }

    // create and send step event
    GstEvent* step_event = gst_event_new_step(GST_FORMAT_BUFFERS, 1, 1, TRUE, FALSE);
    gst_element_send_event(impl_->pipeline, step_event);

    gst_element_set_state(impl_->pipeline, GST_STATE_PLAYING);
    impl_->wait_for_next_frame = true;
}

double VideoFile::getPosition() const {
    return position_;
}

bool VideoFile::exiting() {
    return false;
}

} // namespace just_annotate
