
set(sources
  fork-plugin.cpp
)

add_library(fork-libinput MODULE ${sources})

target_compile_definitions(fork-libinput PRIVATE VERSION_STRING="git-local-2")
target_link_libraries(fork-libinput PkgConfig::libinput)

target_include_directories(fork-libinput PRIVATE ../src/)
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
