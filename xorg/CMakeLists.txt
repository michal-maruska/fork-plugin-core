cmake_minimum_required(VERSION 3.13)

# /usr/share/cmake-3.30/Modules/FindPkgConfig.cmake
# set(PKG_CONFIG_EXECUTABLE "$ENV{PKG_CONFIG}" CACHE FILEPATH "pkg-config executable")
# mark_as_advanced(PKG_CONFIG_EXECUTABLE)
find_package(PkgConfig)

# This results in "Cmake Debug Log:" during the cmake --configure run
set(CMAKE_DEBUG_TARGET_PROPERTIES
  #INCLUDE_DIRECTORIES
  # COMPILE_DEFINITIONS
  COMPILE_OPTIONS
  AUTOUIC_OPTIONS
  POSITION_INDEPENDENT_CODE
  )


# source only library:
# the header file?

# not GLOB_RECURSE
# file(GLOB sources "*.cpp"  "*.c")
set(sources
  fork-plugin.cpp
  xmachine.cpp
  event_ops.cpp

  # header files?
  # machine.h
  # colors.h
  # platform.h
  # xorg.h
)

# enable warnings:
# Wall
add_compile_options("-Wall")
add_library(fork MODULE ${sources})
target_include_directories(fork AFTER PUBLIC PRIVATE "../src/")
target_include_directories(fork AFTER PUBLIC PRIVATE ".")
target_compile_definitions(fork PRIVATE VERSION_STRING="git-local-2")

set_target_properties(fork PROPERTIES PREFIX "" # drop lib
  NO_SONAME "fork.so"
  OUTPUT_NAME "fork"
)

pkg_search_module(xorg-server REQUIRED xorg-server
  IMPORTED_TARGET GLOBAL)
# /usr/lib/x86_64-linux-gnu/pkgconfig/xorg-server.pc
# or SDK?


#why not ${PkgConfig::xorg-server} -- because those are imported targets!
target_link_libraries(fork PkgConfig::xorg-server)
# ${xorg-server_LIBRARIES}
# PROJECT_BINARY_DIR
# ${PROJECT_SOURCE_DIR}/

# set(CMAKE_MY_NAME "" CACHE STRING "Flags used by the compiler during all build types.")

pkg_get_variable(moduledir xorg-server moduledir)
pkg_get_variable(includedir xorg-server sdkdir)


message(STATUS "Building Fork & install in " ${moduledir})
message(STATUS "USing Xorg headers from " ${includedir})

# $<INSTALL_PREFIX>
install(FILES ../include/fork_requests.h
  #PUBLIC_HEADER
  DESTINATION ${includedir}
  )

install(TARGETS fork
  EXPORT fork
  LIBRARY    DESTINATION ${moduledir}
  # CMAKE_INSTALL_PREFIX CMAKE_INSTALL_LIBDIR
  )
