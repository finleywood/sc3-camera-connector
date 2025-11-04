cmake_minimum_required(VERSION 3.5.0)

project(SC3-Camera-Connector VERSION 1.0 LANGUAGES CXX)

# Require C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(include)
add_subdirectory(src)

find_package(k4a REQUIRED)
find_package(OpenCV REQUIRED)

# Add your source files
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE k4a::k4a ${OpenCV_LIBS})

target_include_directories(${PROJECT_NAME} PRIVATE include)
