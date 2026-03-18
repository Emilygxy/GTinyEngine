cmake_minimum_required(VERSION 3.8)
project(GTinyEngine)

# 包含辅助函数
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(SetSourceGroup)

# 设置输出目录（确保所有配置都输出到 bin/ 目录）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
function(get_config_output_path OUT_VAR)
    # 动态生成路径：自动适配单/多配置生成器
    set(config_path "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")
    set(${OUT_VAR} "${config_path}" PARENT_SCOPE)
endfunction()

# 调用函数获取当前路径
get_config_output_path(CURRENT_CONFIG_OUTPUT_DIR)

# Vulkan Env
set(VULKAN_SDK_VALUE "${CMAKE_SOURCE_DIR}/VulkanSDK/1.3.275.0/Bin")
configure_file(configuration/SetVulkanEnv.bat.in configuration/SetVulkanEnv.bat @ONLY)
message(STATUS "若未安装Vulkan环境, 请运行 ${CMAKE_BINARY_DIR}/configuration/SetVulkanEnv.bat 以设置Vulkan环境变量")

# 收集 imgui 源文件
set(IMGUI_SOURCES
	#ImGui Core
	${CMAKE_SOURCE_DIR}/3rdParty/imgui/imgui.cpp
	${CMAKE_SOURCE_DIR}/3rdParty/imgui/imgui_demo.cpp
	${CMAKE_SOURCE_DIR}/3rdParty/imgui/imgui_draw.cpp
	${CMAKE_SOURCE_DIR}/3rdParty/imgui/imgui_tables.cpp
	${CMAKE_SOURCE_DIR}/3rdParty/imgui/imgui_widgets.cpp
	# ${CMAKE_SOURCE_DIR}/3rdParty/imgui/imgui_file_browser/ImGuiFileBrowser.cpp
	#ImGui Backends
	${CMAKE_SOURCE_DIR}/3rdParty/imgui/backends/imgui_impl_glfw.cpp
	${CMAKE_SOURCE_DIR}/3rdParty/imgui/backends/imgui_impl_opengl3.cpp
	#ImGui Extras
	${CMAKE_SOURCE_DIR}/3rdParty/imgui/misc/cpp/imgui_stdlib.cpp
)

add_library(imgui ${IMGUI_SOURCES})

# 为 imgui 文件设置 source_group
set_source_group_for_files("${IMGUI_SOURCES}")

set_target_properties(imgui
    PROPERTIES
    FOLDER "Libraries"
    # 设置PDB文件输出路径，避免PDB冲突
    PDB_NAME "imgui"
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>"
)

set(GLFW_GLAD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenGL/include)
set(VULKAN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/VulkanSDK/1.3.275.0)
set(VULKAN_INCLUDE_DIR ${VULKAN_DIR}/Include)
set(VULKAN_LIB ${VULKAN_DIR}/Lib/vulkan-1.lib)
set(GLFW_LIB ${CMAKE_CURRENT_SOURCE_DIR}/OpenGL/lib/glfw3.lib)
 
set(FFMPEG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ffmpeg/include)
set(FFMPEG_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ffmpeg/lib)

