# the minimum version of CMake.
cmake_minimum_required(VERSION 3.4.1)
project(NativeGlue)

set(UE_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/arm64-v8a)

add_library(${PROJECT_NAME} SHARED main.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC libace_napi.z.so)
target_link_libraries(${PROJECT_NAME} PUBLIC ${UE_LIB_PATH}/libUnreal.so)


