add_executable(ShaderPreprocessorRenderer
    main.cpp
)

target_link_libraries(ShaderPreprocessorRenderer
    ${ALL_LIBS}
)

target_compile_features(ShaderPreprocessorRenderer PRIVATE cxx_std_17)

# 设置输出目录
set_target_properties(ShaderPreprocessorRenderer
    PROPERTIES
    FOLDER "Examples"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>
)

# 添加依赖
add_dependencies(ShaderPreprocessorRenderer GTinyEngine) 