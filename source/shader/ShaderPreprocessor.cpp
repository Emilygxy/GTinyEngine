#include "shader/ShaderPreprocessor.h"
#include "shader.h"
#include "filesystem.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cstdio>

namespace te
{
    // ShaderPreprocessor Implementation
    ShaderPreprocessor::ShaderPreprocessor()
    {
        InitializeBuiltinMacros();
    }

    ShaderPreprocessor::ShaderPreprocessor(const ShaderPreprocessorConfig& config)
        : mConfig(config)
    {
        InitializeBuiltinMacros();
    }

    std::string ShaderPreprocessor::ProcessShader(const std::string& shaderPath)
    {
        std::string fullPath = FileSystem::getPath(shaderPath);
        std::string content = ReadFile(fullPath);
        
        if (content.empty())
        {
            std::cout << "ERROR::SHADER_PREPROCESSOR::Failed to read shader file: " << fullPath << std::endl;
            return "";
        }

        return ProcessShaderContent(content, fullPath);
    }

    std::string ShaderPreprocessor::ProcessShaderContent(const std::string& content, const std::string& basePath)
    {
        mProcessedFiles.clear();
        mLastProcessedContent = content;

        // 1. process include files
        if (mConfig.enableIncludeProcessing)
        {
            mLastProcessedContent = ProcessIncludes(mLastProcessedContent, basePath);
        }

        // 2. process macro definitions
        if (mConfig.enableMacroExpansion)
        {
            mLastProcessedContent = ProcessMacros(mLastProcessedContent);
        }

        // Debug output (can be enabled for troubleshooting)
        #ifdef SHADER_PREPROCESSOR_DEBUG
        std::cout << "=== Processed Shader Content ===" << std::endl;
        std::cout << mLastProcessedContent << std::endl;
        std::cout << "=== End Processed Content ===" << std::endl;
        #endif

        return mLastProcessedContent;
    }

    std::string ShaderPreprocessor::ProcessIncludes(const std::string& content, const std::string& basePath, int depth)
    {
        if (!IsValidIncludeDepth(depth))
        {
            std::cout << "WARNING::SHADER_PREPROCESSOR::Maximum include depth reached" << std::endl;
            return content;
        }

        std::string result = content;
        std::regex includeRegex(R"(#include\s*[<"]([^>"]+)[>"])");
        std::smatch match;

        while (std::regex_search(result, match, includeRegex))
        {
            std::string includePath = match[1].str();
            std::string fullIncludePath = ResolveIncludePath(includePath, basePath);

            // check if the file has been processed (to prevent circular inclusion)
            if (mProcessedFiles.find(fullIncludePath) != mProcessedFiles.end())
            {
                std::cout << "WARNING::SHADER_PREPROCESSOR::Circular include detected: " << includePath << std::endl;
                result = std::regex_replace(result, includeRegex, "// Circular include: " + includePath, std::regex_constants::format_first_only);
                continue;
            }

            std::string includeContent = ReadFile(fullIncludePath);
            if (!includeContent.empty())
            {
                mProcessedFiles.insert(fullIncludePath);
                
                // recursively process the includes in the included file
                includeContent = ProcessIncludes(includeContent, fullIncludePath, depth + 1);
                
                // replace the include directive
                result = std::regex_replace(result, includeRegex, includeContent, std::regex_constants::format_first_only);
            }
            else
            {
                std::cout << "ERROR::SHADER_PREPROCESSOR::Failed to read include file: " << fullIncludePath << std::endl;
                result = std::regex_replace(result, includeRegex, "// Failed to include: " + includePath, std::regex_constants::format_first_only);
            }
        }

        return result;
    }

    std::string ShaderPreprocessor::ProcessMacros(const std::string& content)
    {
        //todo
        std::string result, targetField = "#version";

        size_t pos = content.find(targetField); // find target feid pod
        if (pos == std::string::npos) 
            return content;   // not found return

         // line head: search last change-line flag forwardly
        size_t lineStart = content.rfind('\n', pos) + 1; // +1 skip change-line flag
        if (lineStart == std::string::npos) lineStart = 0;

        // line tear: search next change-line flag backwardly
        size_t lineEnd = content.find('\n', pos);
        if (lineEnd == std::string::npos) lineEnd = content.length();

        // get substring by line end  pos

        result += content.substr(0, lineEnd);
        std::string subContent1 = content.substr(lineEnd, content.length() - lineEnd);
        std::string newField = "";
        for (auto macro: mMacros)
        {
            newField += "\n#define " + macro.second.name + macro.second.value + "\n";
        }

        result += newField;
        result += subContent1;

        return result;
    }

    std::string ShaderPreprocessor::ProcessLineDirectives(const std::string& content, const std::string& originalPath)
    {
        // For now, disable line directives as they often cause issues with OpenGL shader compilation
        // Many OpenGL drivers don't handle #line directives properly
        return content;
        
        // Alternative implementation (commented out due to compatibility issues):
        // Convert Windows path separators to forward slashes for GLSL compatibility
        // std::string normalizedPath = originalPath;
        // std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
        // 
        // // Use a simpler #line format that's more compatible with OpenGL
        // std::string result = "#line 1\n" + content;
        // return result;
    }

