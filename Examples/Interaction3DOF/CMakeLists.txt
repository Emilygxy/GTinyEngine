cmake_minimum_required(VERSION 3.8)

project(Interaction3DOF)

# #收集文件
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

#生成可执行程序
add_executable(Interaction3DOF
    ${HEADERS}
    ${SOURCES}
)

target_include_directories(Interaction3DOF PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include/)

target_link_libraries(Interaction3DOF PUBLIC ${ALL_LIBS})
set_target_properties(Interaction3DOF
	PROPERTIES 
	FOLDER "Examples"
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIGURATION>/
)
target_compile_features(Interaction3DOF PRIVATE cxx_std_17)