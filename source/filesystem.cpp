#include "filesystem.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

std::filesystem::path FileSystem::executableDirectory()
{
    static const std::filesystem::path directory = []() -> std::filesystem::path {
#ifdef _WIN32
        constexpr DWORD kMaxPath = 32768;
        wchar_t buffer[kMaxPath];
        const DWORD length = GetModuleFileNameW(nullptr, buffer, kMaxPath);
        if (length > 0 && length < kMaxPath)
        {
            return std::filesystem::path(buffer).parent_path();
        }
#else
        std::error_code error;
        const std::filesystem::path executable = std::filesystem::read_symlink("/proc/self/exe", error);
        if (!error && !executable.empty())
        {
            return executable.parent_path();
        }
#endif
        return std::filesystem::current_path();
    }();
    return directory;
}

std::string FileSystem::getPath(const std::string& path)
{
    if (path.empty())
    {
        return path;
    }

    const std::filesystem::path input(path);
    if (input.is_absolute())
    {
        return input.lexically_normal().string();
    }

    return (executableDirectory() / input).lexically_normal().string();
}
