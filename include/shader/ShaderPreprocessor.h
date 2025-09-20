#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <unordered_set>
#include <filesystem>

class Shader;

namespace te
{
    // shader preprocessor configuration
    struct ShaderPreprocessorConfig
    {
        std::string shaderDirectory = "resources/shaders/";  // shader root directory
        std::string includeDirectory = "resources/shaders/includes/";  // include directory
        bool enableMacroExpansion = true;  // enable macro expansion
        bool enableIncludeProcessing = true;  // enable include processing
    };

    // macro definition structure
    struct ShaderMacro
    {
        std::string name;
        std::string value;
        bool isFunction = false;  // whether it is a function macro
        
        ShaderMacro() = default;
        ShaderMacro(const std::string& n, const std::string& v, bool func = false)
            : name(n), value(v), isFunction(func) {}
    };

    // shader preprocessor
    class ShaderPreprocessor
    {
    public:
        ShaderPreprocessor();
        explicit ShaderPreprocessor(const ShaderPreprocessorConfig& config);
        ~ShaderPreprocessor() = default;

        // preprocess shader source code
        std::string ProcessShader(const std::string& shaderPath);
        std::string ProcessShaderContent(const std::string& content, const std::string& basePath = "");

        //
        std::string ProcessMacros(const std::string& content);
        //
        void DefineMacro(const std::string& name, const std::string& value, bool isFunction = false);
        void UndefineMacro(const std::string& name);
        void ClearMacros();
        bool IsMacroDefined(const std::string& name) const;

        // configuration management
        void SetConfig(const ShaderPreprocessorConfig& config);
        const ShaderPreprocessorConfig& GetConfig() const;

        // get the processed shader content (for debugging)
        const std::string& GetLastProcessedContent() const { return mLastProcessedContent; }

    private:
        // core processing function
        std::string ProcessIncludes(const std::string& content, const std::string& basePath, int depth = 0);
        std::string ProcessLineDirectives(const std::string& content, const std::string& originalPath);

        // auxiliary function
        std::string ReadFile(const std::string& filePath);
        std::string ResolveIncludePath(const std::string& includePath, const std::string& basePath);
        bool IsValidIncludeDepth(int depth);

        // member variables
        ShaderPreprocessorConfig mConfig;
        std::unordered_map<std::string, ShaderMacro> mMacros;
        std::unordered_set<std::string> mProcessedFiles;  // prevent circular inclusion
        std::string mLastProcessedContent;
        
        // builtin macro definition
        void InitializeBuiltinMacros();
    };

    // shader builder - advanced interface
    class ShaderBuilder
    {
    public:
        ShaderBuilder();
        explicit ShaderBuilder(const ShaderPreprocessorConfig& config);

        // build shader
        std::shared_ptr<Shader> BuildShader(const std::string& vertexPath, const std::string& fragmentPath);
        std::shared_ptr<Shader> BuildShaderFromContent(const std::string& vertexContent, const std::string& fragmentContent);

        // preprocessor access
        ShaderPreprocessor& GetPreprocessor() { return mPreprocessor; }
        const ShaderPreprocessor& GetPreprocessor() const { return mPreprocessor; }

        // convenient method
        ShaderBuilder& Define(const std::string& name, const std::string& value = "", bool isFunction = false);
        ShaderBuilder& Undefine(const std::string& name);
        ShaderBuilder& SetConfig(const ShaderPreprocessorConfig& config);

    private:
        ShaderPreprocessor mPreprocessor;
    };
}
