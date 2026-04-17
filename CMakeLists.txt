cmake_minimum_required(VERSION 3.10)

project(mini_muduo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -g)
add_link_options(-rdynamic)

find_package(Threads REQUIRED)

file(GLOB SRC_FILES CONFIGURE_DEPENDS 
    ${PROJECT_SOURCE_DIR}/src/*.cc
)

add_library(mini_muduo STATIC ${SRC_FILES})

target_include_directories(mini_muduo PUBLIC 
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(mini_muduo PUBLIC
    Threads::Threads
)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE mini_muduo)

file(GLOB TEST_FILES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/tests/*.cpp
)

foreach(test_file ${TEST_FILES})
    get_filename_component(test_name ${test_file} NAME_WE)

    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE mini_muduo)

    list(APPEND ALL_TEST_TARGETS ${test_name})
endforeach()


add_custom_target(tests DEPENDS ${ALL_TEST_TARGETS})
