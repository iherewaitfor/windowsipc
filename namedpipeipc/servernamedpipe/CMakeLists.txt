#Cmake 最低版本号要求
cmake_minimum_required(VERSION 3.10)

# 项目信息
project(servernamedpipe)

# 查找目录下的所有源文件
# 并将名称保存到DIR_SRCS变量
aux_source_directory(. DIR_SRCS)
aux_source_directory(../shared SHARED_SRCS)
include_directories(../shared)

message(NOTICE "PROJECT_NAME is ${PROJECT_NAME}")
# 指定生成目标
add_executable(${PROJECT_NAME} ${DIR_SRCS} ${SHARED_SRCS})
set_target_properties(${PROJECT_NAME} PROPERTIES
					ARCHIVE_OUTPUT_DIRECTORY "${TARGET_OUT_DIR}"
					RUNTIME_OUTPUT_DIRECTORY "${TARGET_OUT_DIR}"
					LIBRARY_OUTPUT_DIRECTORY "${TARGET_OUT_DIR}")