#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
using namespace std;

#include <stdlib.h>
#include <string.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "shader.h"
#include "shader/ShaderPreprocessor.h"

GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path){

	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code from the file
	std::string VertexShaderCode;
	std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
	if(VertexShaderStream.is_open()){
		std::stringstream sstr;
		sstr << VertexShaderStream.rdbuf();
		VertexShaderCode = sstr.str();
		VertexShaderStream.close();
	}else{
		printf("Impossible to open %s. Are you in the right directory ? Don't forget to read the FAQ !\n", vertex_file_path);
		getchar();
		return 0;
	}

	// Read the Fragment Shader code from the file
	std::string FragmentShaderCode;
	std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
	if(FragmentShaderStream.is_open()){
		std::stringstream sstr;
		sstr << FragmentShaderStream.rdbuf();
		FragmentShaderCode = sstr.str();
		FragmentShaderStream.close();
	}

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile Vertex Shader
	printf("Compiling shader : %s\n", vertex_file_path);
	char const * VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
	glCompileShader(VertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if ( InfoLogLength > 0 ){
		std::vector<char> VertexShaderErrorMessage(InfoLogLength+1);
		glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
		printf("%s\n", &VertexShaderErrorMessage[0]);
	}

	// Compile Fragment Shader
	printf("Compiling shader : %s\n", fragment_file_path);
	char const * FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
	glCompileShader(FragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if ( InfoLogLength > 0 ){
		std::vector<char> FragmentShaderErrorMessage(InfoLogLength+1);
		glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
		printf("%s\n", &FragmentShaderErrorMessage[0]);
	}

	// Link the program
	printf("Linking program\n");
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if ( InfoLogLength > 0 ){
		std::vector<char> ProgramErrorMessage(InfoLogLength+1);
		glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
		printf("%s\n", &ProgramErrorMessage[0]);
	}

	
	glDetachShader(ProgramID, VertexShaderID);
	glDetachShader(ProgramID, FragmentShaderID);
	
	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	return ProgramID;
}

Shader::Shader(const char* vertexPath, const char* fragmentPath)
	: mId(0)
{
	// use the default configuration of the preprocessor
	te::ShaderPreprocessorConfig defaultConfig;
	constructWithPreprocessor(vertexPath, fragmentPath, mPreprocessor);
}

Shader::Shader(const char* vertexPath, const char* fragmentPath, const te::ShaderPreprocessorConfig& config)
	: mId(0), mPreprocessor(config)
{
	constructWithPreprocessor(vertexPath, fragmentPath, mPreprocessor);
}

Shader::Shader(const char* vertexPath, const char* fragmentPath, te::ShaderPreprocessor& preprocessor)
	: mId(0)
{
	constructWithPreprocessor(vertexPath, fragmentPath, preprocessor);
}

void Shader::constructWithPreprocessor(const char* vertexPath, const char* fragmentPath, te::ShaderPreprocessor& preprocessor)
{
	// 1. use the preprocessor to process the shader source code
	std::string vertexCode = preprocessor.ProcessShader(vertexPath);
	std::string fragmentCode = preprocessor.ProcessShader(fragmentPath);

	if (vertexCode.empty() || fragmentCode.empty())
	{
		std::cout << "ERROR::SHADER::PREPROCESSOR_FAILED" << std::endl;
		std::cout << "Vertex Shader: " << vertexPath << std::endl;
		std::cout << "Fragment Shader: " << fragmentPath << std::endl;
		mId = 0; // Ensure mId is 0 on failure
		return;
	}

	const char* vShaderCode = vertexCode.c_str();
	const char* fShaderCode = fragmentCode.c_str();

	// 2. compile shaders
	unsigned int vertex = 0, fragment = 0;
	bool compilationSuccess = true;

	// vertex shader
	vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, &vShaderCode, NULL);
	glCompileShader(vertex);
	if (!checkCompileErrors(vertex, "VERTEX"))
	{
		compilationSuccess = false;
	}

	// fragment Shader
	fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &fShaderCode, NULL);
	glCompileShader(fragment);
	if (!checkCompileErrors(fragment, "FRAGMENT"))
	{
		compilationSuccess = false;
	}

	// Only create program if compilation was successful
	if (compilationSuccess)
	{
		// shader Program
		mId = glCreateProgram();
		if (mId != 0)
		{
			glAttachShader(mId, vertex);
			glAttachShader(mId, fragment);
			glLinkProgram(mId);
			if (!checkCompileErrors(mId, "PROGRAM"))
			{
				// Link failed, clean up program
				glDeleteProgram(mId);
				mId = 0;
			}
		}
	}
	else
	{
		// Compilation failed, ensure mId is 0
		mId = 0;
	}

	// delete the shaders as they're linked into our program now and no longer necessary
	if (vertex != 0) glDeleteShader(vertex);
	if (fragment != 0) glDeleteShader(fragment);
}

