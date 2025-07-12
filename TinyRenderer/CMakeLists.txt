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
	${CMAKE_CURRENT_SOURCE_DIR}source/)

target_link_libraries(TinyRenderer PUBLIC ${ALL_LIBS})
set_target_properties(TinyRenderer
	PROPERTIES 
	# FOLDER "Examples"
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>/
)
target_compile_features(TinyRenderer PRIVATE cxx_std_17)