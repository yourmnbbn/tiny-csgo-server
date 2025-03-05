cmake_minimum_required(VERSION 3.23)
project(tiny-goldsrc-server)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${HLSKD}/common
    ${HLSKD}/engine
    ${HLSKD}/public
    ${ASIO-SRC}/include
)

link_directories(${HLSKD}/lib/public)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/bitbuf BITBUF_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/netmessage NETMSG_SRC)

set(SRC_LIST sv-main.cpp ${NETMSG_SRC} ${BITBUF_SRC})

if(MSVC)
    add_definitions(
        -D _WIN32_WINNT=0x0601
        -D _CRT_SECURE_NO_WARNINGS
        -D _GENERIC
        -D COMPILER_MSVC
        -D COMPILER_MSVC32
        -D _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
    )
    set(STATIC_LIB_LIST 
        steam_api.lib
        libprotobuf.lib
    )
    link_directories(${HLSKD}/lib/win32/release/vs2017)
    add_compile_options(/MT)
    add_compile_options(/wd4819)
elseif(("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
    add_definitions(
        -D GNUC
        -D POSIX
    )
    set(STATIC_LIB_LIST 
        steam_api
        protobuf
    )
    link_directories(${HLSKD}/lib/linux)
endif()

add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} ${STATIC_LIB_LIST})
