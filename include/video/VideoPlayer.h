#pragma once
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <vector>
#include <chrono>
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"

namespace te {

// video frame data structure
struct VideoFrame {
    std::vector<uint8_t> data;  // RGB data
    int width;
    int height;
    double timestamp;
    bool isValid;
    
    VideoFrame() : width(0), height(0), timestamp(0.0), isValid(false) {}
    VideoFrame(int w, int h) : width(w), height(h), timestamp(0.0), isValid(true) {
        data.resize(w * h * 3); // RGB format
    }
};

// thread-safe ring buffer
template<typename T>
class RingBuffer {
public:
    RingBuffer(size_t capacity) : capacity_(capacity), head_(0), tail_(0), size_(0) {
        buffer_.resize(capacity);
    }
    
    bool push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size_ >= capacity_) {
            return false; // buffer full
        }
        
        buffer_[tail_] = item;
        tail_ = (tail_ + 1) % capacity_;
        size_++;
        condition_.notify_one();
        return true;
    }
    
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return size_ > 0; });
        
        if (size_ == 0) {
            return false;
        }
        
        item = buffer_[head_];
        head_ = (head_ + 1) % capacity_;
        size_--;
        return true;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_ == 0;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        head_ = tail_ = size_ = 0;
    }

private:
    std::vector<T> buffer_;
    size_t capacity_;
    size_t head_;
    size_t tail_;
    size_t size_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
};

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();
    
    // video load and control
    bool LoadVideo(const std::string& filePath);
    void Play();
    void Pause();
    void Stop();
    void Seek(double timeSeconds);
    
    // state query
    bool IsPlaying() const { return isPlaying_; }
    bool IsLoaded() const { return isLoaded_; }
    double GetCurrentTime() const { return currentTime_; }
    double GetDuration() const { return duration_; }
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    double GetFrameRate() const { return frameRate_; }
    
    // frame get
    bool GetCurrentFrame(VideoFrame& frame);
    void Update(); // update current time, get new frame
    
    // buffer management
    void SetBufferSize(size_t size) { frameBuffer_ = std::make_unique<RingBuffer<VideoFrame>>(size); }
    size_t GetBufferSize() const { return frameBuffer_ ? frameBuffer_->size() : 0; }

private:
    // video decoding thread
    void DecodingThread();
    void Cleanup();
    
    // video file information
    std::string filePath_;
    int width_;
    int height_;
    double frameRate_;
    double duration_;
    
    // playing state
    std::atomic<bool> isPlaying_;
    std::atomic<bool> isLoaded_;
    std::atomic<bool> shouldStop_;
    std::atomic<double> currentTime_;
    
    // thread and synchronization
    std::unique_ptr<std::thread> decodingThread_;
    std::mutex frameMutex_;
    std::condition_variable frameCondition_;
    
    // frame buffer
    std::unique_ptr<RingBuffer<VideoFrame>> frameBuffer_;
    VideoFrame currentFrame_;
    
    // time management
    std::chrono::high_resolution_clock::time_point playStartTime_;
    double pausedTime_;
    
    // FFmpeg related members
    AVFormatContext* formatContext_;
    AVCodecContext* codecContext_;
    AVCodec* codec_;
    AVFrame* frame_;
    AVFrame* frameRGB_;
    AVPacket* packet_;
    SwsContext* swsContext_;
    int videoStreamIndex_;
    bool ffmpegInitialized_;
    
    // initialize FFmpeg
    bool InitializeFFmpeg();
    void CleanupFFmpeg();
    
    // decoding related
    bool DecodeFrame(VideoFrame& frame);
    void GenerateTestPattern(VideoFrame& frame);
};

} // namespace te
