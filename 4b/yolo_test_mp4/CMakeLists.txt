cmake_minimum_required(VERSION 3.24)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
cmake_path(GET CMAKE_CURRENT_SOURCE_DIR FILENAME CUR_DIR_NAME)

project(${CUR_DIR_NAME})

find_package(onnxruntime REQUIRED)
find_package(OpenCV REQUIRED)
add_executable(${CMAKE_PROJECT_NAME} main.cpp)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE 
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE 
    onnxruntime::onnxruntime
    ${OpenCV_LIBS}
    )
# set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES SUFFIX ".elf")