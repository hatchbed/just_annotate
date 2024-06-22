#pragma once

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace just_annotate {

class VideoFile {
  public:
    using Ptr      = std::shared_ptr<VideoFile>;
    using ConstPtr = std::shared_ptr<const VideoFile>;

    ~VideoFile();

    static VideoFile::Ptr open(const std::string& path);
    static void init(int argc, char* argv[]);

    const std::string& getPath() const;
    float getDuration() const;
    int getWidth() const;
    int getHeight() const;
    uint32_t getTextureId();
    void handleFrames();
    bool isPaused() const;
    double getPosition() const;

    void update();

    void play();
    void stop();
    void pause(bool is_paused);
    void seek(double position);
    void seekRelative(double offset);
    void step(bool forward);
    void setDirection(bool forward);

    struct Impl;

  private:
    VideoFile();
    bool exiting();

    std::string path_;
    int width_ = 0;
    int height_ = 0;
    double duration_ = 0;
    double position_ = 0;
    uint32_t texture_id_ = 0;
    bool do_exit_ = false;
    bool is_paused_ = true;
    double next_seek_ = -1;
    double last_seek_ = -1;

    std::unique_ptr<Impl> impl_;

};

} // namespace just_annotate
