add_executable(LoadModelDemo
    main.cpp
)

target_link_libraries(LoadModelDemo
    ${ALL_LIBS}
)

target_compile_features(LoadModelDemo PRIVATE cxx_std_17)

# 设置输出目录
set_target_properties(LoadModelDemo
    PROPERTIES
    FOLDER "Examples"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>
)

# 添加依赖
add_dependencies(LoadModelDemo GTinyEngine) 