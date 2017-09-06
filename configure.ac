dnl
dnl  Copyright © 2017 Keith Packard <keithp@keithp.com>
dnl
dnl  This program is free software; you can redistribute it and/or modify
dnl  it under the terms of the GNU General Public License as published by
dnl  the Free Software Foundation; either version 2 of the License, or
dnl  (at your option) any later version.
dnl
dnl  This program is distributed in the hope that it will be useful, but
dnl  WITHOUT ANY WARRANTY; without even the implied warranty of
dnl  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl  General Public License for more details.
dnl
dnl  You should have received a copy of the GNU General Public License along
dnl  with this program; if not, write to the Free Software Foundation, Inc.,
dnl  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
dnl
dnl Process this file with autoconf to create configure.
AC_PREREQ(2.57)
AC_INIT([chaosread], 1.0)
AC_CONFIG_SRCDIR([chaosread.c])
AM_INIT_AUTOMAKE([foreign dist-bzip2])

RELEASE_DATE=2017-09-06
AC_SUBST(RELEASE_DATE)

VERSION_DASH=`echo $VERSION | sed 's/\./-/g'`
AC_SUBST(VERSION_DASH)

AC_PROG_CC
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES([LIBUSB], [libusb-1.0])

AC_OUTPUT([
Makefile])
