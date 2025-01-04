cmake_minimum_required(VERSION 3.27)
project(cMakeTemplate)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE+CXX_FLAGS} -fsanitize=address -g")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# find_package(somePackage REQUIRED)

set(SOURCES main.cpp)

add_executable(cMakeTemplate ${SOURCES})
