add_executable(PBRMaterialDemo
    main.cpp
)

target_link_libraries(PBRMaterialDemo
    ${ALL_LIBS}
)

target_compile_features(PBRMaterialDemo PRIVATE cxx_std_17)

# 设置输出目录
set_target_properties(PBRMaterialDemo
    PROPERTIES
    FOLDER "Examples"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>
)

# 添加依赖
add_dependencies(PBRMaterialDemo GTinyEngine) 