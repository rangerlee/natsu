cmake_minimum_required(VERSION 2.8)

###################################################################################
project(libnatsu)

if (CMAKE_BUILD_TYPE)
else()
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

message("------------ Options -------------")
message("  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

aux_source_directory(libnatsu CO_SRC_LIST)


set(USE_BOOST_COROUTINE 0)
if (UNIX)
    set(USE_UCONTEXT 1)
    set(USE_FIBER 0)
else()
    set(USE_UCONTEXT 0)
    set(USE_FIBER 1)
endif()


option(ENABLE_DEBUGGER "enable debugger" OFF)
if (ENABLE_DEBUGGER)
    set(ENABLE_DEBUGGER 1)
    message ("  ENABLE_DEBUGGER: ON")
else()
    set(ENABLE_DEBUGGER 0)
    message ("  ENABLE_DEBUGGER: OFF")
endif()

if (DISABLE_HOOK)
    message ("  DISABLE_HOOK: ON")
else()
    message ("  DISABLE_HOOK: OFF")
endif()

if (DISABLE_DYNAMIC_LIB)
    message ("  DISABLE_DYNAMIC_LIB: ON")
else()
    message ("  DISABLE_DYNAMIC_LIB: OFF")
endif()

configure_file(${PROJECT_SOURCE_DIR}/libgo/cmake_config.h.in ${PROJECT_SOURCE_DIR}/libgo/cmake_config.h)
message("----------------------------------")

message("-------------- Env ---------------")
message("  CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message("  CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message("----------------------------------")

# 选项互斥检查
if (UNIX)
    # unix like platforms
    aux_source_directory(${PROJECT_SOURCE_DIR}/libgo/linux CO_SRC_LIST)
    include_directories(${PROJECT_SOURCE_DIR}/libgo/linux)

    aux_source_directory(${PROJECT_SOURCE_DIR}/libgo/ctx_ucontext CO_SRC_LIST)

    if (DISABLE_HOOK)
        list(REMOVE_ITEM CO_SRC_LIST ${PROJECT_SOURCE_DIR}/libgo/linux/linux_glibc_hook.cpp)
        aux_source_directory(${PROJECT_SOURCE_DIR}/libgo/disable_hook CO_SRC_LIST)
    endif()

elseif (WIN32)
    # windows platform
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    include_directories(${PROJECT_SOURCE_DIR}/libgo/windows)
    aux_source_directory(${PROJECT_SOURCE_DIR}/libgo/windows CO_SRC_LIST)
    aux_source_directory(${PROJECT_SOURCE_DIR}/libgo/windows/arpa CO_SRC_LIST)
    aux_source_directory(${PROJECT_SOURCE_DIR}/libgo/windows/sys CO_SRC_LIST)

    aux_source_directory(${PROJECT_SOURCE_DIR}/libgo/ctx_win_fiber CO_SRC_LIST)

    if (DISABLE_HOOK)
        list(REMOVE_ITEM CO_SRC_LIST ${PROJECT_SOURCE_DIR}/libgo/windows/win_vc_hook.cpp)
        aux_source_directory(${PROJECT_SOURCE_DIR}/libgo/disable_hook CO_SRC_LIST)
    else ()
        list(APPEND CO_SRC_LIST "${PROJECT_SOURCE_DIR}/xhook/src/xhook.cpp")
        include_directories(${PROJECT_SOURCE_DIR}/xhook/src)
    endif()

else ()
    # other platforms
    message(FATAL_ERROR "Not support current platform!")
endif()

if (UNIX)
    set(CMAKE_CXX_FLAGS "-std=c++11 -fPIC -Wall -pthread")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -pg ${CMAKE_CXX_FLAGS} -Werror")
    set(CMAKE_CXX_FLAGS_RELEASE "-g -O3 ${CMAKE_CXX_FLAGS} -Werror")
else ()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
endif()
message("------------ Cxx flags -------------")
message("  CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}: ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")
message("------------------------------------")

include_directories(libgo)

set(TARGET "libnatsu")
set(STATIC_T "libnatsu_static")

add_library("${STATIC_T}" STATIC ${CO_SRC_LIST})
set_target_properties("${STATIC_T}" PROPERTIES OUTPUT_NAME "${TARGET}")

if (UNIX)
    #add_subdirectory(${PROJECT_SOURCE_DIR}/libgo/main)

    if (NOT DISABLE_DYNAMIC_LIB)
        set(SHARED_T "libnatsu_dynamic")
        add_library("${SHARED_T}" SHARED ${CO_SRC_LIST})
        set_target_properties("${SHARED_T}" PROPERTIES COMPILE_FLAGS "-DCO_DYNAMIC_LINK")
        set_target_properties("${SHARED_T}" PROPERTIES OUTPUT_NAME "${TARGET}")
        target_link_libraries("${SHARED_T}" ${LINK_LIBS} -ldl)
        install(TARGETS ${SHARED_T} LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")
    endif()

    add_custom_target(debug
        COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=DEBUG ${CMAKE_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Switch CMAKE_BUILD_TYPE to Debug"
    )

    add_custom_target(release
        COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=RELEASE ${CMAKE_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Switch CMAKE_BUILD_TYPE to Release"
    )

else ()
    #add_subdirectory(${PROJECT_SOURCE_DIR}/libgo/main)
    set_target_properties("${STATIC_T}" PROPERTIES COMPILE_FLAGS "/wd4819 /wd4267")
endif()

include_directories(inc)
include_directories(natsu)
