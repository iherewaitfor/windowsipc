#Cmake 最低版本号要求
cmake_minimum_required(VERSION 3.10)

# 项目信息
project (OverlappedDemo)
#项目使用UNICODE版本。win中的各种API将使用UNICODE版本。
#add_definitions("-DUNICODE" "-D_UNICODE")


set(TARGET_OUT_DIR "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

add_subdirectory(fileoverlapped)
add_subdirectory(filepublicoverlapped)
add_subdirectory(servernamedpipe)
add_subdirectory(clientnamedpipe)
add_subdirectory(iocpdemo)
add_subdirectory(servernamedpipeiocp)
add_subdirectory(servernamedpipeiocpmulthread)
