cmake_minimum_required(VERSION 3.8)

project(TestRenderer)

# #收集文件
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

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
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>/
)
target_compile_features(TestRenderer PRIVATE cxx_std_17)