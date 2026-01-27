cmake_minimum_required(VERSION 3.8)

project(TestRenderer)

# 包含辅助函数
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)
include(SetSourceGroup)

# #收集文件
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

# 为源文件和头文件设置 source_group
set_source_group_for_files("${SOURCES}")
set_source_group_for_files("${HEADERS}")

#生成可执行程序
add_executable(TestRenderer
    ${HEADERS}
    ${SOURCES}
)

target_include_directories(TestRenderer PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}source/)

target_link_libraries(TestRenderer PUBLIC ${ALL_LIBS})
set_target_properties(TestRenderer
	PROPERTIES 
	FOLDER "Examples/opengl"
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>/
)
target_compile_features(TestRenderer PRIVATE cxx_std_17)