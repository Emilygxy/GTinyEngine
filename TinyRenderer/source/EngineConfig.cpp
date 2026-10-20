#include "EngineConfig.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
struct JsonValue
{
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Object
    };

    Type type{ Type::Null };
    bool boolean{ false };
    double number{ 0.0 };
    std::string str;
    std::map<std::string, JsonValue> object;

    const JsonValue* Get(const char* key) const
    {
        const auto it = object.find(key);
        return it == object.end() ? nullptr : &it->second;
    }

    std::string AsString(const std::string& fallback = {}) const
    {
        return type == Type::String ? str : fallback;
    }

    bool AsBool(bool fallback) const
    {
        if (type == Type::Bool)
        {
            return boolean;
        }
        if (type == Type::Number)
        {
            return number != 0.0;
        }
        if (type == Type::String)
        {
            std::string lower = str;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower == "true" || lower == "1" || lower == "yes")
            {
                return true;
            }
            if (lower == "false" || lower == "0" || lower == "no")
            {
                return false;
            }
        }
        return fallback;
    }

    int AsInt(int fallback) const
    {
        if (type == Type::Number)
        {
            return static_cast<int>(number);
        }
        if (type == Type::String)
        {
            try
            {
                return std::stoi(str);
            }
            catch (...)
            {
                return fallback;
            }
        }
        return fallback;
    }
};

class JsonParser
{
public:
    explicit JsonParser(std::string text)
        : mText(std::move(text))
    {
    }

    bool Parse(JsonValue& out, std::string& error)
    {
        SkipWhitespaceAndComments();
        if (mPos >= mText.size())
        {
            error = "empty document";
            return false;
        }
        if (!ParseValue(out, error))
        {
            return false;
        }
        SkipWhitespaceAndComments();
        if (mPos < mText.size())
        {
            error = "trailing characters after JSON value";
            return false;
        }
        return true;
    }

private:
    std::string mText;
    size_t mPos{ 0 };

    bool Peek(char& ch) const
    {
        if (mPos >= mText.size())
        {
            return false;
        }
        ch = mText[mPos];
        return true;
    }

    void Advance()
    {
        if (mPos < mText.size())
        {
            ++mPos;
        }
    }

    void SkipWhitespaceAndComments()
    {
        while (mPos < mText.size())
        {
            const char ch = mText[mPos];
            if (std::isspace(static_cast<unsigned char>(ch)))
            {
                Advance();
                continue;
            }
            if (ch == '/' && mPos + 1 < mText.size())
            {
                if (mText[mPos + 1] == '/')
                {
                    mPos += 2;
                    while (mPos < mText.size() && mText[mPos] != '\n')
                    {
                        Advance();
                    }
                    continue;
                }
                if (mText[mPos + 1] == '*')
                {
                    mPos += 2;
                    while (mPos + 1 < mText.size() && !(mText[mPos] == '*' && mText[mPos + 1] == '/'))
                    {
                        Advance();
                    }
                    if (mPos + 1 < mText.size())
                    {
                        mPos += 2;
                    }
                    continue;
                }
            }
            break;
        }
    }

