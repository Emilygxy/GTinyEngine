cmake_minimum_required(VERSION 3.8)

project(TinyRenderer)

# #收集文件
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

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

# 定义宏将资源路径传递给代码
target_compile_definitions(TinyRenderer PRIVATE 
    RESOURCE_PATH="${RESOURCE_OUTPUT_DIR}/"
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