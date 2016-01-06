cmake_minimum_required(VERSION 2.8)
project(aucont)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES_START aucont_start.cpp common.h)
set(SOURCE_FILES_STOP aucont_stop.cpp common.h)
set(SOURCE_FILES_LIST aucont_list.cpp common.h)
set(SOURCE_FILES_EXEC aucont_exec.cpp common.h)
add_executable(aucont_start ${SOURCE_FILES_START})
add_executable(aucont_stop ${SOURCE_FILES_STOP})
add_executable(aucont_list ${SOURCE_FILES_LIST})
add_executable(aucont_exec ${SOURCE_FILES_EXEC})