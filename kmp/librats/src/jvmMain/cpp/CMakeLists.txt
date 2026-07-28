cmake_minimum_required(VERSION 3.22.1)

project("librats_jvm")

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Locate standard JVM headers (jni.h) and Threading library
find_package(JNI REQUIRED)
message(STATUS "=== JNI Detection Check ===")
message(STATUS "JNI Found:          ${JNI_FOUND}")
message(STATUS "JNI Include Dirs:   ${JNI_INCLUDE_DIRS}")
message(STATUS "jni.h Location:     ${JAVA_INCLUDE_PATH}")
message(STATUS "jni_md.h Location:  ${JAVA_INCLUDE_PATH2}")
message(STATUS "===========================")

find_package(Threads REQUIRED)

# Build the core LibRats library by pulling in the repository-root CMake. That
# CMakeLists defines LIBRARY_SOURCES (core, util, wire, transport, peer,
# security, node, subsystems, dht, mdns, crypto, nat) plus the C ABI under
# src/bindings/rats.{h,cpp} when RATS_BINDINGS is ON, and configures the
# generated version.h. We do not duplicate the source list here.
set(LIBRATS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../)
set(LIBRATS_BINARY_DIR ${CMAKE_BINARY_DIR}/librats_native)

if(EXISTS "${LIBRATS_SOURCE_DIR}/CMakeLists.txt")
    # Build options suitable for desktop/server JVM environment.
    set(RATS_BUILD_CLIENT OFF CACHE BOOL "Disable the rats-client executable" FORCE)
    set(RATS_BUILD_EXAMPLES OFF CACHE BOOL "Disable examples" FORCE)
    set(RATS_BUILD_TESTS OFF CACHE BOOL "Disable tests" FORCE)
    set(RATS_CROSSCOMPILING OFF CACHE BOOL "Standard JVM build" FORCE)
    set(RATS_BINDINGS ON CACHE BOOL "Enable the C ABI (src/bindings/rats.cpp) for JNI" FORCE)
    set(RATS_INSTALL OFF CACHE BOOL "Skip install rules" FORCE)

    add_subdirectory(${LIBRATS_SOURCE_DIR} ${LIBRATS_BINARY_DIR})

    if(NOT TARGET rats)
        message(FATAL_ERROR "LibRats target 'rats' was not created by root CMakeLists.txt")
    endif()

    # JNI bridge to the C ABI.
    add_library(rats_jni SHARED librats_jni.cpp)

    # Public headers (src/) and the JNI headers provided by Java SDK
    target_include_directories(rats_jni PRIVATE
        ${LIBRATS_SOURCE_DIR}/src
        ${LIBRATS_BINARY_DIR}/src
        ${JNI_INCLUDE_DIRS}
    )

    # Link the JNI bridge against the core library and standard JNI libraries.
    target_link_libraries(rats_jni PRIVATE
        rats
        JNI::JNI
        Threads::Threads
    )

    target_compile_options(rats_jni PRIVATE
        -fvisibility=default
        -fPIC
        -O2
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )

    set_target_properties(rats_jni PROPERTIES
        LINK_FLAGS "-Wl,--export-dynamic"
    )
else()
    message(FATAL_ERROR "LibRats root CMakeLists.txt not found at ${LIBRATS_SOURCE_DIR}.")
endif()