    bool ParseValue(JsonValue& out, std::string& error)
    {
        SkipWhitespaceAndComments();
        char ch = 0;
        if (!Peek(ch))
        {
            error = "unexpected end of file";
            return false;
        }
        if (ch == '{')
        {
            return ParseObject(out, error);
        }
        if (ch == '"')
        {
            out.type = JsonValue::Type::String;
            return ParseString(out.str, error);
        }
        if (ch == 't' || ch == 'f')
        {
            return ParseLiteral(out, error);
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)))
        {
            return ParseNumber(out, error);
        }
        error = "unsupported JSON value";
        return false;
    }

    bool ParseObject(JsonValue& out, std::string& error)
    {
        out.type = JsonValue::Type::Object;
        Advance(); // '{'
        SkipWhitespaceAndComments();
        char ch = 0;
        if (Peek(ch) && ch == '}')
        {
            Advance();
            return true;
        }
        while (true)
        {
            SkipWhitespaceAndComments();
            std::string key;
            if (!ParseString(key, error))
            {
                return false;
            }
            SkipWhitespaceAndComments();
            if (!Peek(ch) || ch != ':')
            {
                error = "expected ':' after object key";
                return false;
            }
            Advance();
            JsonValue value;
            if (!ParseValue(value, error))
            {
                return false;
            }
            out.object.emplace(std::move(key), std::move(value));
            SkipWhitespaceAndComments();
            if (!Peek(ch))
            {
                error = "unterminated object";
                return false;
            }
            if (ch == ',')
            {
                Advance();
                continue;
            }
            if (ch == '}')
            {
                Advance();
                return true;
            }
            error = "expected ',' or '}' in object";
            return false;
        }
    }

    bool ParseString(std::string& out, std::string& error)
    {
        char ch = 0;
        SkipWhitespaceAndComments();
        if (!Peek(ch) || ch != '"')
        {
            error = "expected string";
            return false;
        }
        Advance();
        out.clear();
        while (Peek(ch))
        {
            Advance();
            if (ch == '"')
            {
                return true;
            }
            if (ch == '\\')
            {
                if (!Peek(ch))
                {
                    error = "unterminated string escape";
                    return false;
                }
                Advance();
                switch (ch)
                {
                case '"':
                case '\\':
                case '/':
                    out.push_back(ch);
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                default:
                    out.push_back(ch);
                    break;
                }
                continue;
            }
            out.push_back(ch);
        }
        error = "unterminated string";
        return false;
    }

    bool ParseLiteral(JsonValue& out, std::string& error)
    {
        if (mText.compare(mPos, 4, "true") == 0)
        {
            mPos += 4;
            out.type = JsonValue::Type::Bool;
            out.boolean = true;
            return true;
        }
        if (mText.compare(mPos, 5, "false") == 0)
        {
            mPos += 5;
            out.type = JsonValue::Type::Bool;
            out.boolean = false;
            return true;
        }
        error = "invalid literal";
        return false;
    }

    bool ParseNumber(JsonValue& out, std::string& error)
    {
        const size_t begin = mPos;
        if (mText[mPos] == '-')
        {
            Advance();
        }
        while (mPos < mText.size() && std::isdigit(static_cast<unsigned char>(mText[mPos])))
        {
            Advance();
        }
        if (mPos < mText.size() && mText[mPos] == '.')
        {
            Advance();
            while (mPos < mText.size() && std::isdigit(static_cast<unsigned char>(mText[mPos])))
            {
                Advance();
            }
        }
        try
        {
            out.type = JsonValue::Type::Number;
            out.number = std::stod(mText.substr(begin, mPos - begin));
            return true;
        }
        catch (...)
        {
            error = "invalid number";
            return false;
        }
    }
};

std::string ToLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::filesystem::path ExecutableDirectory()
{
#ifdef _WIN32
    char buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(buffer).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

std::string ReadTextFile(const std::string& path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file)
    {
        return {};
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    std::string text = oss.str();
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF)
    {
        text.erase(0, 3);
    }
    return text;
}

void ApplyObject(const JsonValue& root, EngineConfig& config)
{
    if (const JsonValue* backend = root.Get("backend"))
    {
        RendererBackend parsed = config.backend;
        if (EngineConfig::ParseBackend(backend->AsString(), parsed))
        {
            config.backend = parsed;
        }
    }

    const JsonValue* renderer = root.Get("renderer");
    if (renderer && renderer->type == JsonValue::Type::Object)
    {
        if (const JsonValue* backend = renderer->Get("backend"))
        {
            RendererBackend parsed = config.backend;
            if (EngineConfig::ParseBackend(backend->AsString(), parsed))
            {
                config.backend = parsed;
            }
        }
        if (const JsonValue* vsync = renderer->Get("vsync"))
        {
            config.vsync = vsync->AsBool(config.vsync);
        }
        if (const JsonValue* multithreaded = renderer->Get("multithreaded"))
        {
            config.multithreaded = multithreaded->AsBool(config.multithreaded);
        }
    }

    const JsonValue* window = root.Get("window");
    if (window && window->type == JsonValue::Type::Object)
    {
        if (const JsonValue* width = window->Get("width"))
        {
            config.windowWidth = (std::max)(1, width->AsInt(config.windowWidth));
        }
        if (const JsonValue* height = window->Get("height"))
        {
            config.windowHeight = (std::max)(1, height->AsInt(config.windowHeight));
        }
        if (const JsonValue* title = window->Get("title"))
        {
            const std::string value = title->AsString();
            if (!value.empty())
            {
                config.windowTitle = value;
            }
        }
        if (const JsonValue* fullscreen = window->Get("fullscreen"))
        {
            config.fullscreen = fullscreen->AsBool(config.fullscreen);
        }
    }
}

void NormalizeConfig(EngineConfig& config)
{
    if (config.backend == RendererBackend::OpenGLES)
    {
        std::cout << "OpenGLES backend is not implemented; falling back to OpenGL." << std::endl;
        config.backend = RendererBackend::OpenGL;
    }
}

