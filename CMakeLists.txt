cmake_minimum_required(VERSION 3.30)
project(pop C)

set(CMAKE_C_STANDARD 11)

# Directorios de cabeceras
include_directories(include)

# Fuentes
set(SOURCES
        server.c
        server/selector.c
        server/stm.c
        utils/args.c
        utils/buffer.c
)
add_executable(main ${SOURCES})
add_executable(pop server.c)
