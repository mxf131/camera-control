cmake_minimum_required(VERSION 3.4.1)

project(camera-control)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-pthread")

# skip 3rd-party lib dependencies
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")

# opencv 
find_package(OpenCV REQUIRED)

# spdlog
find_package(spdlog REQUIRED)

# yolo
add_executable(camera
        src/main.cpp
)

target_link_libraries(camera
  ${OpenCV_LIBS}
  spdlog::spdlog
)
add_executable(frame-test src/frame-test.cpp)

target_link_libraries(frame-test
        ${OpenCV_LIBS}
        spdlog::spdlog
)

