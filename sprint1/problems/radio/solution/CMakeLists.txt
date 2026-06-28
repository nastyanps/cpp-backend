cmake_minimum_required(VERSION 3.11)
project(Radio CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# ВАЖНО: Для Windows определяем макросы ДО компиляции
if(WIN32)
    add_definitions(-DNOMINMAX -D_WIN32_WINNT=0x0A00)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(radio src/main.cpp)

# Добавляем папку src в пути поиска заголовков
target_include_directories(radio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(radio PRIVATE Threads::Threads)

# Для Windows подключаем библиотеку сокетов
if(WIN32)
    target_link_libraries(radio PRIVATE ws2_32)
endif()