#pragma once
#include "filesystem.h"
#include<string>
#include <vector>

// Runtime resources live next to the executable, under resources/.
const std::string g_vk_shader_resource_path = FileSystem::getPath("resources/compiled_shaders");
const std::string g_resource_path = FileSystem::getPath("resources");

// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const* path, bool generateMipMap = true);

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




