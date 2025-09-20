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

        // 3. process line directives (for debugging)
        if (mConfig.enableLineDirectives)
        {
            mLastProcessedContent = ProcessLineDirectives(mLastProcessedContent, basePath);
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
        std::string result = content;

        // process macro definitions
        std::regex defineRegex(R"(#define\s+(\w+)(?:\(([^)]*)\))?\s*(.*))");
        std::smatch match;

        while (std::regex_search(result, match, defineRegex))
        {
            std::string macroName = match[1].str();
            std::string parameters = match[2].str();
            std::string macroValue = match[3].str();

            bool isFunction = !parameters.empty();
            DefineMacro(macroName, macroValue, isFunction);

            // remove the #define line
            result = std::regex_replace(result, defineRegex, "", std::regex_constants::format_first_only);
        }

        // expand macros
        for (const auto& [name, macro] : mMacros)
        {
            result = ExpandMacro(macro, result);
        }

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

    std::string ShaderPreprocessor::ExpandMacro(const ShaderMacro& macro, const std::string& content)
    {
        if (macro.isFunction)
        {
            return ExpandFunctionMacro(macro, content);
        }
        else
        {
            return ExpandSimpleMacro(macro, content);
        }
    }

    std::string ShaderPreprocessor::ExpandFunctionMacro(const ShaderMacro& macro, const std::string& content)
    {
        // simple function macro expansion implementation
        std::string pattern = macro.name + "\\s*\\(";
        std::regex macroRegex(pattern);
        std::smatch match;

        std::string result = content;
        while (std::regex_search(result, match, macroRegex))
        {
            // find the matching position
            size_t pos = match.position();
            size_t start = pos + match.length() - 1; // skip the function name and left parenthesis

            // find the parameter list
            int parenCount = 1;
            size_t end = start;
            while (end < result.length() && parenCount > 0)
            {
                end++;
                if (result[end] == '(') parenCount++;
                else if (result[end] == ')') parenCount--;
            }

            if (parenCount == 0)
            {
                std::string args = result.substr(start + 1, end - start - 1);
                std::vector<std::string> arguments = ParseMacroArguments(args);
                
                // simple parameter replacement
                std::string expanded = macro.value;
                for (size_t i = 0; i < arguments.size(); ++i)
                {
                    std::string paramName = "\\$" + std::to_string(i + 1);
                    std::regex paramRegex(paramName);
                    expanded = std::regex_replace(expanded, paramRegex, arguments[i]);
                }

                result.replace(pos, end - pos + 1, expanded);
            }
            else
            {
                break; // syntax error, stop processing
            }
        }

        return result;
    }

    std::string ShaderPreprocessor::ExpandSimpleMacro(const ShaderMacro& macro, const std::string& content)
    {
        // simple macro expansion, avoid expanding in strings and comments
        std::string result = content;
        std::string pattern = "\\b" + macro.name + "\\b";
        std::regex macroRegex(pattern);
        
        result = std::regex_replace(result, macroRegex, macro.value);
        return result;
    }

    std::vector<std::string> ShaderPreprocessor::ParseMacroArguments(const std::string& args)
    {
        std::vector<std::string> arguments;
        std::string current;
        int parenCount = 0;
        bool inString = false;
        char stringChar = 0;

        for (char c : args)
        {
            if (!inString && (c == '"' || c == '\''))
            {
                inString = true;
                stringChar = c;
                current += c;
            }
            else if (inString && c == stringChar)
            {
                inString = false;
                current += c;
            }
            else if (!inString && c == '(')
            {
                parenCount++;
                current += c;
            }
            else if (!inString && c == ')')
            {
                parenCount--;
                current += c;
            }
            else if (!inString && c == ',' && parenCount == 0)
            {
                arguments.push_back(current);
                current.clear();
            }
            else
            {
                current += c;
            }
        }

        if (!current.empty())
        {
            arguments.push_back(current);
        }

        return arguments;
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
