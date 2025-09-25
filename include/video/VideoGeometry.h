#pragma once
#include "geometry/BasicGeometry.h"
#include "video/VideoMaterial.h"
#include <memory>

namespace te {

class VideoGeometry : public BasicGeometry {
public:
    VideoGeometry(float width = 2.0f, float height = 1.5f);
    ~VideoGeometry();
    
    // set video material
    void SetVideoMaterial(std::shared_ptr<VideoMaterial> material);
    std::shared_ptr<VideoMaterial> GetVideoMaterial() const;
    
    // set plane size
    void SetSize(float width, float height);
    float GetWidth() const { return width_; }
    float GetHeight() const { return height_; }
    
    // set aspect ratio
    void SetAspectRatio(float aspectRatio);
    float GetAspectRatio() const { return aspectRatio_; }
    
    // update geometry (when video size changes)
    void UpdateGeometry();

private:
    void CreatePlane();
    
    float width_;
    float height_;
    float aspectRatio_;
    std::shared_ptr<VideoMaterial> mpVideoMaterial;
};

} // namespace te

