cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY lib)

include_directories(src include include/asio)
link_directories(lib)

project(2D_Physics_Engine VERSION 0.0)

add_executable(Engine src/client_main.cpp)

add_library(Glad src/glad.c)

add_library(Engine_files src/wrapper.cpp src/core.cpp src/random.cpp src/ecs.cpp src/render.cpp src/input.cpp src/physics.cpp src/gui.cpp src/erosion.cpp)

target_link_libraries(Engine Glad)

target_link_libraries(Engine Engine_files)

target_link_libraries(Engine glfw3 gdi32 user32 shell32 opengl32 assimp ws2_32 wsock32 mingw32 mingwex winpthread)

set(CMAKE_BUILD_TYPE Release)

set(CMAKE_CXX_FLAGS_DEBUG "-g -O3 -fpermissive -march=native")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O3 -pthread -fpermissive -march=native -static")
