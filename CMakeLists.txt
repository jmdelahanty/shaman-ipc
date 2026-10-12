cmake_minimum_required(VERSION 3.16)
project(shaman_ipc VERSION 4.0.0 LANGUAGES CXX)

add_library(shaman_ipc INTERFACE)
add_library(shaman_ipc::shaman_ipc ALIAS shaman_ipc)
target_include_directories(shaman_ipc INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(shaman_ipc INTERFACE cxx_std_17)

if(PROJECT_IS_TOP_LEVEL)
    enable_testing()
    add_executable(shaman_ipc_layout_test tests/layout_test.cpp)
    target_link_libraries(shaman_ipc_layout_test PRIVATE shaman_ipc)
    add_test(NAME shaman_ipc_layout_test COMMAND shaman_ipc_layout_test)
endif()
