cmake_minimum_required(VERSION 3.8)
project(GTinyEngine)

add_library(imgui
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

set(GLFW_GLAD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenGL/include)
set(VULKAN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/VulkanSDK/1.3.275.0)
set(VULKAN_INCLUDE_DIR ${VULKAN_DIR}/Include)
set(VULKAN_LIB ${VULKAN_DIR}/Lib/vulkan-1.lib)
set(GLFW_LIB ${CMAKE_CURRENT_SOURCE_DIR}/OpenGL/lib/glfw3.lib)

#收集文件
file(GLOB SOURCES  ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
				   ${CMAKE_CURRENT_SOURCE_DIR}/source/memory/*.cpp
				   ${CMAKE_CURRENT_SOURCE_DIR}/source/mesh/*.cpp
				   ${CMAKE_CURRENT_SOURCE_DIR}/source/materials/*.cpp
				   ${CMAKE_CURRENT_SOURCE_DIR}/source/skybox/*.cpp
				   ${CMAKE_CURRENT_SOURCE_DIR}/source/mesh/curve/*.cpp
				   ${CMAKE_CURRENT_SOURCE_DIR}/source/mesh/geometry/*.cpp
				   ${CMAKE_CURRENT_SOURCE_DIR}/OpenGL/src/*.c)
#file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
#				  ${CMAKE_CURRENT_SOURCE_DIR}/include/common/*.h)

# configure root dir for cpp
configure_file(configuration/root_dir.h.in configuration/root_dir.h)

set(THIRD_PARTY_LIB 
	${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/glm/lib/glm.lib
	${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/assimp/lib/assimp.lib
	imgui
	# more
	)

set(THIRD_PARTY_INCLUDE_DIR 
	${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/glm/
	${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/assimp/
	${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/imgui/
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
	GTinyEngine
	${THIRD_PARTY_LIB}
)

# 设置资源目录路径（假设资源在项目根目录的 Resources/ 下）
set(RESOURCE_DIR ${CMAKE_SOURCE_DIR}/resources)
# 创建构建输出目录中的资源目录
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/resources)
# 复制所有资源到输出目录
add_custom_command(TARGET GTinyEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${RESOURCE_DIR}
    ${CMAKE_BINARY_DIR}/resources
    COMMENT "Copying resources to build directory"
)
# 定义宏将资源路径传递给代码
target_compile_definitions(GTinyEngine PRIVATE 
    RESOURCE_PATH="${CMAKE_BINARY_DIR}/resources/"
)

#生成可执行程序
add_subdirectory(TestRenderer)
add_subdirectory(VulkanHelloWorld)
