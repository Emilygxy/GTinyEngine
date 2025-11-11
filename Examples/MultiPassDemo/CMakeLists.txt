add_executable(MultiPassDemo
    main.cpp
)

target_link_libraries(MultiPassDemo
    ${ALL_LIBS}
)

target_compile_features(MultiPassDemo PRIVATE cxx_std_17)

# 设置输出目录
set_target_properties(MultiPassDemo
    PROPERTIES
    FOLDER "Examples/opengl"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>
)

# 添加依赖
add_dependencies(MultiPassDemo GTinyEngine) 