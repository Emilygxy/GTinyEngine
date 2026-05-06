cmake_minimum_required(VERSION 3.8)

project(VK_DeferredM1Demo)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)
include(SetSourceGroup)

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

set_source_group_for_files("${SOURCES}")
set_source_group_for_files("${HEADERS}")

add_executable(VK_DeferredM1Demo
    ${HEADERS}
    ${SOURCES}
)

add_custom_command(TARGET VK_DeferredM1Demo POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/resources
  COMMENT "Creating resources directory for current config"
)

add_custom_command(TARGET VK_DeferredM1Demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${RESOURCE_OUTPUT_DIR}
    ${PROJECT_BINARY_DIR}/resources
    COMMENT "Copying resources to build directory"
)

target_include_directories(VK_DeferredM1Demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
)

target_link_libraries(VK_DeferredM1Demo PRIVATE ${ALL_LIBS} GTVulkan)
set_target_properties(VK_DeferredM1Demo
    PROPERTIES
    FOLDER "Examples/vulkan"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>/
)
target_compile_features(VK_DeferredM1Demo PRIVATE cxx_std_20)

