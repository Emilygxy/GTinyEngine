#include "video/VideoMaterial.h"
#include "Camera.h"
#include "Light.h"
#include <iostream>

namespace te {

VideoMaterial::VideoMaterial(const std::string& vs_path, const std::string& fs_path)
    : MaterialBase(vs_path, fs_path) {
    
    // create video texture
    mpVideoTexture = std::make_shared<VideoTexture>();
    
    // record start time
    startTime_ = std::chrono::high_resolution_clock::now();
    
    std::cout << "VideoMaterial created with shaders: " << vs_path << ", " << fs_path << std::endl;
}

VideoMaterial::~VideoMaterial() {
    std::cout << "VideoMaterial destroyed" << std::endl;
}

void VideoMaterial::SetVideoPlayer(std::shared_ptr<VideoPlayer> player) {
    mpVideoPlayer = player;
    
    if (mpVideoTexture && mpVideoPlayer) {
        mpVideoTexture->SetVideoPlayer(mpVideoPlayer);
    }
    
    std::cout << "VideoMaterial: Video player set" << std::endl;
}

void VideoMaterial::UpdateFromPlayer() {
    if (mpVideoPlayer && mpVideoTexture) {
        mpVideoPlayer->Update();
        mpVideoTexture->UpdateFromPlayer();
    }
}

void VideoMaterial::OnBind() {
    if (!mpShader) {
        std::cout << "VideoMaterial::OnBind() - No shader available!" << std::endl;
        return;
    }
    
    if (mpVideoTexture && mpVideoTexture->IsValid()) {
        // bind video texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mpVideoTexture->GetHandle());
        
        std::cout << "VideoMaterial::OnBind() - Video texture bound to GL_TEXTURE0" << std::endl;
    } else {
        std::cout << "VideoMaterial::OnBind() - No valid video texture to bind" << std::endl;
    }
}

void VideoMaterial::UpdateUniform() {
    if (!mpShader) {
        std::cout << "VideoMaterial::UpdateUniform() - No shader available!" << std::endl;
        return;
    }
    
    // set video texture uniform
    mpShader->setInt("u_videoTexture", 0);
    
    // set camera parameters
    if (auto pCamera = mpAttachedCamera.lock()) {
        mpShader->setMat4("view", pCamera->GetViewMatrix());
        mpShader->setMat4("projection", pCamera->GetProjectionMatrix());
    }
    
    // set video information
    if (mpVideoPlayer) {
        mpShader->setFloat("u_videoWidth", static_cast<float>(mpVideoPlayer->GetWidth()));
        mpShader->setFloat("u_videoHeight", static_cast<float>(mpVideoPlayer->GetHeight()));
        mpShader->setFloat("u_currentTime", static_cast<float>(mpVideoPlayer->GetCurrentTime()));
        mpShader->setFloat("u_duration", static_cast<float>(mpVideoPlayer->GetDuration()));
        mpShader->setFloat("u_frameRate", static_cast<float>(mpVideoPlayer->GetFrameRate()));
    }
    
    // set time (for animation effect)
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float>(currentTime - startTime_).count();
    mpShader->setFloat("u_time", time);
    
    // set video playing state
    bool isPlaying = mpVideoPlayer ? mpVideoPlayer->IsPlaying() : false;
    mpShader->setFloat("u_isPlaying", isPlaying ? 1.0f : 0.0f);
    
    std::cout << "VideoMaterial::UpdateUniform() - Uniforms updated" << std::endl;
}

void VideoMaterial::OnPerFrameUpdate() {
    // update video texture per frame
    UpdateFromPlayer();
}

} // namespace te

