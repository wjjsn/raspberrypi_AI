cmake_minimum_required(VERSION 3.24)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
cmake_path(GET CMAKE_CURRENT_SOURCE_DIR FILENAME CUR_DIR_NAME)

project(${CUR_DIR_NAME})

find_package(onnxruntime REQUIRED)
add_executable(${CMAKE_PROJECT_NAME} main.cpp)
target_link_libraries(yolo_test PRIVATE 
    onnxruntime::onnxruntime
    )
# set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES SUFFIX ".elf")