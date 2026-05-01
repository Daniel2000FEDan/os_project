cmake_minimum_required(VERSION 3.20)
project(os_project C)

set(CMAKE_C_STANDARD 11)

# 1. Try to find raylib already installed on the Linux system
find_package(raylib QUIET)

# 2. If raylib is not found, download it automatically
if(NOT raylib_FOUND)
    message(STATUS "Raylib not found locally, downloading via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
            raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG        5.0
    )
    FetchContent_MakeAvailable(raylib)
endif()

# 3. Define the executable and the source file
add_executable(os_project main.c)

# 4. Link the raylib library to the project
if(raylib_FOUND)
    # On Linux, raylib requires additional system dependencies
    target_link_libraries(os_project raylib m pthread dl rt X11)
else()
    # When downloaded via FetchContent, dependencies are handled automatically
    target_link_libraries(os_project raylib)
endif()