cmake_minimum_required(VERSION 3.9)

include("cmake/HunterGate.cmake")
HunterGate(
        URL "https://github.com/ruslo/hunter/archive/v0.20.13.tar.gz"
        SHA1 "d92042d6e22522516183b86da10e56fb1af73f7a"
)

project(sqlite_server)
set(CMAKE_CXX_STANDARD 14)

hunter_add_package(Boost COMPONENTS system regex filesystem thread)
find_package(Boost CONFIG REQUIRED system regex filesystem thread)

hunter_add_package(nlohmann_json)
find_package(nlohmann_json CONFIG REQUIRED)

hunter_add_package(fmt)
find_package(fmt CONFIG REQUIRED)

add_executable(sqlite_server
        main.cpp
        ListenSocket.h
        Network.h
        Socket.h
        SQLiteSocket.cpp
        SQLiteSocket.h
        sqlite3/sqlite3.c
        sqlite3/sqlite3.h
        sqlite3/sqlite3ext.h
        RequestHandler.cpp
        RequestHandler.h
        Response.cpp
        Response.h
		IResponse.h
		IRequestHandler.h
		sqlite3_wrapper/SQLDatabase.h
		sqlite3_wrapper/SQLException.h
		sqlite3_wrapper/SQLStatement.h)
target_link_libraries(sqlite_server PUBLIC nlohmann_json fmt::fmt-header-only Boost::system Boost::regex Boost::filesystem Boost::thread)
if(WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
    target_link_libraries(sqlite_server PUBLIC wsock32 ws2_32)
endif()