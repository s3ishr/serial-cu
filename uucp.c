/* uucp.c
   Prepare to copy a file to or from a remote system.

   Copyright (C) 1991 Ian Lance Taylor

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
   c/o AIRS, P.O. Box 520, Waltham, MA 02254.

   $Log$
   Revision 1.10  1992/01/15  07:06:29  ian
   Set configuration directory in Makefile rather than sysdep.h

   Revision 1.9  1992/01/05  03:09:17  ian
   Changed abProgram and abVersion to non const to avoid compiler bug

   Revision 1.8  1991/12/21  21:09:01  ian
   Use ulog to report illegal grade error message

   Revision 1.7  1991/12/18  03:54:14  ian
   Made error messages to terminal appear more normal

   Revision 1.6  1991/12/11  03:59:19  ian
   Create directories when necessary; don't just assume they exist

   Revision 1.5  1991/11/21  22:17:06  ian
   Add version string, print version when printing usage

   Revision 1.4  1991/11/13  23:08:40  ian
   Expand remote pathnames in uucp and uux; fix up uux special cases

   Revision 1.3  1991/09/19  02:30:37  ian
   From Chip Salzenberg: check whether signal is ignored differently

   Revision 1.2  1991/09/11  02:33:14  ian
   Added ffork argument to fsysdep_run

   Revision 1.1  1991/09/10  19:40:31  ian
   Initial revision
  
   */

#include "uucp.h"

#if USE_RCS_ID
char uucp_rcsid[] = "$Id$";
#endif

#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#include "getopt.h"

#include "system.h"
#include "sysdep.h"

/* The program name.  */
char abProgram[] = "uucp";

/* Long getopt options.  */

static const struct option asClongopts[] = { { NULL, 0, NULL, 0 } };

const struct option *_getopt_long_options = asClongopts;

/* Local functions.  */

static void ucusage P((void));
static sigret_t uccatch P((int isig));
static void ucadd_cmd P((const struct ssysteminfo *qsys,
			 const struct scmd *qcmd));
static void ucspool_cmds P((int bgrade));
static const char *zcone_system P((boolean *pfany));

