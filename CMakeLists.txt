cmake_minimum_required(VERSION 3.11)

include(FetchContent)
project(sqlite3-server)

set(CMAKE_CXX_STANDARD 17)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)

# Boost
set(Boost_USE_MULTITHREADED ON)
set(BOOST_INCLUDE_LIBRARIES asio system regex filesystem thread program_options)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(
        Boost
        GIT_REPOSITORY https://github.com/boostorg/boost
        GIT_TAG boost-1.91.0-1
)
FetchContent_MakeAvailable(Boost)

# JSON parser
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json
        GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(json)

# fmt lib
FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt
        GIT_TAG 12.1.0
)
FetchContent_MakeAvailable(fmt)

# sqlite3 compile options
# https://sqlite.org/compile.html
set(SQLITE_COMPILE_OPTIONS
        SQLITE_THREADSAFE=1
        SQLITE_DEFAULT_MEMSTATUS=0
        SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
        SQLITE_LIKE_DOESNT_MATCH_BLOBS
        SQLITE_MAX_EXPR_DEPTH=0
        SQLITE_OMIT_DECLTYPE
        SQLITE_OMIT_DEPRECATED
        SQLITE_OMIT_PROGRESS_CALLBACK
        SQLITE_OMIT_SHARED_CACHE
        SQLITE_USE_ALLOCA)

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

if (ipo_supported)
    set_target_properties(sqlite3-server PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

target_compile_definitions(sqlite3-server PRIVATE ${SQLITE_COMPILE_OPTIONS})

target_link_libraries(sqlite3-server PRIVATE
        Boost::asio
        Boost::system
        Boost::regex
        Boost::filesystem
        Boost::thread
        Boost::program_options
        nlohmann_json::nlohmann_json
        fmt::fmt-header-only
        ${CMAKE_DL_LIBS})

if (WIN32)
    target_compile_definitions(sqlite3-server PRIVATE WIN32_LEAN_AND_MEAN)
    target_link_libraries(sqlite3-server PRIVATE wsock32 ws2_32)
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
target_compile_definitions(sqlite3-server PRIVATE
        SQLITE_SERVER_GIT_COMMIT_HASH="${GITHUB_COMMIT_HASH}"
        SQLITE_SERVER_GIT_BRANCH="${GITHUB_BRANCH}")
