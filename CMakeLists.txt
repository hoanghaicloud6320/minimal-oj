cmake_minimum_required(VERSION 3.22)

project(minimal_oj LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Drogon CONFIG REQUIRED)

add_executable(minimal_oj
    src/JudgeService.cpp
    src/ProblemRepository.cpp
    src/ProblemService.cpp
    src/Server.cpp
    src/main.cpp
)

target_include_directories(minimal_oj PRIVATE include)
target_link_libraries(minimal_oj PRIVATE Drogon::Drogon SQLite3_lib)

if(MSVC)
    target_compile_options(minimal_oj PRIVATE /W4)
else()
    target_compile_options(minimal_oj PRIVATE -Wall -Wextra -Wpedantic)
endif()
