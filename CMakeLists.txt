cmake_minimum_required(VERSION 3.24)
project(BuildYourOwnLisp C)

set(CMAKE_C_STANDARD 23)

add_executable(BuildYourOwnLisp parsing.c mpc.c mpc.h)