int
main (argc, argv)
     int argc;
     char **argv;
{
  int iopt;
  /* -c,-C: if true, copy to spool directory.  */
  boolean fcopy = TRUE;
  /* -d,-f: if true, create directories if they don't exist.  */
  boolean fmkdirs = TRUE;
  /* -g: job grade.  */
  char bgrade = BDEFAULT_UUCP_GRADE;
  /* -I: configuration file name.  */
  const char *zconfig = NULL;
  /* -j: output job id.  */
  boolean fjobid = FALSE;
  /* -m: mail to requesting user.  */
  boolean fmail = FALSE;
  /* -n: notify remote user.  */
  const char *znotify = "";
  /* -r: don't start uucico when finished.  */
  boolean fuucico = TRUE;
  /* -s: report status to named file.  */
  const char *zstatus_file = NULL;
  /* -W: expand local file names only.  */
  boolean fexpand = TRUE;
  /* -x: set debugging level.  */
  int idebug = -1;
  int i;
  char *zexclam;
  char *zdestfile;
  const char *zconst;
  struct ssysteminfo sdestsys;
  const struct ssysteminfo *qdestsys;
  boolean flocaldest;
  const char *zuser;
  char absend_options[5];
  char abrec_options[3];
  char *zoptions;
  boolean fexit;

  while ((iopt = getopt (argc, argv, "cCdfg:I:jmn:rs:Wx:")) != EOF)
    {
      switch (iopt)
	{
	case 'c':
	  /* Do not copy local files to spool directory.  */
	  fcopy = FALSE;
	  break;

	case 'C':
	  /* Copy local files to spool directory.  */
	  fcopy = TRUE;
	  break;

	case 'd':
	  /* Create directories if necessary.  */
	  fmkdirs = TRUE;
	  break;

	case 'f':
	  /* Do not create directories if they don't exist.  */
	  fmkdirs = FALSE;
	  break;

	case 'g':
	  /* Set job grade.  */
	  bgrade = optarg[0];
	  break;

	case 'I':
	  /* Name configuration file.  */
	  zconfig = optarg;
	  break;

	case 'j':
	  /* Output job id.  */
	  fjobid = TRUE;
	  break;

	case 'm':
	  /* Mail to requesting user.  */
	  fmail = TRUE;
	  break;

	case 'n':
	  /* Notify remote user.  */
	  znotify = optarg;
	  break;

	case 'r':
	  /* Don't start uucico when finished.  */
	  fuucico = FALSE;
	  break;

	case 's':
	  /* Report status to named file.  */
	  zstatus_file = optarg;
	  break;

	case 'W':
	  /* Expand only local file names.  */
	  fexpand = FALSE;
	  break;

	case 'x':
	  /* Set debugging level.  */
	  idebug = atoi (optarg);
	  break;

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  ucusage ();
	  break;
	}
    }

  if (! FGRADE_LEGAL (bgrade))
    {
      /* We use LOG_NORMAL rather than LOG_ERROR because this is going
	 to stderr rather than to the log file, and we don't need the
	 ERROR header string.  */
      ulog (LOG_NORMAL, "Ignoring illegal grade");
      bgrade = BDEFAULT_UUCP_GRADE;
    }

  if (argc - optind < 2)
    ucusage ();

  uread_config (zconfig);

  /* Let command line override configuration file.  */
  if (idebug != -1)
    iDebug = idebug;

#ifdef SIGINT
  if (signal (SIGINT, SIG_IGN) != SIG_IGN)
    (void) signal (SIGINT, uccatch);
#endif
#ifdef SIGHUP
  if (signal (SIGHUP, SIG_IGN) != SIG_IGN)
    (void) signal (SIGHUP, uccatch);
#endif
#ifdef SIGQUIT
  if (signal (SIGQUIT, SIG_IGN) != SIG_IGN)
    (void) signal (SIGQUIT, uccatch);
#endif
#ifdef SIGTERM
  if (signal (SIGTERM, SIG_IGN) != SIG_IGN)
    (void) signal (SIGTERM, uccatch);
#endif
#ifdef SIGPIPE
  if (signal (SIGPIPE, SIG_IGN) != SIG_IGN)
    (void) signal (SIGPIPE, uccatch);
#endif
#ifdef SIGABRT
  (void) signal (SIGABRT, uccatch);
#endif

  usysdep_initialize (FALSE);

  zuser = zsysdep_login_name ();
  if (zuser == NULL)
    zuser = "unknown";

  /* Set up the options.  */

  zoptions = absend_options;
  if (fcopy)
    *zoptions++ = 'C';
  else
    *zoptions++ = 'c';
  if (fmkdirs)
    *zoptions++ = 'd';
  else
    *zoptions++ = 'f';
  if (fmail)
    *zoptions++ = 'm';
  if (*znotify != '\0')
    *zoptions++ = 'n';
  *zoptions = '\0';

  zoptions = abrec_options;
  if (fmkdirs)
    *zoptions++ = 'd';
  else
    *zoptions++ = 'f';
  if (fmail)
    *zoptions++ = 'm';
  *zoptions = '\0';

  zexclam = strchr (argv[argc - 1], '!');
  if (zexclam == NULL)
    {
      zdestfile = argv[argc - 1];
      qdestsys = &sLocalsys;
      flocaldest = TRUE;
    }
  else
    {
      int clen;
      char *zcopy;

      clen = zexclam - argv[argc - 1];
      zcopy = (char *) alloca (clen + 1);
      strncpy (zcopy, argv[argc - 1], clen);
      zcopy[clen] = '\0';

      zdestfile = zexclam + 1;

      if (*zcopy == '\0' || strcmp (zcopy, zLocalname) == 0)
	{
	  qdestsys = &sLocalsys;
	  flocaldest = TRUE;
	}
      else
	{
	  if (fread_system_info (zcopy, &sdestsys))
	    qdestsys = &sdestsys;
	  else
	    {
	      if (! fUnknown_ok)
		ulog (LOG_FATAL, "System %s unknown", zcopy);
	      qdestsys = &sUnknown;
	      sUnknown.zname = zcopy;
	    }

	  flocaldest = FALSE;

	  if (! fsysdep_make_spool_dir (qdestsys))
	    {
	      ulog_close ();
	      usysdep_exit (FALSE);
	    }
	}
    }

  /* If the destination file is not an absolute path, expand it
     with the current directory.  */
  if (fexpand || flocaldest)
    {
      zconst = zsysdep_add_cwd (zdestfile, flocaldest);
      if (zconst == NULL)
	{
	  ulog_close ();
	  usysdep_exit (FALSE);
	}
      zdestfile = xstrdup (zconst);
    }

  /* Process each file.  */

  for (i = optind; i < argc - 1; i++)
    {
      struct scmd s;

      zexclam = strchr (argv[i], '!');

      if (zexclam == NULL)
	{
	  char *zfrom;

	  /* This is a local file.  Make sure we get it out of the
	     original directory.  We don't support local wildcards
	     yet (if ever).  */
	  zconst = zsysdep_add_cwd (argv[i], TRUE);
	  if (zconst == NULL)
	    {
	      ulog_close ();
	      usysdep_exit (FALSE);
	    }
	  zfrom = xstrdup (zconst);

	  if (flocaldest)
	    {
	      char *zto;

	      /* Copy one local file to another.  We have to check the
		 destination to make sure we are allowed to copy files
		 there, and we have to know whether the destination is
		 a directory.  */

	      if (! fin_directory_list (&sLocalsys, zdestfile,
					sLocalsys.zremote_receive))
		ulog (LOG_FATAL, "Permission denied for %s", zdestfile);

	      zconst = zsysdep_in_dir (zdestfile, argv[i]);
	      if (zconst == NULL)
		{
		  ulog_close ();
		  usysdep_exit (FALSE);
		}
	      zto = xstrdup (zconst);

	      if (! fcopy_file (zfrom, zto, FALSE, fmkdirs))
		{
		  ulog_close ();
		  usysdep_exit (FALSE);
		}

	      xfree ((pointer) zto);
	    }
	  else
	    {
	      char abtname[CFILE_NAME_LEN];
	      unsigned int imode;

	      /* Copy a local file to a remote file.  We may have to
		 copy the local file to the spool directory.  */

	      imode = isysdep_file_mode (zfrom);
	      if (imode == 0)
		{
		  ulog_close ();
		  usysdep_exit (FALSE);
		}

	      if (! fcopy)
		strcpy (abtname, "D.0");
	      else
		{
		  char *zdata;

		  zconst = zsysdep_data_file_name (qdestsys, bgrade,
						   abtname, (char *) NULL,
						   (char *) NULL);
		  if (zconst == NULL)
		    {
		      ulog_close ();
		      usysdep_exit (FALSE);
		    }
		  zdata = xstrdup (zconst);

		  if (! fcopy_file (zfrom, zdata, FALSE, TRUE))
		    {
		      ulog_close ();
		      usysdep_exit (FALSE);
		    }
		  xfree ((pointer) zdata);
		}

	      s.bcmd = 'S';
	      s.pseq = NULL;
	      s.zfrom = zfrom;
	      s.zto = zdestfile;
	      s.zuser = zuser;
	      s.zoptions = absend_options;
	      s.ztemp = abtname;
	      s.imode = imode;
	      s.znotify = znotify;
	      s.cbytes = -1;

	      ucadd_cmd (qdestsys, &s);
	    }
	}
      else
	{
	  int clen;
	  char *zcopy;
	  struct ssysteminfo *qfromsys;

	  /* Add the current directory to the filename if it's not
	     already there.  */
	  if (fexpand)
	    {
	      zconst = zsysdep_add_cwd (zexclam + 1, FALSE);
	      if (zconst == NULL)
		{
		  ulog_close ();
		  usysdep_exit (FALSE);
		}
	      zconst = xstrdup (zconst);
	    }
	  else
	    zconst = zexclam + 1;

	  /* Read the system information.  */
	  clen = zexclam - argv[i];
	  zcopy = (char *) xmalloc (clen + 1);
	  strncpy (zcopy, argv[i], clen);
	  zcopy[clen] = '\0';

	  qfromsys = ((struct ssysteminfo *)
		      xmalloc (sizeof (struct ssysteminfo)));
	  if (fread_system_info (zcopy, qfromsys))
	    xfree ((pointer) zcopy);
	  else
	    {
	      *qfromsys = sUnknown;
	      qfromsys->zname = zcopy;
	    }

	  if (! fsysdep_make_spool_dir (qfromsys))
	    {
	      ulog_close ();
	      usysdep_exit (FALSE);
	    }

	  if (flocaldest)
	    {
	      char *zto;

	      /* Fetch a file from a remote system.  If the remote
		 filespec is wildcarded, we must generate an 'X'
		 request.  We currently check for Unix shell
		 wildcards.  Note that it should do no harm to mistake
		 a non-wildcard for a wildcard.  */

	      if (strchr (zconst, '*') != NULL
		  || strchr (zconst, '?') != NULL
		  || strchr (zconst, '[') != NULL)
		{
		  s.bcmd = 'X';
		  zto = (char *) alloca (strlen (zLocalname)
					 + strlen (zdestfile)
					 + sizeof "!");
		  sprintf (zto, "%s!%s", zLocalname, zdestfile);
		  zto = xstrdup (zto);
		}
	      else
		{
		  s.bcmd = 'R';
		  zto = zdestfile;
		}
	      s.pseq = NULL;
	      s.zfrom = zconst;
	      s.zto = zto;
	      s.zuser = zuser;
	      s.zoptions = abrec_options;
	      s.ztemp = "";
	      s.imode = 0;
	      s.znotify = "";
	      s.cbytes = -1;

	      ucadd_cmd (qfromsys, &s);
	    }
	  else
	    {
	      /* Move a file from one remote system to another.  */

	      s.bcmd = 'X';
	      s.pseq = NULL;
	      s.zfrom = zconst;
	      s.zto = argv[argc - 1];
	      s.zuser = zuser;
	      s.zoptions = abrec_options;
	      s.ztemp = "";
	      s.imode = 0;
	      s.znotify = "";
	      s.cbytes = -1;

	      ucadd_cmd (qfromsys, &s);
	    }
	}
    }

  /* Now push out the actual commands, making log entries for them.  */
  ulog_to_file (TRUE);
  ulog_user (zuser);

  ucspool_cmds (bgrade);

  ulog_close ();

  if (! fuucico)
    fexit = TRUE;
  else
    {
      const char *zsys;
      boolean fany;

      zsys = zcone_system (&fany);
      if (zsys != NULL)
	fexit = fsysdep_run (TRUE, "uucico", "-s", zsys);
      else if (fany)
	fexit = fsysdep_run (TRUE, "uucico", "-r1", (const char *) NULL);
      else
	fexit = TRUE;
    }

  usysdep_exit (fexit);

  /* Avoid error about not returning.  */
  return 0;
}

