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
  ${CMAKE_DL_LIBS})
target_include_directories(autocommit PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(autocommit PUBLIC -fPIC)
target_link_options(autocommit PUBLIC -fPIC)
target_compile_definitions(autocommit PUBLIC
  "BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
  "SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}")

if("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  add_executable(autocommit_client
	src/client.c)
  add_executable(autocommit_server
	src/server.c)
  target_link_libraries(autocommit_client PRIVATE autocommit)
  target_link_libraries(autocommit_server PRIVATE autocommit)
else()
  add_library("$ENV{name}" MODULE "$ENV{src}")
  target_link_libraries("$ENV{name}" PUBLIC autocommit)
  set_property(TARGET "$ENV{name}" PROPERTY OUTPUT_NAME "$ENV{dst}")
  set(CMAKE_SHARED_MODULE_PREFIX "")
  set(CMAKE_SHARED_MODULE_SUFFIX ".so")

  install(TARGETS "$ENV{name}" RUNTIME_DESTINATION "$ENV{dstloc}")
endif()


