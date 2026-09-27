cmake_minimum_required(VERSION 3.20)
project(fork_libinput)


include(FetchContent)
FetchContent_Declare(
    forking_machine
    GIT_REPOSITORY /home/michal/repo/fork/fork-plugin-core/
    # https://your-host/core-repo.git
    GIT_TAG        0.3
)
FetchContent_MakeAvailable(forking_machine)



find_package(PkgConfig REQUIRED)
pkg_check_modules(libinput REQUIRED IMPORTED_TARGET libinput)


set(sources
  fork-plugin.cpp
)

add_library(fork-libinput MODULE ${sources})

target_compile_definitions(fork-libinput PRIVATE VERSION_STRING="git-local-2")

#target_include_directories(fork-libinput PRIVATE ../src/)
target_link_libraries(fork-libinput PRIVATE forking_machine::forking_machine)
target_link_libraries(fork-libinput PRIVATE PkgConfig::libinput)

target_include_directories(fork-libinput PRIVATE AFTER ".")

set_property(TARGET fork-libinput PROPERTY NO_SONAME)

# link with libinput

set(moduledir "/usr/lib/libinput/modules/")


set_target_properties(fork-libinput PROPERTIES PREFIX "" # drop lib
  NO_SONAME "fork.so"
  OUTPUT_NAME "fork"
)

install(TARGETS fork-libinput
  EXPORT fork
  LIBRARY DESTINATION ${moduledir}
  # CMAKE_INSTALL_PREFIX CMAKE_INSTALL_LIBDIR
  )

export(EXPORT fork FILE dump.cmake)
