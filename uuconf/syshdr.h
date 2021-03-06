/* syshdr.h -*- C -*-
   Unix system header for the uuconf library.

   Copyright (C) 1992, 1993, 2002 Ian Lance Taylor

   This file is part of the Taylor UUCP uuconf library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com.
   */

/* The root directory (used when setting local-send and local-receive
   values).  */
#define ZROOTDIR "/"

/* The current directory (used by uuconv as a prefix for the newly
   created file names).  */
#define ZCURDIR "."

/* The names of the Taylor UUCP configuration files.  These are
   appended to NEWCONFIGLIB which is defined in Makefile.  */
#define CONFIGFILE "/config"
#define SYSFILE "/sys"
#define PORTFILE "/port"
#define CALLFILE "/call"
#define PASSWDFILE "/passwd"

/* A macro to check whether fopen failed because the file did not
   exist.  */
#define FNO_SUCH_FILE() (errno == ENOENT)

#if ! HAVE_STRERROR

/* We need a definition for strerror; normally the function in the
   unix directory is used, but we want to be independent of that
   library.  This macro evaluates its argument multiple times.  */
extern int sys_nerr;
extern char *sys_errlist[];

#define strerror(ierr) \
  ((ierr) >= 0 && (ierr) < sys_nerr ? sys_errlist[ierr] : "unknown error")

#endif /* ! HAVE_STRERROR */

/* This macro is used to make a filename found in a configuration file
   into an absolute path.  The zdir argument is the directory to put it
   in.  The zset argument is set to the new string.  The fallocated
   argument is set to TRUE if the new string was allocated.  */
#define MAKE_ABSOLUTE(zset, fallocated, zfile, zdir, pblock) \
  do \
    { \
      if (*(zfile) == '/') \
	{ \
	  (zset) = (zfile); \
	  (fallocated) = FALSE; \
	} \
      else \
	{ \
	  size_t abs_cdir, abs_cfile; \
	  char *abs_zret; \
\
	  abs_cdir = strlen (zdir); \
	  abs_cfile = strlen (zfile); \
	  abs_zret = (char *) uuconf_malloc ((pblock), \
					     abs_cdir + abs_cfile + 2); \
	  (zset) = abs_zret; \
	  (fallocated) = TRUE; \
	  if (abs_zret != NULL) \
	    { \
	      memcpy ((pointer) abs_zret, (pointer) (zdir), abs_cdir); \
	      abs_zret[abs_cdir] = '/'; \
	      memcpy ((pointer) (abs_zret + abs_cdir + 1), \
		      (pointer) (zfile), abs_cfile + 1); \
	    } \
	} \
    } \
  while (0)

/* We want to be able to mark the Taylor UUCP system files as close on
   exec.  */
#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

#define CLOSE_ON_EXEC(e) \
  do \
    { \
      int cle_i = fileno (e); \
 \
      fcntl (cle_i, F_SETFD, fcntl (cle_i, F_GETFD, 0) | FD_CLOEXEC); \
    } \
  while (0)
