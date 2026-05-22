cmake_minimum_required(VERSION 3.8)
project(ShadowRenderingDemo)

# 包含辅助函数
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)
include(SetSourceGroup)

# 收集文件
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp) 
set_source_group_for_files(TREE ${CMAKE_CURRENT_SOURCE_DIR}/source PREFIX "Source" FILES ${SOURCES})

file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
set_source_group_for_files(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX "Include" FILES ${HEADERS})

# 生成可执行程序
add_executable(ShadowRenderingDemo
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(ShadowRenderingDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/source/)
target_link_libraries(ShadowRenderingDemo PUBLIC ${ALL_LIBS})
set_target_properties(ShadowRenderingDemo
    PROPERTIES 
    FOLDER "Examples/opengl"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>/
)

target_compile_features(ShadowRenderingDemo PRIVATE cxx_std_17)

