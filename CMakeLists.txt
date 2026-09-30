cmake_minimum_required(VERSION 3.13)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(CMAKE_COLOR_DIAGNOSTICS ON)
project(fork-plugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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
    INTERFACE_CXX_STANDARD 17
    INTERFACE_CXX_STANDARD_REQUIRED ON
)
add_library(forking_machine::forking_machine ALIAS forking_machine)
target_compile_features(forking_machine INTERFACE cxx_std_17)


if(NOT WIN32)
  find_package(PkgConfig REQUIRED)

  pkg_search_module(xorg-server
    REQUIRED
    IMPORTED_TARGET GLOBAL
    xorg-server
  )
if(xorg-server_MODULE_NAME)
  add_subdirectory(xorg)
else()
  message("xorg-server NOT FOUND, hence not building")
endif()
else()
    message(STATUS "Skipping Xorg subproject: not building on Linux")
endif()


# check to have
find_package(GTest)
if(GTest_FOUND)
  enable_testing()
  add_subdirectory(test)
else()
  message("GTest NOT FOUND, hence not building")
endif()