Shader::~Shader() 
{
	if (mId != 0)
	{
		glDeleteProgram(mId);
		mId = 0;
	}
}

// static method implementation
std::shared_ptr<Shader> Shader::CreateWithBuilder(const std::string& vertexPath, const std::string& fragmentPath)
{
	te::ShaderBuilder builder;
	return builder.BuildShader(vertexPath, fragmentPath);
}

std::shared_ptr<Shader> Shader::CreateWithBuilder(const std::string& vertexPath, const std::string& fragmentPath, 
                                                 const te::ShaderPreprocessorConfig& config)
{
	te::ShaderBuilder builder(config);
	return builder.BuildShader(vertexPath, fragmentPath);
}

// utility function for checking shader compilation/linking errors.
	// ------------------------------------------------------------------------
bool Shader::checkCompileErrors(unsigned int shader, std::string type)
{
	int success;
	char infoLog[1024];
	if (type != "PROGRAM")
	{
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
		}
		return success != 0;
	}
	else
	{
		glGetProgramiv(shader, GL_LINK_STATUS, &success);
		if (!success)
		{
			glGetProgramInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
		}
		return success != 0;
	}
}

// activate the shader
	// ------------------------------------------------------------------------
void Shader::use()
{
	if (mId != 0)
	{
		glUseProgram(mId);
	}
	else
	{
		std::cout << "WARNING::SHADER::use() called on invalid shader (mId = 0)" << std::endl;
	}
}

// utility uniform functions
// ------------------------------------------------------------------------
void Shader::setBool(const std::string& name, bool value) const
{
	if (mId != 0)
	{
		glUniform1i(glGetUniformLocation(mId, name.c_str()), (int)value);
	}
}
// ------------------------------------------------------------------------
void Shader::setInt(const std::string& name, int value) const
{
	if (mId != 0)
	{
		glUniform1i(glGetUniformLocation(mId, name.c_str()), value);
	}
}
// ------------------------------------------------------------------------
void Shader::setFloat(const std::string& name, float value) const
{
	if (mId != 0)
	{
		glUniform1f(glGetUniformLocation(mId, name.c_str()), value);
	}
}
// ------------------------------------------------------------------------
void Shader::setVec2(const std::string& name, const glm::vec2& value) const
{
	if (mId != 0)
	{
		glUniform2fv(glGetUniformLocation(mId, name.c_str()), 1, &value[0]);
	}
}
// ------------------------------------------------------------------------
void Shader::setVec3(const std::string& name, const glm::vec3& value) const
{
	if (mId != 0)
	{
		glUniform3fv(glGetUniformLocation(mId, name.c_str()), 1, &value[0]);
	}
}
void Shader::setVec3(const std::string& name, float x, float y, float z) const
{
	if (mId != 0)
	{
		glUniform3f(glGetUniformLocation(mId, name.c_str()), x, y, z);
	}
}
// ------------------------------------------------------------------------
void Shader::setVec4(const std::string& name, const glm::vec4& value) const
{
	if (mId != 0)
	{
		glUniform4fv(glGetUniformLocation(mId, name.c_str()), 1, &value[0]);
	}
}
void Shader::setVec4(const std::string& name, float x, float y, float z, float w) const
{
	if (mId != 0)
	{
		glUniform4f(glGetUniformLocation(mId, name.c_str()), x, y, z, w);
	}
}
// ------------------------------------------------------------------------
void Shader::setMat2(const std::string& name, const glm::mat2& mat) const
{
	if (mId != 0)
	{
		glUniformMatrix2fv(glGetUniformLocation(mId, name.c_str()), 1, GL_FALSE, &mat[0][0]);
	}
}
// ------------------------------------------------------------------------
void Shader::setMat3(const std::string& name, const glm::mat3& mat) const
{
	if (mId != 0)
	{
		glUniformMatrix3fv(glGetUniformLocation(mId, name.c_str()), 1, GL_FALSE, &mat[0][0]);
	}
}
// ------------------------------------------------------------------------
void Shader::setMat4(const std::string& name, const glm::mat4& mat) const
{
	if (mId != 0)
	{
		glUniformMatrix4fv(glGetUniformLocation(mId, name.c_str()), 1, GL_FALSE, &mat[0][0]);
	}
}