static void
ucusage ()
{
  fprintf (stderr,
	   "Taylor UUCP version %s, copyright (C) 1991 Ian Lance Taylor\n",
	   abVersion);
  fprintf (stderr,
	   "Usage: uucp [options] file1 [file2 ...] dest\n");
  fprintf (stderr,
	   " -c: Do not copy local files to spool directory\n");
  fprintf (stderr,
	   " -C: Copy local files to spool directory (default)\n");
  fprintf (stderr,
	   " -d: Create necessary directories (default)\n");
  fprintf (stderr,
	   " -f: Do not create directories (fail if they do not exist)\n");
  fprintf (stderr,
	   " -g grade: Set job grade (must be alphabetic)\n");
  fprintf (stderr,
	   " -m: Report status of copy by mail\n");
  fprintf (stderr,
	   " -n user: Report status of copy by mail to remote user\n");
  fprintf (stderr,
	   " -r: Do not start uucico daemon\n");
  fprintf (stderr,
	   " -s file: Report completion status to file\n");
  fprintf (stderr,
	   " -j: Report job id\n");
  fprintf (stderr,
	   " -x debug: Set debugging level\n");
#if HAVE_TAYLOR_CONFIG
  fprintf (stderr,
	   " -I file: Set configuration file to use (default %s%s)\n",
	   NEWCONFIGLIB, CONFIGFILE);
#endif /* HAVE_TAYLOR_CONFIG */
  exit (EXIT_FAILURE);
}

