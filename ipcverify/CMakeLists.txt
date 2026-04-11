cmake_minimum_required(VERSION 3.22.1)
project(ipcverify C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(ipcverify SHARED
    app/src/main/cpp/android_bridge.c
    src/ipc_checks.c
)

target_include_directories(ipcverify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../binder
    ${CMAKE_CURRENT_SOURCE_DIR}/../ashmem
)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(ipcverify ${log-lib} ${android-lib})
