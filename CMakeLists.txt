cmake_minimum_required(VERSION 3.12)

project(pedeps)

add_library(pedeps STATIC
        lib/pedeps.c
        lib/pedeps.h
        lib/pedeps_version.h
        lib/pestructs.c
        lib/pestructs.h
        )

target_include_directories(pedeps PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_executable(listpedeps src/listpedeps.c)
target_link_libraries(listpedeps PRIVATE pedeps)

add_executable(copypedeps src/copypedeps.cpp)
target_link_libraries(copypedeps PRIVATE pedeps)
