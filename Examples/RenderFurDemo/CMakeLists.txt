add_executable(RenderFurDemo
    main.cpp
)

target_link_libraries(RenderFurDemo
    ${ALL_LIBS}
)

target_compile_features(RenderFurDemo PRIVATE cxx_std_17)

# 设置输出目录
set_target_properties(RenderFurDemo
    PROPERTIES
    FOLDER "Examples"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>
)

# 添加依赖
add_dependencies(RenderFurDemo GTinyEngine) 