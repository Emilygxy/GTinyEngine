add_executable(VideoRenderDemo
    main.cpp
)

target_link_libraries(VideoRenderDemo
    ${ALL_LIBS}
)

target_compile_features(VideoRenderDemo PRIVATE cxx_std_17)

# 设置输出目录
set_target_properties(VideoRenderDemo
    PROPERTIES
    FOLDER "Examples"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>
)

# 添加依赖
add_dependencies(VideoRenderDemo GTinyEngine) 