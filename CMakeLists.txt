cmake_minimum_required(VERSION 3.12)

project(pedeps)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(pedeps STATIC
        lib/pedeps.c
        lib/pedeps.h
        lib/pedeps_version.h
        lib/pestructs.c
        lib/pestructs.h
        )

target_compile_definitions(pedeps PUBLIC BUILD_PEDEPS_STATIC)
target_include_directories(pedeps PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib)


add_executable(listpedeps src/listpedeps.c)
target_link_libraries(listpedeps PRIVATE pedeps)

find_library(AVTLIB avt)
if (${AVTLIB})
    add_executable(copypedeps src/copypedeps.c)
    target_link_libraries(copypedeps PRIVATE pedeps ${AVTLIB} )
endif ()

add_executable(copypedepscpp src/copypedeps.cpp)
target_link_libraries(copypedepscpp PRIVATE pedeps Shlwapi)
target_compile_options(copypedepscpp PRIVATE /Zc:__cplusplus)