    std::string ShaderPreprocessor::ReadFile(const std::string& filePath)
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            std::cout << "ERROR::SHADER_PREPROCESSOR::Cannot open file: " << filePath << std::endl;
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        return buffer.str();
    }

    std::string ShaderPreprocessor::ResolveIncludePath(const std::string& includePath, const std::string& basePath)
    {
        // if it is a relative path, parse it based on the current file path
        if (includePath[0] != '/' && includePath[0] != '\\')
        {
            //std::filesystem::path baseDir = std::filesystem::path(basePath).parent_path();
            std::string include_fullPath = FileSystem::getPath(mConfig.shaderDirectory + includePath);
            return include_fullPath;
        }

        // if it is an absolute path or system path, use it directly
        if (includePath[0] == '<' && includePath.back() == '>')
        {
            // system header file, find it in the include directory
            std::string systemPath = includePath.substr(1, includePath.length() - 2);
            return FileSystem::getPath(mConfig.includeDirectory + systemPath);
        }

        return includePath;
    }

    bool ShaderPreprocessor::IsValidIncludeDepth(int depth)
    {
        const int MAX_INCLUDE_DEPTH = 32;
        return depth < MAX_INCLUDE_DEPTH;
    }

    void ShaderPreprocessor::DefineMacro(const std::string& name, const std::string& value, bool isFunction)
    {
        mMacros[name] = ShaderMacro(name, value, isFunction);
    }

    void ShaderPreprocessor::UndefineMacro(const std::string& name)
    {
        mMacros.erase(name);
    }

    void ShaderPreprocessor::ClearMacros()
    {
        mMacros.clear();
        InitializeBuiltinMacros();
    }

    bool ShaderPreprocessor::IsMacroDefined(const std::string& name) const
    {
        return mMacros.find(name) != mMacros.end();
    }

    void ShaderPreprocessor::SetConfig(const ShaderPreprocessorConfig& config)
    {
        mConfig = config;
    }

    const ShaderPreprocessorConfig& ShaderPreprocessor::GetConfig() const
    {
        return mConfig;
    }

    void ShaderPreprocessor::InitializeBuiltinMacros()
    {
        // add some builtin macro definitions
        DefineMacro("GLSL_VERSION", "330");
        DefineMacro("PI", "3.14159265359");
        DefineMacro("TWO_PI", "6.28318530718");
        DefineMacro("HALF_PI", "1.57079632679");
    }

    // ShaderBuilder Implementation
    ShaderBuilder::ShaderBuilder()
    {
    }

    ShaderBuilder::ShaderBuilder(const ShaderPreprocessorConfig& config)
        : mPreprocessor(config)
    {
    }

    std::shared_ptr<Shader> ShaderBuilder::BuildShader(const std::string& vertexPath, const std::string& fragmentPath)
    {
        std::string vertexContent = mPreprocessor.ProcessShader(vertexPath);
        std::string fragmentContent = mPreprocessor.ProcessShader(fragmentPath);

        if (vertexContent.empty() || fragmentContent.empty())
        {
            std::cout << "ERROR::SHADER_BUILDER::Failed to process shader files" << std::endl;
            return nullptr;
        }

        return BuildShaderFromContent(vertexContent, fragmentContent);
    }

    std::shared_ptr<Shader> ShaderBuilder::BuildShaderFromContent(const std::string& vertexContent, const std::string& fragmentContent)
    {
        // create a temporary file to store the processed shader content
        std::string tempVertexPath = "temp_vertex.glsl";
        std::string tempFragmentPath = "temp_fragment.glsl";

        // write to the temporary file
        std::ofstream vertexFile(tempVertexPath);
        std::ofstream fragmentFile(tempFragmentPath);
        
        if (!vertexFile.is_open() || !fragmentFile.is_open())
        {
            std::cout << "ERROR::SHADER_BUILDER::Failed to create temporary files" << std::endl;
            return nullptr;
        }

        vertexFile << vertexContent;
        fragmentFile << fragmentContent;
        
        vertexFile.close();
        fragmentFile.close();

        // create the shader
        auto shader = std::make_shared<Shader>(tempVertexPath.c_str(), tempFragmentPath.c_str());

        // clean up the temporary file
        std::remove(tempVertexPath.c_str());
        std::remove(tempFragmentPath.c_str());

        return shader;
    }

    ShaderBuilder& ShaderBuilder::Define(const std::string& name, const std::string& value, bool isFunction)
    {
        mPreprocessor.DefineMacro(name, value, isFunction);
        return *this;
    }

    ShaderBuilder& ShaderBuilder::Undefine(const std::string& name)
    {
        mPreprocessor.UndefineMacro(name);
        return *this;
    }

    ShaderBuilder& ShaderBuilder::SetConfig(const ShaderPreprocessorConfig& config)
    {
        mPreprocessor.SetConfig(config);
        return *this;
    }
}
