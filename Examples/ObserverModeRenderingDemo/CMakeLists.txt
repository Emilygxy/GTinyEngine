add_executable(ObserverModeRenderingDemo
    main.cpp
)

target_link_libraries(ObserverModeRenderingDemo
    ${ALL_LIBS}
)

target_compile_features(ObserverModeRenderingDemo PRIVATE cxx_std_17)

# 设置输出目录
set_target_properties(ObserverModeRenderingDemo
    PROPERTIES
    FOLDER "Examples/opengl"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>
)

# 添加依赖
add_dependencies(ObserverModeRenderingDemo GTinyEngine) 