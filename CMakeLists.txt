cmake_minimum_required(VERSION 3.9)

include("cmake/HunterGate.cmake")
HunterGate(
        URL "https://github.com/ruslo/hunter/archive/v0.23.80.tar.gz"
        SHA1 "3bfa8be33ac3bdac570312ca1ece33059cc71c20"
)
project(sqlite3-server)

set(CMAKE_CXX_STANDARD 14)
set(Boost_USE_MULTITHREADED ON)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if (ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

# Boost
hunter_add_package(Boost COMPONENTS system regex filesystem thread program_options)
find_package(Boost CONFIG REQUIRED system regex filesystem thread program_options)

# JSON parser
hunter_add_package(nlohmann_json)
find_package(nlohmann_json CONFIG REQUIRED)

# fmt lib
hunter_add_package(fmt)
find_package(fmt CONFIG REQUIRED)

# sqlite3 compile options
# https://sqlite.org/compile.html
add_definitions(
        -DSQLITE_THREADSAFE=1
        -DSQLITE_DEFAULT_MEMSTATUS=0
        -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1
        -DSQLITE_LIKE_DOESNT_MATCH_BLOBS
        -DSQLITE_MAX_EXPR_DEPTH=0
        -DSQLITE_OMIT_DECLTYPE
        -DSQLITE_OMIT_DEPRECATED
        -DSQLITE_OMIT_PROGRESS_CALLBACK
        -DSQLITE_OMIT_SHARED_CACHE
        -DSQLITE_USE_ALLOCA)

add_executable(sqlite3-server
        sqlite3/sqlite3.c
        sqlite3/sqlite3.h
        sqlite3/sqlite3ext.h
        main.cpp
        ListenSocket.h
        Network.h
        Socket.cpp
        Socket.h
        SQLiteSocket.cpp
        SQLiteSocket.h
        RequestHandler.cpp
        RequestHandler.h
        Response.cpp
        Response.h
        IResponse.h
        IRequestHandler.h
        sqlite3_wrapper/SQLDatabase.h
        sqlite3_wrapper/SQLException.h
        sqlite3_wrapper/SQLStatement.h
        Config.cpp
        Config.h
        Logger.h)
target_link_libraries(sqlite3-server PUBLIC
        Boost::system
        Boost::regex
        Boost::filesystem
        Boost::thread
        Boost::program_options
        nlohmann_json::nlohmann_json
        fmt::fmt-header-only
        ${CMAKE_DL_LIBS})
if (WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    target_link_libraries(sqlite3-server PUBLIC wsock32 ws2_32)
endif ()

# Get the current working branch
execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GITHUB_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GITHUB_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_definitions("-DSQLITE_SERVER_GIT_COMMIT_HASH=\"${GITHUB_COMMIT_HASH}\"")
add_definitions("-DSQLITE_SERVER_GIT_BRANCH=\"${GITHUB_BRANCH}\"")
