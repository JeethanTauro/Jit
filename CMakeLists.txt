cmake_minimum_required(VERSION 4.1)
project(Jit C)

set(CMAKE_C_STANDARD 23)

add_executable(jit main.c
        init.c
        init.h
        add.c
        add.h
        commit.c
        commit.h
        status.c
        status.h
        log.c
        log.h
        unstage.c
        unstage.h
        branch.c
        branch.h
        delete_branch.c
        delete_branch.h
        switch_branch.c
        switch_branch.h
        check_branch.c
        check_branch.h
)
