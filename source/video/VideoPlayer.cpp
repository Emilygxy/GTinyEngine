#include "video/VideoPlayer.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cmath>
#include "filesystem.h"
//#include "FFmpegLoader.h"
// FFmpeg includes
#ifdef __cplusplus
extern "C"
{
#endif

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
//
//#pragma comment (lib, "Ws2_32.lib")  
//#pragma comment (lib, "avcodec.lib")
//#pragma comment (lib, "avdevice.lib")
////#pragma comment (lib, "avfilter.lib")
//#pragma comment (lib, "avformat.lib")
//#pragma comment (lib, "avutil.lib")
//#pragma comment (lib, "swresample.lib")
//#pragma comment (lib, "swscale.lib")

namespace te {

VideoPlayer::VideoPlayer() 
    : width_(0), height_(0), frameRate_(30.0), duration_(0.0)
    , isPlaying_(false), isLoaded_(false), shouldStop_(false)
    , currentTime_(0.0), pausedTime_(0.0)
    , formatContext_(nullptr), codecContext_(nullptr), codec_(nullptr)
    , frame_(nullptr), frameRGB_(nullptr), packet_(nullptr), swsContext_(nullptr)
    , videoStreamIndex_(-1), ffmpegInitialized_(false) {
    
    // create default size frame buffer
    frameBuffer_ = std::make_unique<RingBuffer<VideoFrame>>(10);
    
    // initialize FFmpeg
    InitializeFFmpeg();
    
    std::cout << "VideoPlayer created" << std::endl;
}

VideoPlayer::~VideoPlayer() {
    Cleanup();
    CleanupFFmpeg();
}

bool VideoPlayer::LoadVideo(const std::string& filePath) {
    std::cout << "VideoPlayer::LoadVideo - Loading: " << filePath << std::endl;
    
    // stop current playing
    Stop();
    
    // clean up previous resources
    CleanupFFmpeg();
    
    filePath_ = FileSystem::getPath(filePath);
    
    // check if file exists
    std::ifstream file(filePath_, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "VideoPlayer::LoadVideo - Failed to open file: " << filePath_ << std::endl;
        return false;
    }
    file.close();
    
    // initialize FFmpeg
    if (!InitializeFFmpeg()) {
        std::cout << "VideoPlayer::LoadVideo - Failed to initialize FFmpeg" << std::endl;
        return false;
    }
    
    // open video file
    if (avformat_open_input(&formatContext_, filePath_.c_str(), nullptr, nullptr) < 0) {
        std::cout << "VideoPlayer::LoadVideo - Could not open video file: " << filePath_ << std::endl;
        return false;
    }
    
    // get stream information
    if (avformat_find_stream_info(formatContext_, nullptr) < 0) {
        std::cout << "VideoPlayer::LoadVideo - Could not find stream information" << std::endl;
        avformat_close_input(&formatContext_);
        return false;
    }
    
    // find video stream
    videoStreamIndex_ = -1;
    for (unsigned int i = 0; i < formatContext_->nb_streams; i++) {
        if (formatContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex_ = i;
            break;
        }
    }
    
    if (videoStreamIndex_ == -1) {
        std::cout << "VideoPlayer::LoadVideo - Could not find video stream" << std::endl;
        avformat_close_input(&formatContext_);
        return false;
    }
    
    // get codec parameters
    AVCodecParameters* codecParams = formatContext_->streams[videoStreamIndex_]->codecpar;
    
    // find decoder
    codec_ = const_cast<AVCodec*>(avcodec_find_decoder(codecParams->codec_id));
    if (!codec_) {
        std::cout << "VideoPlayer::LoadVideo - Unsupported codec" << std::endl;
        avformat_close_input(&formatContext_);
        return false;
    }
    
    // create decoder context
    codecContext_ = avcodec_alloc_context3(codec_);
    if (!codecContext_) {
        std::cout << "VideoPlayer::LoadVideo - Could not allocate codec context" << std::endl;
        avformat_close_input(&formatContext_);
        return false;
    }
    
    // copy codec parameters to context
    if (avcodec_parameters_to_context(codecContext_, codecParams) < 0) {
        std::cout << "VideoPlayer::LoadVideo - Could not copy codec parameters" << std::endl;
        avcodec_free_context(&codecContext_);
        avformat_close_input(&formatContext_);
        return false;
    }
    
    // open decoder
    if (avcodec_open2(codecContext_, codec_, nullptr) < 0) {
        std::cout << "VideoPlayer::LoadVideo - Could not open codec" << std::endl;
        avcodec_free_context(&codecContext_);
        avformat_close_input(&formatContext_);
        return false;
    }
    
    // allocate frame memory
    frame_ = av_frame_alloc();
    frameRGB_ = av_frame_alloc();
    packet_ = av_packet_alloc();
    
    if (!frame_ || !frameRGB_ || !packet_) {
        std::cout << "VideoPlayer::LoadVideo - Could not allocate frames/packet" << std::endl;
        CleanupFFmpeg();
        return false;
    }
    
    // get video information
    width_ = codecContext_->width;
    height_ = codecContext_->height;
    
    // calculate frame rate
    AVRational frameRate = formatContext_->streams[videoStreamIndex_]->r_frame_rate;
    if (frameRate.den != 0) {
        frameRate_ = static_cast<double>(frameRate.num) / static_cast<double>(frameRate.den);
    } else {
        frameRate_ = 30.0; // default frame rate
    }
    
    // calculate duration
    if (formatContext_->duration != AV_NOPTS_VALUE) {
        duration_ = static_cast<double>(formatContext_->duration) / AV_TIME_BASE;
    } else {
        duration_ = 10.0; // default duration
    }
    
    // set RGB frame parameters
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width_, height_, 1);
    uint8_t* buffer = static_cast<uint8_t*>(av_malloc(numBytes * sizeof(uint8_t)));
    av_image_fill_arrays(frameRGB_->data, frameRGB_->linesize, buffer, AV_PIX_FMT_RGB24, width_, height_, 1);
    
    // create image conversion context
    swsContext_ = sws_getContext(width_, height_, codecContext_->pix_fmt,
                                 width_, height_, AV_PIX_FMT_RGB24,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);
    
    if (!swsContext_) {
        std::cout << "VideoPlayer::LoadVideo - Could not create SWS context" << std::endl;
        CleanupFFmpeg();
        return false;
    }
    
    isLoaded_ = true;
    currentTime_ = 0.0;
    
    std::cout << "VideoPlayer::LoadVideo - Video loaded successfully" << std::endl;
    std::cout << "  Width: " << width_ << ", Height: " << height_ << std::endl;
    std::cout << "  Frame Rate: " << frameRate_ << " fps" << std::endl;
    std::cout << "  Duration: " << duration_ << " seconds" << std::endl;
    std::cout << "  Codec: " << codec_->name << std::endl;
    
    return true;
}

void VideoPlayer::Play() {
    if (!isLoaded_) {
        std::cout << "VideoPlayer::Play - No video loaded" << std::endl;
        return;
    }
    
    if (isPlaying_) {
        std::cout << "VideoPlayer::Play - Already playing" << std::endl;
        return;
    }
    
    isPlaying_ = true;
    shouldStop_ = false;
    playStartTime_ = std::chrono::high_resolution_clock::now();
    
    // start decoding thread
    decodingThread_ = std::make_unique<std::thread>(&VideoPlayer::DecodingThread, this);
    
    std::cout << "VideoPlayer::Play - Started playing" << std::endl;
}

void VideoPlayer::Pause() {
    if (!isPlaying_) {
        return;
    }
    
    isPlaying_ = false;
    pausedTime_ = currentTime_;
    
    std::cout << "VideoPlayer::Pause - Paused at " << pausedTime_ << "s" << std::endl;
}

void VideoPlayer::Stop() {
    isPlaying_ = false;
    shouldStop_ = true;
    
    // wait for decoding thread to end
    if (decodingThread_ && decodingThread_->joinable()) {
        decodingThread_->join();
    }
    
    currentTime_ = 0.0;
    pausedTime_ = 0.0;
    
    // clear buffer
    if (frameBuffer_) {
        frameBuffer_->clear();
    }
    
    std::cout << "VideoPlayer::Stop - Stopped" << std::endl;
}

void VideoPlayer::Seek(double timeSeconds) {
    if (!isLoaded_) {
        return;
    }
    
    currentTime_ = std::max(0.0, std::min(timeSeconds, duration_));
    
    // clear buffer, restart decoding
    if (frameBuffer_) {
        frameBuffer_->clear();
    }
    
    std::cout << "VideoPlayer::Seek - Seeked to " << currentTime_ << "s" << std::endl;
}

bool VideoPlayer::GetCurrentFrame(VideoFrame& frame) {
    std::lock_guard<std::mutex> lock(frameMutex_);
    
    if (currentFrame_.isValid) {
        frame = currentFrame_;
        return true;
    }
    
    return false;
}

void VideoPlayer::Update() {
    if (!isPlaying_ || !isLoaded_) {
        return;
    }
    
    // update current time
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(now - playStartTime_).count();
    currentTime_ = pausedTime_ + elapsed;
    
    // check if reached video end
    if (currentTime_ >= duration_) {
        Stop();
        return;
    }
    
    // try to get new frame from buffer
    VideoFrame newFrame;
    if (frameBuffer_ && frameBuffer_->pop(newFrame)) {
        std::lock_guard<std::mutex> lock(frameMutex_);
        currentFrame_ = newFrame;
    }
}

void VideoPlayer::DecodingThread() {
    std::cout << "VideoPlayer::DecodingThread - Started" << std::endl;
    
    double frameTime = 1.0 / frameRate_;
    double nextFrameTime = 0.0;
    
    while (!shouldStop_ && isPlaying_) {
        // check if need to decode new frame
        if (currentTime_ >= nextFrameTime) {
            // use FFmpeg to decode real video frame
            VideoFrame frame(width_, height_);
            frame.timestamp = currentTime_;
            
            if (DecodeFrame(frame)) {
                // add frame to buffer
                if (frameBuffer_) {
                    frameBuffer_->push(frame);
                }
            } else {
                // if decode failed, generate test pattern as backup
                GenerateTestPattern(frame);
                if (frameBuffer_) {
                    frameBuffer_->push(frame);
                }
            }
            
            nextFrameTime += frameTime;
        }
        
        // sleep for a short time to avoid overusing CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::cout << "VideoPlayer::DecodingThread - Ended" << std::endl;
}

void VideoPlayer::Cleanup() {
    Stop();
    
    std::lock_guard<std::mutex> lock(frameMutex_);
    currentFrame_.isValid = false;
    
    std::cout << "VideoPlayer::Cleanup - Cleaned up" << std::endl;
}

bool VideoPlayer::InitializeFFmpeg() {
    if (ffmpegInitialized_) {
        return true;
    }
    
    // FFmpeg 4.0+ does not need explicit initialization
    ffmpegInitialized_ = true;
    
    std::cout << "VideoPlayer::InitializeFFmpeg - FFmpeg initialized" << std::endl;
    return true;
}

void VideoPlayer::CleanupFFmpeg() {
    // clean up SWS context
    if (swsContext_) {
        sws_freeContext(swsContext_);
        swsContext_ = nullptr;
    }
    
    // clean up frames
    if (frame_) {
        av_frame_free(&frame_);
    }
    if (frameRGB_) {
        av_frame_free(&frameRGB_);
    }
    
    // clean up packet
    if (packet_) {
        av_packet_free(&packet_);
    }
    
    // clean up codec context
    if (codecContext_) {
        avcodec_free_context(&codecContext_);
    }
    
    // close format context
    if (formatContext_) {
        avformat_close_input(&formatContext_);
    }
    
    ffmpegInitialized_ = false;
    
    std::cout << "VideoPlayer::CleanupFFmpeg - FFmpeg cleaned up" << std::endl;
}

bool VideoPlayer::DecodeFrame(VideoFrame& frame) {
    if (!formatContext_ || !codecContext_ || !frame_ || !frameRGB_) {
        return false;
    }
    
    int ret = 0;
    
    // read packet
    while ((ret = av_read_frame(formatContext_, packet_)) >= 0) {
        if (packet_->stream_index == videoStreamIndex_) {
            // send packet to decoder
            ret = avcodec_send_packet(codecContext_, packet_);
            if (ret < 0) {
                av_packet_unref(packet_);
                continue;
            }
            
            // receive frame
            ret = avcodec_receive_frame(codecContext_, frame_);
            if (ret == 0) {
                // convert pixel format
                sws_scale(swsContext_, frame_->data, frame_->linesize, 0, height_,
                         frameRGB_->data, frameRGB_->linesize);
                
                // copy RGB data to VideoFrame
                frame.data.resize(width_ * height_ * 3);
                memcpy(frame.data.data(), frameRGB_->data[0], width_ * height_ * 3);
                frame.isValid = true;
                
                av_packet_unref(packet_);
                return true;
            }
        }
        av_packet_unref(packet_);
    }
    
    return false;
}

void VideoPlayer::GenerateTestPattern(VideoFrame& frame) {
    // generate simple test pattern as backup
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            int index = (y * width_ + x) * 3;
            
            // create colored stripe pattern
            float r = (sin(currentTime_ * 2.0 + x * 0.01) + 1.0) * 0.5;
            float g = (sin(currentTime_ * 1.5 + y * 0.01) + 1.0) * 0.5;
            float b = (sin(currentTime_ * 3.0 + (x + y) * 0.005) + 1.0) * 0.5;
            
            frame.data[index] = static_cast<uint8_t>(r * 255);
            frame.data[index + 1] = static_cast<uint8_t>(g * 255);
            frame.data[index + 2] = static_cast<uint8_t>(b * 255);
        }
    }
    frame.isValid = true;
}

} // namespace te

}

