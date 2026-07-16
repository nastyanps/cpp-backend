cmake_minimum_required(VERSION 3.11)
project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Статическая библиотека с игровой моделью (доменный слой).
# Не содержит сетевого кода, HTTP-обработчиков и т.п. - только то,
# что тестируется юнит-тестами Catch2.
add_library(game_model_lib STATIC
    src/model.h
    src/model.cpp
    src/loot_generator.h
    src/loot_generator.cpp
    src/tagged.h
    src/collision_detector.h
    src/collision_detector.cpp
    src/geom.h
)

target_link_libraries(game_model_lib PUBLIC CONAN_PKG::boost)

add_executable(game_server
        src/main.cpp
        src/http_server.cpp
        src/http_server.h
        src/sdk.h
        src/boost_json.cpp
        src/json_loader.h
        src/json_loader.cpp
        src/request_handler.cpp
        src/request_handler.h
        src/api_handler.h
        src/api_handler.cpp
        src/application.h
        src/players.h
        src/logger.h
        src/logging_request_handler.h
        src/ticker.h
        src/extra_data.h
	src/model_serialization.h
        src/application_listener.h
        src/serializing_listener.h
)

target_link_libraries(game_server PRIVATE game_model_lib Threads::Threads CONAN_PKG::boost)

add_executable(game_server_tests
        tests/main.cpp
        tests/model-tests.cpp
        tests/loot_generator_tests.cpp
)

target_link_libraries(game_server_tests PRIVATE
    game_model_lib
    CONAN_LIB::catch2_Catch2Main
    CONAN_LIB::catch2_Catch2
    CONAN_PKG::catch2
    CONAN_PKG::boost
    Threads::Threads
)
