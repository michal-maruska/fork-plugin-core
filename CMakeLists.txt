# when this is the top project, we do more
# otherwise just define the ... library
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(FORKING_MACHINE_IS_TOP_LEVEL TRUE)
else()
    set(FORKING_MACHINE_IS_TOP_LEVEL FALSE)
endif()


cmake_minimum_required(VERSION 3.13)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(CMAKE_COLOR_DIAGNOSTICS ON)
project(fork-plugin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
include_directories(include/)

message(WARNING "######## test color output")

# option(WITH_LIBINPUT "Build plugin for libinput." ON)
# option(WITH_XORG "Build plugin for Xorg server" ON)
# /usr/share/cmake-3.30/Modules/CMakeDependentOption.cmake

# https://medium.com/@alasher/colored-c-compiler-output-with-ninja-clang-gcc-10bfe7f2b949
# force for Ninja:
option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." FALSE)
if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options(-fdiagnostics-color=always)
       add_link_options(-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       add_compile_options(-fcolor-diagnostics -ferror-limit=2)
       add_link_options(-fcolor-diagnostics)
    else()
      error("unknown compiler")
    endif ()
endif ()
#end

# why this? it builds everything with -lgcov !!
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|Flang")
  # ai cmake-extras
    include(CodeCoverage.cmake)
    # whatever append_coverage_compiler_flags() / setup_target_for_coverage_* calls follow
else()
    message(STATUS "Skipping code coverage setup: compiler is ${CMAKE_CXX_COMPILER_ID}, not GNU/Clang/Flang")
endif()

# configure_file(CMake/config.h.in ${PROJECT_BINARY_DIR}/include/config.h)

add_library(forking_machine INTERFACE)
target_include_directories(forking_machine INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(forking_machine PROPERTIES
    INTERFACE_CXX_STANDARD 20
    INTERFACE_CXX_STANDARD_REQUIRED OFF
)
add_library(forking_machine::forking_machine ALIAS forking_machine)
target_compile_features(forking_machine INTERFACE cxx_std_17)


if (FORKING_MACHINE_IS_TOP_LEVEL AND NOT WIN32)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_search_module(xorg-server IMPORTED_TARGET GLOBAL xorg-server)
    endif()
    if(xorg-server_FOUND)
      add_subdirectory(xorg)
    else()
      message(STATUS "xorg-server NOT FOUND, hence not building xorg")
    endif()
else()
    message(STATUS "Skipping Xorg subproject")
endif()


# check to have
find_package(GTest)
if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP true
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

if(FORKING_MACHINE_IS_TOP_LEVEL)
  enable_testing()
  add_subdirectory(test)
endif()


