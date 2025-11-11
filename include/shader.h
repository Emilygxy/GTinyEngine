#ifndef SHADER_HPP
#define SHADER_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "shader/ShaderPreprocessor.h"

GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path);

class Shader
{
public:
	Shader() = delete;
	~Shader();

	// constructor generates the shader on the fly
	// ------------------------------------------------------------------------
	Shader(const char* vertexPath, const char* fragmentPath);
	
	// constructor with preprocessor support
	Shader(const char* vertexPath, const char* fragmentPath, const te::ShaderPreprocessorConfig& config);
	
	// constructor with custom preprocessor
	Shader(const char* vertexPath, const char* fragmentPath, te::ShaderPreprocessor& preprocessor);

	void use();

	void setBool(const std::string& name, bool value) const;

	void setInt(const std::string& name, int value) const;

	void setFloat(const std::string& name, float value) const;

	void setVec2(const std::string& name, const glm::vec2& value) const;

	void setVec3(const std::string& name, const glm::vec3& value) const;

	void setVec3(const std::string& name, float x, float y, float z) const;

	void setVec4(const std::string& name, const glm::vec4& value) const;

	void setVec4(const std::string& name, float x, float y, float z, float w) const;

	void setMat2(const std::string& name, const glm::mat2& mat) const;

	void setMat3(const std::string& name, const glm::mat3& mat) const;

	void setMat4(const std::string& name, const glm::mat4& mat) const;

	unsigned int GetID() const noexcept { return mId; }
	
	// check if shader is valid
	bool IsValid() const noexcept { return mId != 0; }
	
	// static method: create shader with ShaderBuilder
	static std::shared_ptr<Shader> CreateWithBuilder(const std::string& vertexPath, const std::string& fragmentPath);
	static std::shared_ptr<Shader> CreateWithBuilder(const std::string& vertexPath, const std::string& fragmentPath, 
	                                                const te::ShaderPreprocessorConfig& config);

private:
	unsigned int mId;
	te::ShaderPreprocessor mPreprocessor;  // preprocessor instance

	bool checkCompileErrors(unsigned int shader, std::string type);
	
	// internal constructor, support preprocessor
	void constructWithPreprocessor(const char* vertexPath, const char* fragmentPath, te::ShaderPreprocessor& preprocessor);
	
	// helper functions
	bool isOpenGLContextValid() const;
	unsigned int createShaderSafely(GLenum shaderType, const char* source) const;
};

#endif