cmake_minimum_required(VERSION 3.8)

project(TinyRenderer)

# 包含辅助函数
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
include(SetSourceGroup)

# #收集文件
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

# 为源文件和头文件设置 source_group
set_source_group_for_files("${SOURCES}")
set_source_group_for_files("${HEADERS}")

#生成可执行程序
add_executable(TinyRenderer
    ${HEADERS}
    ${SOURCES}
)

target_include_directories(TinyRenderer PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include/)

target_link_libraries(TinyRenderer PUBLIC ${ALL_LIBS})
set_target_properties(TinyRenderer
	PROPERTIES 
	# FOLDER "Examples"
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>/
)
target_compile_features(TinyRenderer PRIVATE cxx_std_17)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/EngineConfig.json" TINYRENDERER_ENGINE_CONFIG_SOURCE)
target_compile_definitions(TinyRenderer PRIVATE 
    RESOURCE_PATH="${RESOURCE_OUTPUT_DIR}/"
    ENGINE_CONFIG_SOURCE_PATH="${TINYRENDERER_ENGINE_CONFIG_SOURCE}"
)

set(TINYRENDERER_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/EngineConfig.json)
set_source_files_properties(${TINYRENDERER_CONFIG_FILE} PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(TinyRenderer PRIVATE ${TINYRENDERER_CONFIG_FILE})
source_group("Config" FILES ${TINYRENDERER_CONFIG_FILE})

add_custom_command(TARGET TinyRenderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${TINYRENDERER_CONFIG_FILE}
        $<TARGET_FILE_DIR:TinyRenderer>/EngineConfig.json
    COMMENT "Copying EngineConfig.json next to TinyRenderer"
)

# 将上级资源拷贝到work path中
add_custom_command(TARGET TinyRenderer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/resources
  COMMENT "Creating resources directory for current config"
)

add_custom_command(TARGET TinyRenderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${RESOURCE_OUTPUT_DIR}
    ${PROJECT_BINARY_DIR}/resources
    COMMENT "Copying resources to build directory"
)