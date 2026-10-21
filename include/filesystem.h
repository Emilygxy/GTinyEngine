#pragma once

#include <filesystem>
#include <string>

class FileSystem
{
public:
    // Relative paths are resolved from the directory that contains the running executable.
    // Callers keep writing "resources/...", which lands in <exeDir>/resources/...
    static std::string getPath(const std::string& path);
    static std::filesystem::path executableDirectory();
};
