#include <just_annotate/video_file.h>

#include <cmath>
#include <chrono>
#include <cstring>
#include <deque>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GLFW/glfw3.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>
#include <gst/gl/x11/gstgldisplay_x11.h>
#include <gst/video/video.h>
#include <gst/app/app.h>
#include <spdlog/spdlog.h>

namespace just_annotate {

struct VideoFile::Impl {
    GstElement* pipeline = nullptr;
    GstElement* uridecodebin = nullptr;
    GstElement* videoconvert = nullptr;
    GstElement* glupload = nullptr;
    GstElement* glcolorconvert = nullptr;
    GstElement* fakesink = nullptr;
    GstGLDisplay *gldisplay = nullptr;
    GstGLContext *glcontext = nullptr;
    GMainLoop* loop = nullptr;
    std::deque<GstBuffer*> frame_buffer;
    bool need_display_context = true;
    bool need_gl_context = true;
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
    case GST_MESSAGE_NEED_CONTEXT:
    {
      VideoFile::Impl* video_file_impl = static_cast<VideoFile::Impl*>(data);

      const gchar *context_type;

      gst_message_parse_context_type (msg, &context_type);

      if (g_strcmp0 (context_type, GST_GL_DISPLAY_CONTEXT_TYPE) == 0) {
        GstContext *display_context = gst_context_new(GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
        gst_context_set_gl_display (display_context, video_file_impl->gldisplay);
        gst_element_set_context(GST_ELEMENT (msg->src), display_context);
        video_file_impl->need_display_context = false;
      } else if (g_strcmp0 (context_type, "gst.gl.app_context") == 0) {
        GstContext *app_context = gst_context_new("gst.gl.app_context", TRUE);
        GstStructure *s = gst_context_writable_structure (app_context);
        gst_structure_set (s, "context", GST_TYPE_GL_CONTEXT, video_file_impl->glcontext, NULL);
        gst_element_set_context (GST_ELEMENT (msg->src), app_context);
        video_file_impl->need_gl_context = false;
      }
      break;
    }
    default:
      break;
  }
}

void on_gst_buffer(GstElement* /* element */, GstBuffer* buf, GstPad* /* pad */, gpointer data)
{
    VideoFile::Impl* video_file_impl = static_cast<VideoFile::Impl*>(data);

    if (video_file_impl->end_of_stream) {
        return;
    }

    // decrement reference counts of expired frames
    while (video_file_impl->frame_buffer.size() > 30) {
        gst_buffer_unref(video_file_impl->frame_buffer.front());
        video_file_impl->frame_buffer.pop_front();
    }

    // increment the reference count for the new frame
    video_file_impl->frame_buffer.push_back(buf);
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
    gst_object_unref(impl_->glupload);
    gst_object_unref(impl_->glcolorconvert);
    gst_object_unref(impl_->fakesink);
    gst_object_unref(impl_->gldisplay);
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

    // capture the current GL context so it can be shared with the GST GL context
    auto glfw_window = glfwGetCurrentContext();
    auto gl_context = glXGetCurrentContext();
    auto gl_display = glXGetCurrentDisplay();

    // the current GL context needs to be disabled first, so a dummy hidden window will be created,
    // set to be the current GL context and then destroyed
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
    auto dummy_window = glfwCreateWindow(640, 480, "", nullptr, nullptr);
    glfwMakeContextCurrent(dummy_window);
    glfwDestroyWindow(dummy_window);

    // wrap current GL display and context in GST objects
    video_file->impl_->gldisplay = (GstGLDisplay*)gst_gl_display_x11_new_with_display(gl_display);
    video_file->impl_->glcontext = gst_gl_context_new_wrapped(video_file->impl_->gldisplay,
                                                              (guintptr)gl_context,
                                                              GST_GL_PLATFORM_GLX,
                                                              GST_GL_API_OPENGL);

    std::string config = "uridecodebin name=source uri=file://";
    config += path;
    config += " ! videoconvert name=convert ! glupload name=upload ! glcolorconvert name=colorconvert ! video/x-raw(memory:GLMemory),format=RGBA ! fakesink name=fakesink sync=1";
    video_file->impl_->pipeline = gst_parse_launch(config.c_str(), nullptr);
    video_file->impl_->uridecodebin = gst_bin_get_by_name(GST_BIN(video_file->impl_->pipeline), "source");
    video_file->impl_->videoconvert = gst_bin_get_by_name(GST_BIN(video_file->impl_->pipeline), "convert");
    video_file->impl_->glupload = gst_bin_get_by_name(GST_BIN(video_file->impl_->pipeline), "upload");
    video_file->impl_->glcolorconvert = gst_bin_get_by_name(GST_BIN(video_file->impl_->pipeline), "colorconvert");
    video_file->impl_->fakesink = gst_bin_get_by_name(GST_BIN(video_file->impl_->pipeline), "fakesink");

    g_object_set (G_OBJECT(video_file->impl_->fakesink), "signal-handoffs", true, nullptr);
    g_signal_connect(video_file->impl_->fakesink , "handoff", G_CALLBACK(on_gst_buffer), video_file->impl_.get());

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE (video_file->impl_->pipeline));
    gst_bus_add_signal_watch(bus);
    gst_bus_enable_sync_message_emission(bus);
    g_signal_connect(bus, "sync-message", G_CALLBACK(sync_bus_call), video_file->impl_.get());
    gst_object_unref(bus);

    gst_element_set_state(video_file->impl_->pipeline, GST_STATE_PLAYING);
    video_file->impl_->wait_for_next_frame = true;

    bool context_restored = false;
    for (size_t i = 0; i < 1000; i++) {
        video_file->update();
        if (!video_file->impl_->need_gl_context && !video_file->impl_->need_display_context) {
            glfwMakeContextCurrent(glfw_window);
            context_restored = true;
            break;
        }
    }
    if (!context_restored) {
        spdlog::error("Failed to share GL context.");
        glfwMakeContextCurrent(glfw_window);
        return {};
    }

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
        GstGLMemory* gl_memory = (GstGLMemory*) mem;
        gl_memory->mem.context->gl_vtable->Flush();

        auto v_meta = gst_buffer_get_video_meta(frame);
        GstVideoInfo v_info;
        gst_video_info_set_format(&v_info, v_meta->format, v_meta->width,
            v_meta->height);
        width_ = v_meta->width;
        height_ = v_meta->height;

        GstVideoFrame v_frame;
        gst_video_frame_map(&v_frame, &v_info, frame, (GstMapFlags) (GST_MAP_READ | GST_MAP_GL));

        texture_id_ = *(guint *) v_frame.data[0];
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
