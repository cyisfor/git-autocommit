AC_PREREQ([2.69])
AC_INIT([git-autocommit], [0.1], [derp])
AC_CONFIG_SRCDIR([src/client.c])
AC_CONFIG_AUX_DIR(build-aux)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign subdir-objects])
LT_INIT([disable-static])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES(GIT, libgit2)
PKG_CHECK_MODULES(EVENT, libevent)

AC_CHECK_LIB(ltdl, lt_dlopen)

# Checks for header files.
AC_CHECK_HEADERS([sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([dup2 mkdir munmap realpath setenv socket])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
