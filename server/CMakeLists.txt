cmake_minimum_required(VERSION 3.16)
project(WerewolfServer VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

file(GLOB SERVER_CORE_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(REMOVE_ITEM SERVER_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(werewolf_server_core STATIC ${SERVER_CORE_SOURCES})
target_include_directories(werewolf_server_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(werewolf_server_core PUBLIC Threads::Threads)

add_executable(server src/main.cpp)
target_link_libraries(server PRIVATE werewolf_server_core)

if(MSVC)
    target_compile_options(werewolf_server_core PRIVATE /W4 /utf-8)
    target_compile_options(server PRIVATE /W4 /utf-8)
else()
    target_compile_options(werewolf_server_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_link_libraries(werewolf_server_core PUBLIC ws2_32)
    target_compile_definitions(werewolf_server_core PUBLIC
        _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

option(BUILD_LEGACY_CLI_CLIENT "Build the original command-line test client" OFF)
if(BUILD_LEGACY_CLI_CLIENT)
    add_executable(legacy_client client.cpp)
    target_link_libraries(legacy_client PRIVATE Threads::Threads)
    if(WIN32)
        target_link_libraries(legacy_client PRIVATE ws2_32)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(room_manager_tests tests/room_manager_tests.cpp)
    target_link_libraries(room_manager_tests PRIVATE werewolf_server_core)
    add_test(NAME room_manager_tests COMMAND room_manager_tests)
endif()
