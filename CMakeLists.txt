cmake_minimum_required(VERSION 3.13)
project(redis-starter-cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src)

set(SOURCE_FILES
    src/main.cpp
    src/utils/utils.cpp
    src/protocol/parser.cpp
    src/db/database.cpp
    src/server/server.cpp
    src/server/client.cpp
    src/commands/dispatcher.cpp
    src/commands/cmd_admin.cpp
    src/commands/cmd_strings.cpp
    src/commands/cmd_lists.cpp
    src/commands/cmd_zset.cpp
    src/commands/cmd_geo.cpp
    src/commands/cmd_tx.cpp
)

find_package(Threads REQUIRED)

add_executable(redis ${SOURCE_FILES})
target_link_libraries(redis PRIVATE Threads::Threads)