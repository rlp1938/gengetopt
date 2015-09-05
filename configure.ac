#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([gengo], [1.0], [rlp1938@gmail.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([gengo.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([limits.h stdlib.h string.h unistd.h fcntl.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memchr memset strchr strdup strtol strstr])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
