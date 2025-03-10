cmake_minimum_required (VERSION 3.8)

project ("BimmerDis")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_executable (${PROJECT_NAME} "src/BimmerDis.cpp")

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)