cmake_minimum_required(VERSION 3.5.0)

project(SC3-Camera-Connector VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Require C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(include)
add_subdirectory(src)

find_package(k4a REQUIRED)
find_package(OpenCV REQUIRED)
find_package(CURL REQUIRED)

# Add your source files
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

add_library(miniz STATIC src/miniz/miniz.c)

target_include_directories(miniz PUBLIC include/miniz)

target_link_libraries(${PROJECT_NAME} PRIVATE k4a::k4a ${OpenCV_LIBS} CURL::libcurl)

target_include_directories(${PROJECT_NAME} PRIVATE include)

target_link_libraries(${PROJECT_NAME} PRIVATE miniz)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(FILES ./auxiliary/config.json
		DESTINATION /etc/sc3
)

install(FILES ./auxiliary/sc3-camera-connector.service
		DESTINATION /lib/systemd/system
)

set(CPACK_GENERATOR "DEB")

execute_process(
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/gitver.sh
		OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE VERSION_FROM_SCRIPT
)

message(STATUS "FOUND VERSION AS ${VERSION_FROM_SCRIPT}")

set(CPACK_PACKAGE_NAME "sc3-camera-connector")
set(CPACK_PACKAGE_VERSION ${VERSION_FROM_SCRIPT})
set(CPACK_DEBIAN_PACKAGE_VERSION ${VERSION_FROM_SCRIPT})
set(CPACK_PACKAGE_CONTACT "Finley Wood <fjw1g21@soton.ac.uk>")
set(CPACK_PACKAGE_DESCRIPTION "Camera and processing backend for SC3")

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_SOURCE_DIR}/debian/postinst"
)

set(CPACK_DEBIAN_PACKAGE_DEPENDS "libk4a1.4 (>=1.4), libcurl4t64 (>= 8), libopencv-core410 (>= 4.10), libopencv-highgui410 (>= 4.10), libopencv-imgcodecs410 (>= 4.10), libopencv-objdetect410 (>= 4.10), libopencv-imgproc410 (>= 4.10)")

# Optional: architecture override (auto-detects otherwise)
# set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")

include(CPack)
