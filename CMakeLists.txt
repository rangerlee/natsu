cmake_minimum_required(VERSION 2.8)

###################################################################################
project(libnatsu)

if (CMAKE_BUILD_TYPE)
else()
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

message("  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

set(USE_BOOST_COROUTINE 0)
set(USE_UCONTEXT 1)
set(USE_FIBER 0)

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

configure_file(${PROJECT_SOURCE_DIR}/libgo/cmake_config.h.in ${PROJECT_SOURCE_DIR}/libgo/cmake_config.h)
message("----------------------------------")

message("-------------- Env ---------------")
message("  CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message("  CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message("----------------------------------")

# unix like platforms
aux_source_directory(${PROJECT_SOURCE_DIR}/libgo CO_SRC_LIST)
include_directories(${PROJECT_SOURCE_DIR}/libgo)
aux_source_directory(${PROJECT_SOURCE_DIR}/libgo/linux CO_SRC_LIST)
include_directories(${PROJECT_SOURCE_DIR}/libgo/linux)
aux_source_directory(${PROJECT_SOURCE_DIR}/libgo/ctx_ucontext CO_SRC_LIST)
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/inc)
aux_source_directory(${PROJECT_SOURCE_DIR}/natsu CO_SRC_LIST)
include_directories(${PROJECT_SOURCE_DIR}/hiredis)
if (DISABLE_HOOK)
    list(REMOVE_ITEM CO_SRC_LIST ${PROJECT_SOURCE_DIR}/libgo/linux/linux_glibc_hook.cpp)
    aux_source_directory(${PROJECT_SOURCE_DIR}/libgo/disable_hook CO_SRC_LIST)
endif()
set(CMAKE_CXX_FLAGS "-std=c++11 -fPIC -Wall -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "-g -pg ${CMAKE_CXX_FLAGS} -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O3 ${CMAKE_CXX_FLAGS} -Werror")

message("------------ Cxx flags -------------")
message("  CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}: ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")
message("------------------------------------")

set(TARGET "natsu")
set(SHARED_T "libnatsu")
add_library("${SHARED_T}" SHARED ${CO_SRC_LIST})
set_target_properties("${SHARED_T}" PROPERTIES COMPILE_FLAGS "-DCO_DYNAMIC_LINK")
set_target_properties("${SHARED_T}" PROPERTIES OUTPUT_NAME "${TARGET}")
target_link_libraries("${SHARED_T}" ${LINK_LIBS} -ldl)
install(TARGETS ${SHARED_T} LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")

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


