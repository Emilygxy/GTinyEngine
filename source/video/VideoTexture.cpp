#include "video/VideoTexture.h"
#include "glad/glad.h"
#include <iostream>

namespace te {

VideoTexture::VideoTexture() 
    : currentWidth_(0), currentHeight_(0), textureCreated_(false) {
    
    std::cout << "VideoTexture created" << std::endl;
}

VideoTexture::~VideoTexture() {
    Destroy();
}

void VideoTexture::SetVideoPlayer(std::shared_ptr<VideoPlayer> player) {
    std::lock_guard<std::mutex> lock(updateMutex_);
    
    mpVideoPlayer = player;
    
    if (mpVideoPlayer) {
        currentWidth_ = mpVideoPlayer->GetWidth();
        currentHeight_ = mpVideoPlayer->GetHeight();
        
        // create initial texture
        CreateTexture(currentWidth_, currentHeight_);
        
        std::cout << "VideoTexture::SetVideoPlayer - Video player set, size: " 
                  << currentWidth_ << "x" << currentHeight_ << std::endl;
    }
}

void VideoTexture::UpdateFromPlayer() {
    if (!mpVideoPlayer) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(updateMutex_);
    
    // get current frame
    VideoFrame frame;
    if (mpVideoPlayer->GetCurrentFrame(frame)) {
        UpdateTextureData(frame);
    }
}

void VideoTexture::Destroy() {
    std::lock_guard<std::mutex> lock(updateMutex_);
    
    if (mHandle != kInvalidHandle) {
        glDeleteTextures(1, &mHandle);
        mHandle = kInvalidHandle;
    }
    
    textureCreated_ = false;
    currentWidth_ = 0;
    currentHeight_ = 0;
    
    std::cout << "VideoTexture::Destroy - Texture destroyed" << std::endl;
}

void VideoTexture::ParseData() {
    // VideoTexture does not use file path, but gets data from VideoPlayer
    // this method is kept empty, actual texture creation is done in CreateTexture
}

bool VideoTexture::IsValid() const {
    return mHandle != kInvalidHandle && textureCreated_;
}

void VideoTexture::UpdateTextureData(const VideoFrame& frame) {
    if (!frame.isValid || frame.data.empty()) {
        return;
    }
    
    // check if size changed
    if (frame.width != currentWidth_ || frame.height != currentHeight_) {
        currentWidth_ = frame.width;
        currentHeight_ = frame.height;
        CreateTexture(currentWidth_, currentHeight_);
    }
    
    if (mHandle == kInvalidHandle) {
        return;
    }
    
    // update texture data
    glBindTexture(GL_TEXTURE_2D, mHandle);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                    currentWidth_, currentHeight_, 
                    GL_RGB, GL_UNSIGNED_BYTE, frame.data.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void VideoTexture::CreateTexture(int width, int height) {
    // if there is already a texture, delete it
    if (mHandle != kInvalidHandle) {
        glDeleteTextures(1, &mHandle);
    }
    
    // create new texture
    glGenTextures(1, &mHandle);
    glBindTexture(GL_TEXTURE_2D, mHandle);
    
    // set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // allocate texture storage space
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, 
                 GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    textureCreated_ = true;
    
    std::cout << "VideoTexture::CreateTexture - Created texture " << mHandle 
              << " with size " << width << "x" << height << std::endl;
}

} // namespace te

