cmake_minimum_required(VERSION 3.8)
project(GTVulkan)

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

set(VULKAN_DIR ${CMAKE_SOURCE_DIR}/VulkanSDK/1.3.275.0)
set(VULKAN_INCLUDE_DIR ${VULKAN_DIR}/Include)
set(VULKAN_LIB ${VULKAN_DIR}/Lib/vulkan-1.lib)

#收集文件
file(GLOB SOURCES  ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/GTVulkan/*.h )

# 为源文件和头文件设置 source_group
set_source_group_for_files("${SOURCES}")
set_source_group_for_files("${HEADERS}")

add_library(GTVulkan STATIC 
		${HEADERS}
		${SOURCES})  ## lib

# 头文件可见性：
# - PUBLIC: 使用 GTVulkan 头文件的下游目标也需要
# - PRIVATE: 仅 GTVulkan 自身编译需要
target_include_directories(GTVulkan
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${VULKAN_INCLUDE_DIR}
    PRIVATE
        ${CMAKE_BINARY_DIR}/configuration
)

# 链接可见性：
# - PUBLIC: 静态库对 Vulkan 的符号依赖需要传递到最终可执行目标
target_link_libraries(GTVulkan
    PUBLIC
        ${VULKAN_LIB}
)
		
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_target_properties(GTVulkan
    PROPERTIES
	FOLDER "Libraries"
  #  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/#$<CONFIGURATION>/
)

target_compile_features(GTVulkan PRIVATE cxx_std_20)

# 设置资源目录路径（假设资源在项目根目录的 Resources/ 下）
set(RESOURCE_DIR ${CMAKE_SOURCE_DIR}/resources)
set(RESOURCE_OUTPUT_DIR "${CURRENT_CONFIG_OUTPUT_DIR}/resources")
# 定义宏将资源路径传递给代码
target_compile_definitions(GTVulkan PUBLIC 
    RESOURCE_PATH="${RESOURCE_OUTPUT_DIR}/"
)