/* Catch a signal.  We should cleanup here, but we don't yet.  */

static sigret_t
uccatch (isig)
     int isig;
{
  if (fAborting)
    {
      ulog_close ();
      usysdep_exit (FALSE);
    }
  else
    {
      ulog (LOG_ERROR, "Got signal %d", isig);
      ulog_close ();
      (void) signal (isig, SIG_DFL);
      raise (isig);
    }
}

/* We keep a list of jobs for each system.  */

struct sjob
{
  struct sjob *qnext;
  const struct ssysteminfo *qsys;
  int ccmds;
  struct scmd *pascmds;
};

static struct sjob *qCjobs;

static void
ucadd_cmd (qsys, qcmd)
     const struct ssysteminfo *qsys;
     const struct scmd *qcmd;
{
  struct sjob *qjob;

  for (qjob = qCjobs; qjob != NULL; qjob = qjob->qnext)
    if (strcmp (qjob->qsys->zname, qsys->zname) == 0)
      break;

  if (qjob == NULL)
    {
      qjob = (struct sjob *) xmalloc (sizeof (struct sjob));
      qjob->qnext = qCjobs;
      qjob->qsys = qsys;
      qjob->ccmds = 0;
      qjob->pascmds = NULL;
      qCjobs = qjob;
    }

  qjob->pascmds = ((struct scmd *)
		   xrealloc ((pointer) qjob->pascmds,
			     (qjob->ccmds + 1) * sizeof (struct scmd)));
  qjob->pascmds[qjob->ccmds] = *qcmd;
  ++qjob->ccmds;
}

