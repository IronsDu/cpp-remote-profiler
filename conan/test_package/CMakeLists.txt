cmake_minimum_required(VERSION 3.15)
project(test_package LANGUAGES CXX)

find_package(cpp-remote-profiler REQUIRED)

add_executable(test_package main.cpp)

target_link_libraries(test_package cpp-remote-profiler::cpp-remote-profiler)
