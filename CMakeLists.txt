cmake_minimum_required(VERSION 3.20)
project(InterPlayAcmCodec CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    get_filename_component(MSVC_COMPAT_H "${CMAKE_CURRENT_SOURCE_DIR}/msvc-compat.h" ABSOLUTE)
    add_compile_options(/FI${MSVC_COMPAT_H})

    add_compile_options(/wd4996 /wd4244 /wd4267 /wd4018)

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /GL)
        add_link_options(/LTCG)
    endif()
endif()

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(acmtool
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/snd2acm.cpp
    ${SRC_DIR}/acm2snd.cpp
    ${SRC_DIR}/subband.cpp
    ${SRC_DIR}/packer.cpp
    ${SRC_DIR}/bitstream.cpp
    ${SRC_DIR}/utils.cpp
    ${SRC_DIR}/readers.cpp
    ${SRC_DIR}/riffhdr.cpp
    ${SRC_DIR}/unpacker.cpp
    ${SRC_DIR}/decoder.cpp
    ${SRC_DIR}/portable-utils.cpp
)

target_include_directories(acmtool PRIVATE ${SRC_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(acmtool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin
)

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "" FORCE)

include(GNUInstallDirs)

file(GLOB INSTALL_HEADERS
    ${SRC_DIR}/*.h
    ${SRC_DIR}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

install(FILES ${INSTALL_HEADERS} DESTINATION include)