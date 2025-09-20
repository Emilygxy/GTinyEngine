#include <iostream>
#include <memory>
#include <sstream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "shader.h"
#include "shader/ShaderPreprocessor.h"

// simple OpenGL context initialization
bool InitializeOpenGL(GLFWwindow*& window) {
    // initialize GLFW
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // configure GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // create window
    window = glfwCreateWindow(800, 600, "Shader Preprocessor Demo", NULL, NULL);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);

    // initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    return true;
}

// example 1: basic usage
void Example1_BasicUsage() {
    std::cout << "\n=== Example 1: Basic Usage ===" << std::endl;
    
    //use the default configuration to create a shader
    Shader shader("resources/shaders/basic.vs", "resources/shaders/basic.fs");
    
    if (shader.IsValid()) {
        std::cout << "Shader created successfully!" << std::endl;
        std::cout << "Shader ID: " << shader.GetID() << std::endl;
    } else {
        std::cout << "Failed to create shader!" << std::endl;
    }
}

// example 2: use custom configuration
void Example2_CustomConfig() {
    std::cout << "\n=== Example 2: Custom Configuration ===" << std::endl;
    
    // create custom configuration
    te::ShaderPreprocessorConfig config;
    config.shaderDirectory = "resources/shaders/";
    config.includeDirectory = "resources/shaders/includes/";
    config.enableMacroExpansion = true;
    config.enableIncludeProcessing = false;
    
    // use configuration to create a shader
    Shader shader("resources/shaders/basic.vs", "resources/shaders/basic.fs", config);
    
    if (shader.IsValid()) {
        std::cout << "Shader created with custom config successfully!" << std::endl;
    } else {
        std::cout << "Failed to create shader with custom config!" << std::endl;
    }
}

// example 3: use ShaderBuilder
void Example3_ShaderBuilder() {
    std::cout << "\n=== Example 3: Using ShaderBuilder ===" << std::endl;
    
    // use ShaderBuilder for chain operation
    auto shader = te::ShaderBuilder()
        .Define("MAX_LIGHTS", "4")
        .Define("USE_SHADOWS", "1")
        .Define("PI", "3.14159265359")
        .Define("LERP", "mix($1, $2, $3)", true)  // function macro
        .BuildShader("resources/shaders/basic.vs", "resources/shaders/basic.fs");
    
    if (shader && shader->IsValid()) {
        std::cout << "Shader created with ShaderBuilder successfully!" << std::endl;
        std::cout << "Shader ID: " << shader->GetID() << std::endl;
    } else {
        std::cout << "Failed to create shader with ShaderBuilder!" << std::endl;
    }
}

// example 4: dynamic macro management
void Example4_DynamicMacros() {
    std::cout << "\n=== Example 4: Dynamic Macro Management ===" << std::endl;
    
    // create preprocessor instance
    te::ShaderPreprocessor preprocessor;
    
    // add some macros
    preprocessor.DefineMacro("FEATURE_A", "1");
    preprocessor.DefineMacro("FEATURE_B", "0");
    preprocessor.DefineMacro("MAX_ITERATIONS", "100");
    
    // check if the macro is defined
    if (preprocessor.IsMacroDefined("FEATURE_A")) {
        std::cout << "FEATURE_A is defined" << std::endl;
    }
    
    if (preprocessor.IsMacroDefined("FEATURE_B")) {
        std::cout << "FEATURE_B is defined" << std::endl;
    }
    
    // undefine the macro
    preprocessor.UndefineMacro("FEATURE_B");
    
    if (!preprocessor.IsMacroDefined("FEATURE_B")) {
        std::cout << "FEATURE_B is undefined" << std::endl;
    }
    
    // use the preprocessor to create a shader
    Shader shader("resources/shaders/basic.vs", "resources/shaders/basic.fs", preprocessor);
    
    if (shader.IsValid()) {
        std::cout << "Shader created with dynamic macros successfully!" << std::endl;
    } else {
        std::cout << "Failed to create shader with dynamic macros!" << std::endl;
    }
}

// example 5: process include file shader
void Example5_IncludeProcessing() {
    std::cout << "\n=== Example 5: Processing Include File Shader ===" << std::endl;
    
    // create a shader with include file
    auto shader = te::ShaderBuilder()
        .Define("USE_TONE_MAPPING", "1")
        .Define("EXPOSURE", "1.0")
        .BuildShader("resources/shaders/postprocess/tone_mapping.vs", 
                     "resources/shaders/postprocess/tone_mapping.fs");
    
    if (shader && shader->IsValid()) {
        std::cout << "Shader with include files created successfully!" << std::endl;
    } else {
        std::cout << "Failed to create shader with include files!" << std::endl;
    }
}

// example 6: debug preprocessing result
void Example6_DebugPreprocessing() {
    std::cout << "\n=== Example 6: Debug Preprocessing Result ===" << std::endl;
    
    // create preprocessor
    te::ShaderPreprocessor preprocessor;
    preprocessor.DefineMacro("DEBUG_MODE", "1");
    preprocessor.DefineMacro("MAX_LIGHTS", "8");
    
    // process the shader content
    std::string processedContent = preprocessor.ProcessShader("resources/shaders/basic.fs");
    
    if (!processedContent.empty()) {
        std::cout << "Preprocessing successful!" << std::endl;
        std::cout << "Processed content length: " << processedContent.length() << " characters" << std::endl;
        
        // show the first few lines of content
        std::istringstream stream(processedContent);
        std::string line;
        int lineCount = 0;
        while (std::getline(stream, line) && lineCount < 10) {
            std::cout << "  " << line << std::endl;
            lineCount++;
        }
        if (lineCount >= 10) {
            std::cout << "  ... (more content)" << std::endl;
        }
    } else {
        std::cout << "Preprocessing failed!" << std::endl;
    }
}

int main() {
    std::cout << "GTinyEngine Shader Preprocessor Usage Examples" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    GLFWwindow* window = nullptr;
    
    // initialize OpenGL context
    if (!InitializeOpenGL(window)) {
        std::cout << "OpenGL initialization failed!" << std::endl;
        return -1;
    }
    
    std::cout << "OpenGL initialization successful!" << std::endl;
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
    
    // run examples
    //Example1_BasicUsage();
    Example2_CustomConfig();
   /* Example3_ShaderBuilder();
    Example4_DynamicMacros();
    Example5_IncludeProcessing();
    Example6_DebugPreprocessing();*/
    
    std::cout << "\n=== All examples completed ===" << std::endl;
    
    // clean up
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
    
    return 0;
}
