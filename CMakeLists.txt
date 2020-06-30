cmake_minimum_required(VERSION 3.1)

option(COMPILE_TESTS "Should we compile tests while building this project?" ON)
option(COMPILE_EXAMPLES "Should we compile the examples?" ON)

project(bounded-poly VERSION 0.1.0)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)

# warning level 4 and all warnings as errors
if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

add_library(bounded-poly INTERFACE)
target_include_directories(bounded-poly INTERFACE src)


if (COMPILE_TESTS)
    add_subdirectory(tests)
endif()

if (COMPILE_EXAMPLES)
    add_subdirectory(examples)
endif()
