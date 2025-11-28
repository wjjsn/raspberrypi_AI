cmake_minimum_required(VERSION 3.24)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
cmake_path(GET CMAKE_CURRENT_SOURCE_DIR FILENAME CUR_DIR_NAME)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(${CUR_DIR_NAME})
# add_compile_options(-v)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(OpenCV REQUIRED)
find_package(HailoRT 4.20.0 EXACT REQUIRED)

add_executable(${CMAKE_PROJECT_NAME} 
    main.cpp
    hailo_init.cpp
    )

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE 
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE 
    HailoRT::libhailort 
    Threads::Threads
    ${OpenCV_LIBS}
)

# set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES SUFFIX ".elf")