#收集文件
file(GLOB SOURCES  ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
				   ${CMAKE_CURRENT_SOURCE_DIR}/source/memory/*.cpp
				   ${CMAKE_CURRENT_SOURCE_DIR}/source/mesh/*.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/source/skybox/*.cpp
				   ${CMAKE_CURRENT_SOURCE_DIR}/source/materials/*.cpp
				   ${CMAKE_CURRENT_SOURCE_DIR}/source/textures/*.cpp
				   ${CMAKE_CURRENT_SOURCE_DIR}/source/framework/*.cpp
				   ${CMAKE_CURRENT_SOURCE_DIR}/source/mesh/curve/*.cpp
				   ${CMAKE_CURRENT_SOURCE_DIR}/source/geometry/*.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/source/shader/*.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/source/math/*.cpp
				   ${CMAKE_CURRENT_SOURCE_DIR}/OpenGL/src/*.c)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
				  ${CMAKE_CURRENT_SOURCE_DIR}/include/framework/*.h
				  ${CMAKE_CURRENT_SOURCE_DIR}/include/common/*.h
				  ${CMAKE_CURRENT_SOURCE_DIR}/include/materials/*.h
				  ${CMAKE_CURRENT_SOURCE_DIR}/include/mesh/*.h
                  ${CMAKE_CURRENT_SOURCE_DIR}/include/skybox/*.h
				  ${CMAKE_CURRENT_SOURCE_DIR}/include/textures/*.h
				  ${CMAKE_CURRENT_SOURCE_DIR}/include/geometry/*.h
                  ${CMAKE_CURRENT_SOURCE_DIR}/include/math/*.h
                  ${CMAKE_CURRENT_SOURCE_DIR}/include/shader/*.h
                  )

# 为源文件和头文件设置 source_group
set_source_group_for_files("${SOURCES}")
set_source_group_for_files("${HEADERS}")

# configure root dir for cpp
configure_file(configuration/root_dir.h.in configuration/root_dir.h)

set(THIRD_PARTY_LIB 
	${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/glm/lib/glm.lib
	${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/assimp/lib/assimp.lib
	imgui
	# ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ffmpeg/lib/avcodec.lib
	# ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ffmpeg/lib/avformat.lib
    # ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ffmpeg/lib/avutil.lib
    # ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ffmpeg/lib/swscale.lib
    # ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ffmpeg/lib/avdevice.lib
    # ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ffmpeg/lib/swresample.lib
	# more
	)

set(THIRD_PARTY_INCLUDE_DIR 
	${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/glm/
	${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/assimp/
	${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/imgui/
	${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ffmpeg/include/
	# more
	)

#添加依赖的头文件
include_directories(
	${GLFW_GLAD_INCLUDE_DIR}
	${VULKAN_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_BINARY_DIR}/configuration
	${THIRD_PARTY_INCLUDE_DIR})

# copy dlls files, ahead of GTinyEngine library
set(THIRD_PARTY_DLLS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/assimp/dlls/
					# more
					)
add_custom_target(copy-runtime-files ALL COMMAND ${CMAKE_COMMAND} -E copy_directory 
					${THIRD_PARTY_DLLS_DIR} ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>/ 
					# more
					DEPENDS ${TARGET})

add_library(GTinyEngine STATIC 
			${HEADERS}
			${SOURCES})  ## lib

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_target_properties(copy-runtime-files
    PROPERTIES
	FOLDER "Libraries/copyAssect"
)
set_target_properties(GTinyEngine
    PROPERTIES
	FOLDER "Libraries"
  #  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/#$<CONFIGURATION>/
)

add_dependencies(GTinyEngine copy-runtime-files)
target_compile_features(GTinyEngine PRIVATE cxx_std_17)

set(ALL_LIBS
	${GLFW_LIB}
	${VULKAN_LIB}
	${THIRD_PARTY_LIB}
	GTinyEngine
)

# 设置资源目录路径（假设资源在项目根目录的 Resources/ 下）
set(RESOURCE_DIR ${CMAKE_SOURCE_DIR}/resources)
set(RESOURCE_OUTPUT_DIR "${CURRENT_CONFIG_OUTPUT_DIR}/resources")
# 定义宏将资源路径传递给代码
target_compile_definitions(GTinyEngine PUBLIC 
    RESOURCE_PATH="${RESOURCE_OUTPUT_DIR}/"
)

#生成可执行程序
add_subdirectory(Examples/TestRenderer)
add_subdirectory(Examples/VulkanHelloWorld)
add_subdirectory(Examples/VulkanPractice1)
add_subdirectory(Examples/VulkanPractice2)
add_subdirectory(Examples/VK_VertixTriangle)

add_subdirectory(Examples/RendererDemo)
add_subdirectory(Examples/MultiPassDemo)
add_subdirectory(Examples/ShaderPreprocessorSimpleExample)
add_subdirectory(Examples/LoadModelDemo)
add_subdirectory(Examples/MultiPassWithBackgroundDemo)
add_subdirectory(Examples/ObserverModeRenderingDemo)
add_subdirectory(TinyRenderer)

add_custom_command(TARGET GTinyEngine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${RESOURCE_OUTPUT_DIR}
  COMMENT "Creating resources directory for current config"
)

add_custom_command(TARGET GTinyEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${RESOURCE_DIR}
    ${RESOURCE_OUTPUT_DIR}
    COMMENT "Copying resources to build directory"
)

## 编译着色器 Starting...
# 设置着色器源目录和输出目录
set(VK_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/vk)
set(VK_SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/resources/compiled_shaders)
# make sure output directory exists
file(MAKE_DIRECTORY ${VK_SHADER_OUTPUT_DIR})
# 查找 glslangValidator
find_program(GLSLANG_VALIDATOR glslangValidator
    HINTS
        "${VULKAN_DIR}/Bin"
        "${VULKAN_DIR}/Bin32"
        "$ENV{VULKAN_DIR}/Bin"
        "$ENV{VULKAN_DIR}/Bin32"
    DOC "Path to glslangValidator executable"
)

if(NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found! Make sure Vulkan SDK is installed and VULKAN_SDK is set")
endif()
message(STATUS "Found glslangValidator: ${GLSLANG_VALIDATOR}")
# 定义着色器文件扩展名与类型的映射
set(SHADER_TYPES
    ".vert=vert" 
    ".frag=frag" 
    ".tesc=tesc" 
    ".tese=tese" 
    ".geom=geom" 
    ".comp=comp"
)
# 收集所有着色器文件, CONFIGURE_DEPENDS确保新增文件时自动更新列表
file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS
    "${VK_SHADER_SOURCE_DIR}/*.vert"
    "${VK_SHADER_SOURCE_DIR}/*.frag"
    "${VK_SHADER_SOURCE_DIR}/*.tesc"
    "${VK_SHADER_SOURCE_DIR}/*.tese"
    "${VK_SHADER_SOURCE_DIR}/*.geom"
    "${VK_SHADER_SOURCE_DIR}/*.comp"
)
# Print file length
list(LENGTH SHADER_SOURCES TOTAL_FILES)
message(STATUS "Total vk shader source files: ${TOTAL_FILES}")

# 创建着色器编译命令
foreach(SHADER_SOURCE ${SHADER_SOURCES})
    # 获取文件名和扩展名
    get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME_WE)
    get_filename_component(SHADER_EXT ${SHADER_SOURCE} EXT)
    
    # 确定着色器类型
    set(SHADER_TYPE "")
    foreach(TYPE_MAP ${SHADER_TYPES})
        string(REPLACE "=" ";" TYPE_MAP_LIST ${TYPE_MAP})
        list(GET TYPE_MAP_LIST 0 EXT)
        list(GET TYPE_MAP_LIST 1 TYPE)
        if(${SHADER_EXT} STREQUAL ${EXT})
            set(SHADER_TYPE ${TYPE})
            break()
        endif()
    endforeach()
	if(NOT SHADER_TYPE)
        message(WARNING "Unknown shader type for ${SHADER_SOURCE}")
        continue()
    endif()
    
    # 设置输出文件路径
    set(OUTPUT_FILE ${VK_SHADER_OUTPUT_DIR}/${SHADER_NAME}_${SHADER_TYPE}.spv)
    
    # 添加自定义编译命令
    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        COMMAND ${GLSLANG_VALIDATOR}
            -V                     # 输出SPIR-V
            -o ${OUTPUT_FILE}      # 输出文件
            ${SHADER_SOURCE}       # 输入文件
            --target-env vulkan1.3 # 目标环境
            -g                     # 生成调试信息
        DEPENDS ${SHADER_SOURCE}
        COMMENT "Compiling ${SHADER_SOURCE} to SPIR-V"
        VERBATIM
    )
    
    # 收集所有输出文件
    list(APPEND SHADER_BINARIES ${OUTPUT_FILE})
endforeach()

# 计算文件数量
list(LENGTH SHADER_BINARIES SHADER_BINARIES_COUNT)
# 输出日志（STATUS 前缀显示为 "--"）
message(STATUS "Number of .spv files: ${SHADER_BINARIES_COUNT}")

# 创建自定义目标以编译所有着色器

add_custom_target(CompileToVKShaders ALL
    DEPENDS ${SHADER_BINARIES}
    COMMENT "Compiling all shaders to SPIR-V"
)
# 设置VS项目文件夹
set_target_properties(CompileToVKShaders
    PROPERTIES
	FOLDER "Libraries"
)

# 使主目标依赖于着色器编译
add_dependencies(GTinyEngine CompileToVKShaders)

# 设置资源路径宏
target_compile_definitions(GTinyEngine PUBLIC
    VK_SHADER_RESOURCE_PATH="${VK_SHADER_OUTPUT_DIR}/"
)
# 在代码中访问着色器路径
message(STATUS "Vulkan shader binaries will be available at: ${VK_SHADER_OUTPUT_DIR}")

add_custom_command(TARGET GTinyEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${VK_SHADER_OUTPUT_DIR}
    ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>/resources/compiled_shaders
    COMMENT "Copying resources to build directory"
)

## 编译着色器 Ending...
