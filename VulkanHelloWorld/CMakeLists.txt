cmake_minimum_required(VERSION 3.8)

project(VulkanHelloWorld)

# 添加 PDB 设置
if(MSVC)
    # 设置调试信息格式
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Z7")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Z7")
    
    # 为每个目标单独设置 PDB 输出
    # set_target_properties(VulkanHelloWorld PROPERTIES
    #     COMPILE_PDB_NAME_DEBUG "VulkanHelloWorld"
    #     COMPILE_PDB_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    #     PDB_NAME_DEBUG "VulkanHelloWorld"
    #     PDB_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    # )
    
    # 确保输出目录存在
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Debug")
endif()

# #收集文件
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h )


#生成可执行程序
add_executable(VulkanHelloWorld
    ${HEADERS}
    ${SOURCES}
)

add_custom_command(TARGET VulkanHelloWorld POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/resources
  COMMENT "Creating resources directory for current config"
)

add_custom_command(TARGET VulkanHelloWorld POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${RESOURCE_OUTPUT_DIR}
    ${PROJECT_BINARY_DIR}/resources
    COMMENT "Copying resources to build directory"
)

target_include_directories(VulkanHelloWorld PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include/)

target_link_libraries(VulkanHelloWorld PUBLIC ${ALL_LIBS})
set_target_properties(VulkanHelloWorld
	PROPERTIES 
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>/
)
target_compile_features(VulkanHelloWorld PRIVATE cxx_std_17)