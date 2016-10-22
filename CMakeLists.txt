cmake_minimum_required(VERSION 3.6)
project(Prj)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

set(SOURCE_FILES tcp_server.cpp tcp_server.h connection.cpp
        connection.h global_definitions.h tcp_client.cpp tcp_client.h main_server.cpp buffer.h pipe_buffer.h pipe_create_server_connections.cpp pipe_create_server_connections.h)

add_executable(Prj ${SOURCE_FILES})