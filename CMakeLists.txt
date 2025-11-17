cmake_minimum_required(VERSION 3.25)
project(CCThreadPool LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message("============= Configuring The Thread Pool =============")
message("============= Configuring the library =============")
add_library(    CCXXThreadPool 
                CCThreadPool/CCThreadPool.h
                src/CCThreadPool_configure.cc 
                src/CCThreadPool.cc)
# Include the request folder
target_include_directories(CCXXThreadPool PUBLIC CCThreadPool)
message("============= Configuring the library Done =============")
message("============= Configuring the test =============")
add_subdirectory(test)
message("============= Configuring the test Done =============")
message("============= Configuring The Thread Pool Done =============")





