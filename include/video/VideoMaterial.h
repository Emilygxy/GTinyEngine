#pragma once
#include "materials/BaseMaterial.h"
#include "video/VideoPlayer.h"
#include "video/VideoTexture.h"
#include <memory>
#include <chrono>

namespace te {

class VideoMaterial : public MaterialBase {
public:
    VideoMaterial(const std::string& vs_path = "resources/shaders/video/video.vs", 
                  const std::string& fs_path = "resources/shaders/video/video.fs");
    ~VideoMaterial();
    
    // set video player
    void SetVideoPlayer(std::shared_ptr<VideoPlayer> player);
    std::shared_ptr<VideoPlayer> GetVideoPlayer() const { return mpVideoPlayer; }
    
    // update video texture
    void UpdateFromPlayer();
    
    // override base class method
    void OnBind() override;
    void UpdateUniform() override;
    void OnPerFrameUpdate() override;
    
    // get video texture
    std::shared_ptr<VideoTexture> GetVideoTexture() const { return mpVideoTexture; }

private:
    std::shared_ptr<VideoPlayer> mpVideoPlayer;
    std::shared_ptr<VideoTexture> mpVideoTexture;
    
    // time management
    std::chrono::high_resolution_clock::time_point startTime_;
};

} // namespace te

