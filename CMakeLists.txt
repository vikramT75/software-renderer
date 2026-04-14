cmake_minimum_required(VERSION 3.16)
project(swr VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(SDL2 REQUIRED)

set(SWR_SOURCES
    src/main.cpp
    src/loaders/stb_image.cpp
)

add_executable(swr ${SWR_SOURCES})

target_include_directories(swr PRIVATE src)

if (TARGET SDL2::SDL2)
    target_link_libraries(swr PRIVATE SDL2::SDL2main SDL2::SDL2)
elseif (TARGET SDL2::SDL2-static)
    target_link_libraries(swr PRIVATE SDL2::SDL2main SDL2::SDL2-static)
else()
    target_include_directories(swr PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(swr PRIVATE ${SDL2_LIBRARIES})
endif()

install(TARGETS swr DESTINATION bin)