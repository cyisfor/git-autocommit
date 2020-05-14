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
  GIT v4
  SIGNER 5F15F8C9C05B4D3D31EBB1C3F66D599380F88521
  file://$ENV{HOME}/code/cstuff
  file://$ENV{HOME}/repo/cstuff
  git@github.com:cyisfor/cstuff.git
  https://github.com/cyisfor/cstuff.git)
add_cstuff()

if(COMPILING_MODULE)
  add_library(autocommit INTERFACE)
  # since cmake devs are dumb, you can't set target_include_directories that are public
  # for an interface library.
  set_property(TARGET autocommit PROPERTY includes
	"${CMAKE_CURRENT_BINARY_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/src")
  set(CMAKE_SHARED_MODULE_PREFIX "" PARENT_SCOPE)
  set(CMAKE_SHARED_MODULE_SUFFIX "" PARENT_SCOPE)
  
  # make sure location is set
  function(compile_module name src dest)
	add_library("${name}" MODULE "${location}/${src}")
	target_link_libraries("${name}" PUBLIC autocommit)

	set_property(TARGET "${name}" PROPERTY OUTPUT_NAME "${dest}")
	get_property(rpath TARGET "${name}" PROPERTY INSTALL_RPATH)
	list(APPEND rpath "${location}")
	set_property(TARGET "${name}" PROPERTY INSTALL_RPATH "${rpath}")
	install(TARGETS "${name}" DESTINATION "${location}")
	get_property(includes TARGET autocommit PROPERTY includes)
	target_include_directories("${name}" PUBLIC
	  ${includes})
	  
  endfunction(compile_module)
  set(derp INTERFACE)
else()
  add_module(ctemplate
	GIT v3
	file://$ENV{HOME}/code/ctemplate
	file://$ENV{HOME}/repo/ctemplate
	git@github.com:cyisfor/ctemplate.git
	https://github.com/cyisfor/ctemplate.git)
  configure_file(src/config_locations.h.in config_locations.h)
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
  configure_file(make_module.cmake.in make_module.cmake)
  add_ctemplate(make_module
	"${CMAKE_CURRENT_BINARY_DIR}/make_module.cmake"
	"make_module.cmake.snippet.c")
  add_dependencies(autocommit make_module)
  add_executable(autocommit_client
	src/client.c)
  add_executable(autocommit_server
	src/server.c)
  target_link_libraries(autocommit_client PRIVATE autocommit)
  target_link_libraries(autocommit_server PRIVATE autocommit)

  # HAX
  target_compile_options(cstuff PUBLIC -fPIC)
  target_link_options(cstuff PUBLIC -fPIC)

  target_compile_options(autocommit PUBLIC -fPIC)
  target_link_options(autocommit PUBLIC -fPIC)
  target_include_directories(autocommit PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
  target_include_directories(autocommit PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
  set(derp PUBLIC)
endif()

target_link_libraries(autocommit ${derp}
  cstuff
  PkgConfig::GPGME
  PkgConfig::EVENT
  PkgConfig::GIT
  pthread
  ${CMAKE_DL_LIBS})
