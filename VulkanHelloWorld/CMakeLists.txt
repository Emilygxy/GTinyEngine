cmake_minimum_required(VERSION 3.8)

project(VulkanHelloWorld)

# #收集文件
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h )

#生成可执行程序
add_executable(VulkanHelloWorld
    ${HEADERS}
    ${SOURCES}
)

target_include_directories(VulkanHelloWorld PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}source/)

target_link_libraries(VulkanHelloWorld PUBLIC ${ALL_LIBS})
set_target_properties(VulkanHelloWorld
	PROPERTIES 
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>/
)
target_compile_features(VulkanHelloWorld PRIVATE cxx_std_17)