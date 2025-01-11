cmake_minimum_required(VERSION 3.27)
project(dictMaker)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE+CXX_FLAGS} -fsanitize=address -g")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)

set(SOURCES src/main.cpp src/data.cpp CppUtil/util.cpp)

add_executable(dictMaker ${SOURCES})

include_directories(${SQLite3_INCLUDE_DIRS})

target_link_libraries(dictMaker ${SQLite3_LIBRARIES} ${CURL_LIBRARIES})
