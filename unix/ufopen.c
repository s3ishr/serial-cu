/* ufopen.c
   Open a file with the permissions of the invoking user.

   Copyright (C) 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucp.h"

#include <errno.h>

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#endif

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

/* Local functions.  */

static uid_t isuser_perms P((void));
static boolean fsuucp_perms P((long ieuid));

/* Switch to permissions of the invoking user.  */

static uid_t
isuser_perms ()
{
  uid_t ieuid, iuid;

  ieuid = geteuid ();
  iuid = getuid ();

#if HAVE_SETREUID
  /* Swap the effective user id and the real user id.  We can then
     swap them back again when we want to return to the uucp user's
     permissions.  */
  if (setreuid (ieuid, iuid) < 0)
    {
      ulog (LOG_ERROR, "setreuid (%ld, %ld): %s",
	    (long) ieuid, (long) iuid, strerror (errno));
      return (uid_t) -1;
    }
#else /* ! HAVE_SETREUID */
#if HAVE_SAVED_SETUID
  /* Set the effective user id to the real user id.  Since the
     effective user id is saved (it's the saved setuid) we will able
     to set back to it later.  If the real user id is root we will not
     be able to switch back and forth, so don't even try.  */
  if (iuid != 0)
    {
      if (setuid (iuid) < 0)
	{
	  ulog (LOG_ERROR, "setuid (%ld): %s", (long) iuid, strerror (errno));
	  return (uid_t) -1;
	}
    }
#else /* ! HAVE_SAVED_SETUID */
  /* There's no way to switch between real permissions and effective
     permissions.  Just try to open the file with the uucp
     permissions.  */
#endif /* ! HAVE_SAVED_SETUID */
#endif /* ! HAVE_SETREUID */

  return ieuid;
}

/* Restore the uucp permissions.  */

/*ARGSUSED*/
static boolean
fsuucp_perms (ieuid)
     long ieuid;
{
#if HAVE_SETREUID
  /* Swap effective and real user id's back to what they were.  */
  if (isuser_perms () < 0)
    return FALSE;
#else /* ! HAVE_SETREUID */
#if HAVE_SAVED_SETUID
  /* Set ourselves back to our original effective user id.  */
  if (setuid ((uid_t) ieuid) < 0)
    {
      ulog (LOG_ERROR, "setuid (%ld): %s", (long) ieuid, strerror (errno));
      /* Is this error message helpful or confusing?  */
      if (errno == EPERM)
	ulog (LOG_ERROR,
	      "Probably HAVE_SAVED_SETUID in policy.h should be set to 0");
      return FALSE;
    }
#else /* ! HAVE_SAVED_SETUID */
  /* We didn't switch, no need to switch back.  */
#endif /* ! HAVE_SAVED_SETUID */
#endif /* ! HAVE_SETREUID */

  return TRUE;
}

/* Open a file with the permissions of the invoking user.  */

openfile_t
esysdep_user_fopen (zfile)
     const char *zfile;
{
  uid_t ieuid;
  openfile_t e;
  const char *zerr;
  int o = 0;

  ieuid = isuser_perms ();
  if (ieuid < 0)
    return EFILECLOSED;

  zerr = NULL;

#if USE_STDIO
  e = fopen (zfile, BINREAD);
  if (e == NULL)
    zerr = "fopen";
  else
    o = fileno (e);
#else
  e = open ((char *) zfile, O_RDONLY | O_NOCTTY, 0);
  if (e < 0)
    zerr = "open";
  else
    o = e;
#endif


  if (! fsuucp_perms ((long) ieuid))
    {
      if (ffileisopen (e))
	(void) ffileclose (e);
      return EFILECLOSED;
    }

  if (zerr != NULL)
    {
      ulog (LOG_ERROR, "%s (%s): %s", zerr, zfile, strerror (errno));
#if ! HAVE_SETREUID
      /* Are these error messages helpful or confusing?  */
#if HAVE_SAVED_SETUID
      if (errno == EACCES && getuid () == 0)
	ulog (LOG_ERROR,
	      "The superuser may only transfer files that are readable by %s",
	      OWNER);
#else
      if (errno == EACCES)
	ulog (LOG_ERROR,
	      "You may only transfer files that are readable by %s", OWNER);
#endif
#endif /* ! HAVE_SETREUID */
      return EFILECLOSED;
    }

  if (fcntl (o, F_SETFD, fcntl (o, F_GETFD, 0) | FD_CLOEXEC) < 0)
    {
      ulog (LOG_ERROR, "fcntl (FD_CLOEXEC): %s", strerror (errno));
      (void) ffileclose (e);
      return EFILECLOSED;
    }

  return e;
}