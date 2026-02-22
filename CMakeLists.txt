cmake_minimum_required(VERSION 4.1)
project(Jit C)

set(CMAKE_C_STANDARD 23)

add_executable(jit main.c
        init.c
        init.h)
