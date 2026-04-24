cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
project(aplusb CXX)

set(CMAKE_CXX_STANDARD 20)

if(WIN32)
    # Убираем расширение .exe
    set(CMAKE_EXECUTABLE_SUFFIX "")
    # Кладем исполняемый файл прямо в build/, а не в подпапку Debug/
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}")
endif()

add_executable(
  sum main.cpp
)
