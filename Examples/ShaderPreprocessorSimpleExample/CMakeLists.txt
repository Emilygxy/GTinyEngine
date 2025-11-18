add_executable(ShaderPreprocessorSimpleExample
    main.cpp
)

target_link_libraries(ShaderPreprocessorSimpleExample
    ${ALL_LIBS}
)

target_compile_features(ShaderPreprocessorSimpleExample PRIVATE cxx_std_17)

# 设置输出目录
set_target_properties(ShaderPreprocessorSimpleExample
    PROPERTIES
    FOLDER "Examples/opengl"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>
)

# 添加依赖
add_dependencies(ShaderPreprocessorSimpleExample GTinyEngine)