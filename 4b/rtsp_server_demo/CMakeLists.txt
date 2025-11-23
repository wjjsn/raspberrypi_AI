cmake_minimum_required(VERSION 3.24)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
cmake_path(GET CMAKE_CURRENT_SOURCE_DIR FILENAME CUR_DIR_NAME)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(${CUR_DIR_NAME})
set(PKG_CONFIG_EXECUTABLE /usr/bin/pkg-config)
find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GST REQUIRED gstreamer-1.0)
pkg_check_modules(GSTAPP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GSTBASE REQUIRED gstreamer-base-1.0)
pkg_check_modules(GSTRTP REQUIRED gstreamer-rtp-1.0)
pkg_check_modules(GSTRTSP REQUIRED gstreamer-rtsp-server-1.0)

include_directories(
    ${GST_INCLUDE_DIRS}
    ${GSTAPP_INCLUDE_DIRS}
    ${GSTBASE_INCLUDE_DIRS}
    ${GSTRTP_INCLUDE_DIRS}
    ${GSTRTSP_INCLUDE_DIRS}
)

link_directories(
    ${GST_LIBRARY_DIRS}
    ${GSTAPP_LIBRARY_DIRS}
    ${GSTBASE_LIBRARY_DIRS}
    ${GSTRTP_LIBRARY_DIRS}
    ${GSTRTSP_LIBRARY_DIRS}
)
add_executable(${CMAKE_PROJECT_NAME} main.cpp)

include_directories(${OpenCV_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE 
    ${OpenCV_LIBS}
    ${GST_LIBRARIES}
    ${GSTAPP_LIBRARIES}
    ${GSTBASE_LIBRARIES}
    ${GSTRTP_LIBRARIES}
    ${GSTRTSP_LIBRARIES}
    )

# set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES SUFFIX ".elf")