static void
ucspool_cmds (bgrade)
     int bgrade;
{
  struct sjob *qjob;

  for (qjob = qCjobs; qjob != NULL; qjob = qjob->qnext)
    {
      ulog_system (qjob->qsys->zname);
      if (fsysdep_spool_commands (qjob->qsys, bgrade, qjob->ccmds,
				  qjob->pascmds))
	{
	  int i;
	  struct scmd *qcmd;

	  for (i = 0, qcmd = qjob->pascmds; i < qjob->ccmds; i++, qcmd++)
	    {
	      if (qcmd->bcmd == 'S')
		ulog (LOG_NORMAL, "Queuing send of %s to %s",
		      qcmd->zfrom, qcmd->zto);
	      else if (qcmd->bcmd == 'R')
		ulog (LOG_NORMAL, "Queuing request of %s to %s",
		      qcmd->zfrom, qcmd->zto);
	      else
		ulog (LOG_NORMAL, "Queuing execution (%s to %s)",
		      qcmd->zfrom, qcmd->zto);
	    }
	}
    }
}

/* Return the system name for which we have created commands, or NULL
   if we've created commands for more than one system.  Set *pfany to
   FALSE if we didn't create work for any system.  */

static const char *
zcone_system (pfany)
     boolean *pfany;
{
  if (qCjobs == NULL)
    {
      *pfany = FALSE;
      return NULL;
    }

  *pfany = TRUE;

  if (qCjobs->qnext == NULL)
    return qCjobs->qsys->zname;
  else
    return NULL;
}
