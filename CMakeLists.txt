cmake_minimum_required(VERSION 3.4.0)

project(migrate)

set(build_type "Release")

if(DEBUG STREQUAL "true")
    message(NOTICE "-- Open debug mod")
    set(build_type "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
    set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
endif()

set(CMAKE_BUILD_TYPE ${build_type})

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_DEBUG_POSTFIX d)

set(migrate_sources 
    src/hiredis/alloc.c
    src/hiredis/async.c
    src/hiredis/dict.c
    src/hiredis/hiredis.c
    src/hiredis/net.c
    src/hiredis/read.c
    src/hiredis/sds.c
    src/hiredis/sockcompat.c
    src/log.cpp
    src/thread_pool.cpp
    src/redis_migrate.cpp)

set(migrate_sources ${migrate_sources})

add_library(migrate SHARED ${migrate_sources})


