#include "video/VideoGeometry.h"
#include <iostream>

namespace te {

VideoGeometry::VideoGeometry(float width, float height) 
    : width_(width), height_(height), aspectRatio_(width / height) {
    
    CreatePlane();
    
    std::cout << "VideoGeometry created with size: " << width_ << "x" << height_ << std::endl;
}

VideoGeometry::~VideoGeometry() {
    std::cout << "VideoGeometry destroyed" << std::endl;
}

void VideoGeometry::SetVideoMaterial(std::shared_ptr<VideoMaterial> material) {
    mpVideoMaterial = material;
    SetMaterial(material); // call base class method to set material
    
    std::cout << "VideoGeometry: Video material set" << std::endl;
}

std::shared_ptr<VideoMaterial> VideoGeometry::GetVideoMaterial() const {
    return mpVideoMaterial;
}

void VideoGeometry::SetSize(float width, float height) {
    width_ = width;
    height_ = height;
    aspectRatio_ = width / height;
    
    CreatePlane();
    
    std::cout << "VideoGeometry: Size updated to " << width_ << "x" << height_ << std::endl;
}

void VideoGeometry::SetAspectRatio(float aspectRatio) {
    aspectRatio_ = aspectRatio;
    height_ = width_ / aspectRatio_;
    
    CreatePlane();
    
    std::cout << "VideoGeometry: Aspect ratio updated to " << aspectRatio_ << std::endl;
}

void VideoGeometry::UpdateGeometry() {
    CreatePlane();
    std::cout << "VideoGeometry: Geometry updated" << std::endl;
}

void VideoGeometry::CreatePlane() {
    mVertices.clear();
    mIndices.clear();
    
    // create plane vertices (centered at origin)
    float halfWidth = width_ * 0.5f;
    float halfHeight = height_ * 0.5f;
    
    // vertex data
    mVertices = {
        // position              normal              UV coordinates
        {{-halfWidth, -halfHeight, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // 左下
        {{ halfWidth, -halfHeight, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // 右下
        {{ halfWidth,  halfHeight, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // 右上
        {{-halfWidth,  halfHeight, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}  // 左上
    };
    
    // index data (two triangles)
    mIndices = {
        0, 1, 2,  // first triangle
        2, 3, 0   // second triangle
    };
    
    mbHasUV = true;
    
    // reset mesh
    SetupMesh();
    
    std::cout << "VideoGeometry::CreatePlane - Created plane with " 
              << mVertices.size() << " vertices and " 
              << mIndices.size() << " indices" << std::endl;
}

} // namespace te

