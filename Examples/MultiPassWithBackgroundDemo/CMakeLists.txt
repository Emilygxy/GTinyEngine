# 包含辅助函数
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)
include(SetSourceGroup)

add_executable(MultiPassWithBackgroundDemo
    main.cpp
)

# 为源文件设置 source_group（需要在 add_executable 之后）
set_source_group_for_files("${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

target_link_libraries(MultiPassWithBackgroundDemo
    ${ALL_LIBS}
)

target_compile_features(MultiPassWithBackgroundDemo PRIVATE cxx_std_17)

# 设置输出目录
set_target_properties(MultiPassWithBackgroundDemo
    PROPERTIES
    FOLDER "Examples/opengl"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>
)

# 添加依赖
add_dependencies(MultiPassWithBackgroundDemo GTinyEngine) 