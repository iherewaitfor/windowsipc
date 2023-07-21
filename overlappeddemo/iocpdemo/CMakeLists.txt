#Cmake 最低版本号要求
cmake_minimum_required(VERSION 3.10)

# 项目信息
project(iocpdemo)

# 设置子系统为窗口程序
set ( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS" ) 

#项目使用UNICODE版本。win中的各种API将使用UNICODE版本。
add_definitions("-DUNICODE" "-D_UNICODE")

# 查找目录下的所有源文件
# 并将名称保存到DIR_SRCS变量
aux_source_directory(. DIR_SRCS)

set(RESOURCES_SRCS
FileCopy.ico
FileCopy.rc
Resource.h
)
source_group("resources" FILES ${RESOURCES_SRCS})


message(NOTICE "PROJECT_NAME is ${PROJECT_NAME}" )
# 指定生成目标
add_executable(${PROJECT_NAME} ${DIR_SRCS} ${RESOURCES_SRCS})
set_target_properties(${PROJECT_NAME} PROPERTIES
					ARCHIVE_OUTPUT_DIRECTORY "${TARGET_OUT_DIR}"
					RUNTIME_OUTPUT_DIRECTORY "${TARGET_OUT_DIR}"
					LIBRARY_OUTPUT_DIRECTORY "${TARGET_OUT_DIR}")