bool EqualsIgnoreCase(const std::string& lhs, const char* rhs)
{
    return ToLower(lhs) == ToLower(rhs);
}

} // namespace

const char* EngineConfig::BackendToString(RendererBackend backend)
{
    switch (backend)
    {
    case RendererBackend::OpenGL:
        return "OpenGL";
    case RendererBackend::OpenGLES:
        return "OpenGLES";
    case RendererBackend::Vulkan:
        return "Vulkan";
    default:
        return "Unknown";
    }
}

bool EngineConfig::ParseBackend(const std::string& text, RendererBackend& out)
{
    const std::string lower = ToLower(text);
    if (lower == "opengl" || lower == "gl" || lower == "ogl")
    {
        out = RendererBackend::OpenGL;
        return true;
    }
    if (lower == "opengles" || lower == "gles" || lower == "es")
    {
        out = RendererBackend::OpenGLES;
        return true;
    }
    if (lower == "vulkan" || lower == "vk")
    {
        out = RendererBackend::Vulkan;
        return true;
    }
    return false;
}

std::vector<std::string> EngineConfigLoader::CollectSearchPaths(const std::string& extraPath)
{
    std::vector<std::string> paths;
    auto addPath = [&paths](std::filesystem::path path) {
        if (path.empty())
        {
            return;
        }
        std::error_code ec;
        const std::filesystem::path normalized = std::filesystem::weakly_canonical(path, ec);
        const std::string value = (ec ? path : normalized).string();
        if (std::find(paths.begin(), paths.end(), value) == paths.end())
        {
            paths.push_back(value);
        }
    };

    if (!extraPath.empty())
    {
        addPath(extraPath);
    }

    addPath(std::filesystem::current_path() / "EngineConfig.json");
    addPath(std::filesystem::current_path() / "TinyRenderer" / "EngineConfig.json");
#ifdef ENGINE_CONFIG_SOURCE_PATH
    addPath(ENGINE_CONFIG_SOURCE_PATH);
#endif
    addPath(ExecutableDirectory() / "EngineConfig.json");

#ifdef RESOURCE_PATH
    addPath(std::filesystem::path(RESOURCE_PATH) / "EngineConfig.json");
    addPath(std::filesystem::path(RESOURCE_PATH).parent_path() / "EngineConfig.json");
#endif
    return paths;
}

EngineConfig EngineConfigLoader::LoadFromFile(const std::string& path)
{
    EngineConfig config;
    const std::string text = ReadTextFile(path);
    if (text.empty())
    {
        std::cout << "EngineConfig: failed to read " << path << ", using defaults." << std::endl;
        return config;
    }

    JsonParser parser(text);
    JsonValue root;
    std::string error;
    if (!parser.Parse(root, error) || root.type != JsonValue::Type::Object)
    {
        std::cout << "EngineConfig: failed to parse " << path << " (" << error << "), using defaults." << std::endl;
        return config;
    }

    ApplyObject(root, config);
    NormalizeConfig(config);
    config.loadedFrom = path;
    return config;
}

EngineConfig EngineConfigLoader::Load(int argc, char** argv)
{
    std::string configPath;
    std::string backendOverride;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i] ? argv[i] : "";
        if (EqualsIgnoreCase(arg, "--config") && i + 1 < argc)
        {
            configPath = argv[++i];
            continue;
        }
        if (EqualsIgnoreCase(arg, "--backend") && i + 1 < argc)
        {
            backendOverride = argv[++i];
            continue;
        }
        if (arg.size() >= 5 && ToLower(arg.substr(arg.size() - 5)) == ".json")
        {
            configPath = arg;
        }
    }

    EngineConfig config;
    bool loaded = false;
    for (const std::string& candidate : CollectSearchPaths(configPath))
    {
        std::error_code ec;
        if (!std::filesystem::exists(candidate, ec) || ec)
        {
            continue;
        }
        config = LoadFromFile(candidate);
        loaded = true;
        break;
    }

    if (!loaded)
    {
        std::cout << "EngineConfig.json not found; using built-in defaults ("
                  << EngineConfig::BackendToString(config.backend) << ")." << std::endl;
        config.loadedFrom = "(defaults)";
    }

    if (!backendOverride.empty())
    {
        RendererBackend parsed = config.backend;
        if (EngineConfig::ParseBackend(backendOverride, parsed))
        {
            config.backend = parsed;
            std::cout << "EngineConfig: --backend override -> "
                      << EngineConfig::BackendToString(config.backend) << std::endl;
        }
        else
        {
            std::cout << "EngineConfig: unknown --backend '" << backendOverride << "', ignored." << std::endl;
        }
    }

    NormalizeConfig(config);
    return config;
}
