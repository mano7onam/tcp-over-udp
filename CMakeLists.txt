cmake_minimum_required(VERSION 3.6)
project(Prj)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

set(SOURCE_FILES tcp_server.cpp tcp_server.h connection.cpp
        connection.h global_definitions.h tcp_client.cpp tcp_client.h
        buffer.h pipe_buffer.h connections_creator.cpp connections_creator.h
        test_file_transfer/server.cpp)

add_executable(Prj ${SOURCE_FILES})