cmake_minimum_required(VERSION 3.12)

project(web)

include_directories(include)

add_executable(client src/client.cpp)

add_executable(server src/server.cpp)
