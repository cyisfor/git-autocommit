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

add_library(autocommit
  	src/activity.c
	src/check.c
	src/net.c
	src/repo.c
	src/hooks.c
	src/checkpid.c
	src/eventbase.c
	src/continuation.c
	src/note.c)
target_link_libraries(autocommit PUBLIC
  PkgConfig::GPGME
  PkgConfig::EVENT
  PkgConfig::GIT
  pthread
  ltdl)
target_include_directories(autocommit PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(autocommit PUBLIC -fPIC)
target_link_options(autocommit PUBLIC -fPIC)

add_executable(autocommit_client
  src/client.c)
add_executable(autocommit_server
  src/server.c)
target_link_libraries(autocommit_client PRIVATE autocommit)
target_link_libraries(autocommit_server PRIVATE autocommit)

configure_file(src/my_cflags.cmake.h my_cflags.cmakesucks)
file(GENERATE OUTPUT my_cflags.h
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/my_cflags.cmakesucks")

add_custom_target(check_my_cflags
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/my_cflags.h")
add_dependencies(autocommit check_my_cflags)
