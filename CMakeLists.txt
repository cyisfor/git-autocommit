cmake_minimum_required(VERSION 3.14)
project(gpg-auto-commit VERSION 0.1)

include(FindPkgConfig)
pkg_check_modules(GPGME
  REQUIRED IMPORTED_TARGET
  gpgme)
pkg_check_modules(EVENT
  REQUIRED IMPORTED_TARGET
  libevent)
pkg_check_modules(GIT
  REQUIRED IMPORTED_TARGET
  libgit2)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/add_module")
include(AddModule)
add_module(cstuff
  GIT v1
  SIGNER 5F15F8C9C05B4D3D31EBB1C3F66D599380F88521
  file://$ENV{HOME}/code/cstuff
  file://$ENV{HOME}/repo/cstuff
  git@github.com:cyisfor/cstuff.git
  https://github.com/cyisfor/cstuff.git)
add_cstuff()

add_module(ctemplate
  GIT v1
  file://$ENV{HOME}/code/ctemplate
  file://$ENV{HOME}/repo/ctemplate
  git@github.com:cyisfor/ctemplate.git
  https://github.com/cyisfor/ctemplate.git)
add_library(autocommit SHARED
  src/activity.c
  src/check.c
  src/net.c
  src/repo.c
  src/hooks.c
  src/checkpid.c
  src/eventbase.c
  src/continuation.c
  src/note.c)
get_property(rpath TARGET autocommit PROPERTY BUILD_RPATH)
list(APPEND rpath "${CMAKE_CURRENT_BINARY_DIR}")
set_property(TARGET autocommit PROPERTY BUILD_RPATH "${rpath}")
configure_file(make_module.cmake.in make_module.cmake)
add_ctemplate(make_module
  "${CMAKE_CURRENT_BINARY_DIR}/make_module.cmake"
  "make_module.cmake.snippet.c")
add_dependencies(autocommit make_module)

if(COMPILING_MODULE)
  # cmake gets bitchy about building projects outside the binary directory
  # XXX: doesn't it get bitchy about compiling source files that aren't generated in the binary dir?
  
  add_library("${name}" MODULE "${location}/${src}")
  target_link_libraries("${name}" PUBLIC autocommit)

  set_property(TARGET "${name}" PROPERTY OUTPUT_NAME "${dst}")
  get_property(rpath TARGET "${name}" PROPERTY INSTALL_RPATH)
  list(APPEND rpath "${CMAKE_CURRENT_BINARY_DIR}")
  list(APPEND rpath "${location}")
  set_property(TARGET "${name}" PROPERTY INSTALL_RPATH "${rpath}")
  set(CMAKE_SHARED_MODULE_PREFIX "")
  set(CMAKE_SHARED_MODULE_SUFFIX "")
  install(TARGETS "${name}" DESTINATION "${location}")
else()
  add_executable(autocommit_client
	src/client.c)
  add_executable(autocommit_server
	src/server.c)
  target_link_libraries(autocommit_client PRIVATE autocommit)
  target_link_libraries(autocommit_server PRIVATE autocommit)
endif()

target_link_libraries(autocommit PUBLIC
  cstuff
  PkgConfig::GPGME
  PkgConfig::EVENT
  PkgConfig::GIT
  pthread
  ${CMAKE_DL_LIBS})
target_include_directories(autocommit PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
configure_file(src/config_locations.h.in config_locations.h)
# HAX
target_compile_options(cstuff PUBLIC -fPIC)
target_link_options(cstuff PUBLIC -fPIC)

target_compile_options(autocommit PUBLIC -fPIC)
target_link_options(autocommit PUBLIC -fPIC)

target_include_directories(autocommit PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
