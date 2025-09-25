#pragma once
#include "textures/Texture.h"
#include "video/VideoPlayer.h"
#include <memory>
#include <mutex>

namespace te {

class VideoTexture : public Texture2D {
public:
    VideoTexture();
    ~VideoTexture();
    
    // set video player
    void SetVideoPlayer(std::shared_ptr<VideoPlayer> player);
    
    // update texture from video player
    void UpdateFromPlayer();
    
    // override base class method
    void Destroy() override;
    void ParseData() override;
    
    // check if texture is valid
    bool IsValid() const override;
    
    // get current frame information
    int GetCurrentWidth() const { return currentWidth_; }
    int GetCurrentHeight() const { return currentHeight_; }

private:
    void UpdateTextureData(const VideoFrame& frame);
    void CreateTexture(int width, int height);
    
    std::shared_ptr<VideoPlayer> mpVideoPlayer;
    std::mutex updateMutex_;
    
    int currentWidth_;
    int currentHeight_;
    bool textureCreated_;
};

} // namespace te




