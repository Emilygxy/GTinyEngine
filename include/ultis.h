#pragma once
#include "root_dir.h"
#include<string>
#include <vector>

// use CMake defined micro path
const std::string g_vk_shader_resource_path = VK_SHADER_RESOURCE_PATH;
const std::string g_resource_path = RESOURCE_PATH;

// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const* path);

// loads a cubemap texture from 6 individual texture faces
// order:
// +X (right)
// -X (left)
// +Y (top)
// -Y (bottom)
// +Z (front) 
// -Z (back)
// -------------------------------------------------------
unsigned int loadCubemap(const std::vector<std::string>& faces);




