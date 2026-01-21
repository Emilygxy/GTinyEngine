add_executable(MultiPassWithBackgroundDemo
    main.cpp
)

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