/* uucp.c
   Prepare to copy a file to or from a remote system.

   Copyright (C) 1991, 1992 Ian Lance Taylor

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
   c/o AIRS, P.O. Box 520, Waltham, MA 02254.  */

#include "uucp.h"

#if USE_RCS_ID
char uucp_rcsid[] = "$Id$";
#endif

#include <ctype.h>
#include <errno.h>

#include "getopt.h"

#include "system.h"
#include "sysdep.h"

/* Local functions.  */

static void ucusage P((void));
static void ucdirfile P((const char *zdir, const char *zfile,
			 pointer pinfo));
static void uccopy P((const char *zfile, const char *zdest));
static void ucadd_cmd P((const struct ssysteminfo *qsys,
			 const struct scmd *qcmd));
static void ucspool_cmds P((boolean fjobid));
static const char *zcone_system P((boolean *pfany));
static void ucrecord_file P((const char *zfile));
static void ucabort P((void));

/* The program name.  */
char abProgram[] = "uucp";

/* Long getopt options.  */

static const struct option asClongopts[] = { { NULL, 0, NULL, 0 } };

const struct option *_getopt_long_options = asClongopts;

/* Local variables.  There are a bunch of these, mostly set by the
   options and the last (the destination) argument.  These have file
   scope so that they may be easily passed into uccopy; they could for
   the most part also be wrapped up in a structure and passed in.  */

/* TRUE if source files should be copied to the spool directory.  */
static boolean fCcopy = TRUE;

/* Grade to use.  */
static char bCgrade = BDEFAULT_UUCP_GRADE;

/* User to notify on remote system.  */
static const char *zCnotify = "";

/* TRUE if remote files should be prefixed with the current working
   directory.  */
static boolean fCexpand = TRUE;

/* TRUE if necessary directories should be created on the destination
   system.  */
static boolean fCmkdirs = TRUE;

/* User name.  */
static const char *zCuser;

/* TRUE if the destination is this system.  */
static boolean fClocaldest;

/* Destination system.  */
static const struct ssysteminfo *qCdestsys;

/* Options to use when sending a file.  */
static char abCsend_options[20];

/* Options to use when receiving a file.  */
static char abCrec_options[20];

/* The main program.  */

int
main (argc, argv)
     int argc;
     char **argv;
{
  int iopt;
  /* -I: configuration file name.  */
  const char *zconfig = NULL;
  /* -j: output job id.  */
  boolean fjobid = FALSE;
  /* -m: mail to requesting user.  */
  boolean fmail = FALSE;
  /* -r: don't start uucico when finished.  */
  boolean fuucico = TRUE;
  /* -R: copy directories recursively.  */
  boolean frecursive = FALSE;
  /* -s: report status to named file.  */
  const char *zstatus_file = NULL;
  /* -t: emulate uuto.  */
  boolean fuuto = FALSE;
  int i;
  boolean fgetcwd;
  char *zexclam;
  char *zdestfile;
  const char *zconst;
  char *zcopy;
  struct ssysteminfo sdestsys;
  char *zoptions;
  boolean fexit;

  while ((iopt = getopt (argc, argv, "cCdfg:I:jmn:prRs:tWx:")) != EOF)
    {
      switch (iopt)
	{
	case 'c':
	  /* Do not copy local files to spool directory.  */
	  fCcopy = FALSE;
	  break;

	case 'p':
	case 'C':
	  /* Copy local files to spool directory.  */
	  fCcopy = TRUE;
	  break;

	case 'd':
	  /* Create directories if necessary.  */
	  fCmkdirs = TRUE;
	  break;

	case 'f':
	  /* Do not create directories if they don't exist.  */
	  fCmkdirs = FALSE;
	  break;

	case 'g':
	  /* Set job grade.  */
	  bCgrade = optarg[0];
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
	  zCnotify = optarg;
	  break;

	case 'r':
	  /* Don't start uucico when finished.  */
	  fuucico = FALSE;
	  break;

	case 'R':
	  /* Copy directories recursively.  */
	  frecursive = TRUE;
	  break;

	case 's':
	  /* Report status to named file.  */
	  zstatus_file = optarg;
	  break;

	case 't':
	  /* Emulate uuto.  */
	  fuuto = TRUE;
	  break;

	case 'W':
	  /* Expand only local file names.  */
	  fCexpand = FALSE;
	  break;

	case 'x':
#if DEBUG > 1
	  /* Set debugging level.  */
	  iDebug |= idebug_parse (optarg);
#endif
	  break;

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  ucusage ();
	  break;
	}
    }

  if (! FGRADE_LEGAL (bCgrade))
    {
      ulog (LOG_ERROR, "Ignoring illegal grade");
      bCgrade = BDEFAULT_UUCP_GRADE;
    }

  if (argc - optind < 2)
    ucusage ();

  uread_config (zconfig);

  /* If we are emulating uuto, translate the destination argument, and
     notify the destination user.  */

  if (fuuto)
    {
      if (*zCnotify == '\0')
	{
	  zexclam = strrchr (argv[argc - 1], '!');
	  if (zexclam == NULL)
	    ucusage ();
	  zCnotify = zexclam + 1;
	}
      argv[argc - 1] = zsysdep_uuto (argv[argc - 1]);
      if (argv[argc - 1] == NULL)
	ucusage ();
    }

  /* See if we are going to need to know the current directory.  We
     just check each argument to see whether it's an absolute
     pathname.  We actually aren't going to need the cwd if fCexpand
     is FALSE and the file is remote, but so what.  */
  fgetcwd = FALSE;
  for (i = optind; i < argc; i++)
    {
      zexclam = strrchr (argv[i], '!');
      if (zexclam == NULL)
	zexclam = argv[i];
      else
	++zexclam;
      if (fsysdep_needs_cwd (zexclam))
	{
	  fgetcwd = TRUE;
	  break;
	}
    }

#ifdef SIGINT
  usysdep_signal (SIGINT);
#endif
#ifdef SIGHUP
  usysdep_signal (SIGHUP);
#endif
#ifdef SIGQUIT
  usysdep_signal (SIGQUIT);
#endif
#ifdef SIGTERM
  usysdep_signal (SIGTERM);
#endif
#ifdef SIGPIPE
  usysdep_signal (SIGPIPE);
#endif

  usysdep_initialize (fgetcwd ? INIT_GETCWD : 0);

  ulog_fatal_fn (ucabort);

  zCuser = zsysdep_login_name ();

  /* Set up the options.  */

  zoptions = abCsend_options;
  if (fCcopy)
    *zoptions++ = 'C';
  else
    *zoptions++ = 'c';
  if (fCmkdirs)
    *zoptions++ = 'd';
  else
    *zoptions++ = 'f';
  if (fmail)
    *zoptions++ = 'm';
  if (*zCnotify != '\0')
    *zoptions++ = 'n';
  *zoptions = '\0';

  zoptions = abCrec_options;
  if (fCmkdirs)
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
      qCdestsys = &sLocalsys;
      fClocaldest = TRUE;
    }
  else
    {
      int clen;

      clen = zexclam - argv[argc - 1];
      zcopy = (char *) alloca (clen + 1);
      strncpy (zcopy, argv[argc - 1], clen);
      zcopy[clen] = '\0';

      zdestfile = zexclam + 1;

      if (*zcopy == '\0' || strcmp (zcopy, zLocalname) == 0)
	{
	  qCdestsys = &sLocalsys;
	  fClocaldest = TRUE;
	}
      else
	{
	  if (fread_system_info (zcopy, &sdestsys))
	    qCdestsys = &sdestsys;
	  else
	    {
	      if (! fUnknown_ok)
		ulog (LOG_FATAL, "System %s unknown", zcopy);
	      qCdestsys = &sUnknown;
	      sUnknown.zname = zcopy;
	    }

	  fClocaldest = FALSE;

	  if (! fsysdep_make_spool_dir (qCdestsys))
	    {
	      ulog_close ();
	      usysdep_exit (FALSE);
	    }
	}
    }

  /* If the destination file is not an absolute path, expand it
     with the current directory.  */
  if (fCexpand || fClocaldest)
    {
      zconst = zsysdep_add_cwd (zdestfile, fClocaldest);
      if (zconst == NULL)
	{
	  ulog_close ();
	  usysdep_exit (FALSE);
	}
      zdestfile = xstrdup (zconst);
    }

  /* Process each source argument.  */

  for (i = optind; i < argc - 1 && ! FGOT_SIGNAL (); i++)
    {
      boolean flocal;
      char *zfrom;

      if (strchr (argv[i], '!') != NULL)
	{
	  flocal = FALSE;
	  zfrom = argv[i];
	}
      else
	{
	  /* This is a local file.  Make sure we get it out of the
	     original directory.  We don't support local wildcards yet
	     (if ever).  */
	  flocal = TRUE;
	  zconst = zsysdep_add_cwd (argv[i], TRUE);
	  if (zconst == NULL)
	    ucabort ();
	  zfrom = xstrdup (zconst);
	}

      if (! flocal || ! fsysdep_directory (zfrom))
	uccopy (zfrom, zdestfile);
      else
	{
	  if (! frecursive)
	    ulog (LOG_ERROR, "%s: directory without -R", zfrom);
	  else
	    {
	      zconst = zsysdep_base_name (zfrom);
	      if (zconst == NULL)
		ucabort ();
	      zcopy = (char *) alloca (strlen (zconst) + 1);
	      strcpy (zcopy, zconst);
	      zconst = zsysdep_in_dir (zdestfile, zcopy);
	      if (zconst == NULL)
		ucabort ();
	      usysdep_walk_tree (zfrom, ucdirfile,
				 (pointer) xstrdup (zconst));
	    }
	}
    }

  /* See if we got an interrupt, presumably from the user.  */
  if (FGOT_SIGNAL ())
    ucabort ();

  /* Now push out the actual commands, making log entries for them.  */
  ulog_to_file (TRUE);
  ulog_user (zCuser);

  ucspool_cmds (fjobid);

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
	   "Taylor UUCP version %s, copyright (C) 1991, 1992 Ian Lance Taylor\n",
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

/* This is called for each file in a directory heirarchy.  */

static void
ucdirfile (zfull, zrelative, pinfo)
     const char *zfull;
     const char *zrelative;
     pointer pinfo;
{
  const char *zdestfile = (const char *) pinfo;
  const char *zto;

  zto = zsysdep_in_dir (zdestfile, zrelative);
  if (zto == NULL)
    ucabort ();
  zto = xstrdup (zto);

  uccopy (xstrdup (zfull), zto);
}

/* Handle the copying of one regular file.  The zdest argument is the
   destination file; if we are recursively copying a directory, it
   will be extended by any subdirectory names.  */

static void
uccopy (zfile, zdest)
     const char *zfile;
     const char *zdest;
{
  struct scmd s;
  char *zexclam;
  const char *zconst;
  char *zto;

  zexclam = strchr (zfile, '!');

  if (zexclam == NULL)
    {
      /* Make sure the user has access to this file, since we are
	 running setuid.  */
      if (! fsysdep_access (zfile))
	ucabort ();

      if (fClocaldest)
	{
	  /* Copy one local file to another.  */

	  /* Check that we have permission to receive into the desired
	     directory.  */
	  if (! fin_directory_list (&sLocalsys, zdest,
				    sLocalsys.zlocal_receive,
				    TRUE, FALSE, zCuser))
	    ulog (LOG_FATAL, "Not permitted to receive to %s", zdest);

	  zconst = zsysdep_real_file_name (&sLocalsys, zdest, zfile);
	  if (zconst == NULL)
	    ucabort ();
	  zto = xstrdup (zconst);

	  if (! fcopy_file (zfile, zto, FALSE, fCmkdirs))
	    ucabort ();

	  xfree ((pointer) zto);
	}
      else
	{
	  char abtname[CFILE_NAME_LEN];
	  unsigned int imode;

	  /* Copy a local file to a remote file.  We may have to
	     copy the local file to the spool directory.  */

	  imode = isysdep_file_mode (zfile);
	  if (imode == 0)
	    ucabort ();

	  if (! fCcopy)
	    {
	      /* Make sure the daemon will be permitted to send
		 this file.  */
	      if (! fsysdep_daemon_access (zfile))
		ucabort ();
	      if (! fin_directory_list (&sLocalsys, zfile,
					sLocalsys.zlocal_send,
					TRUE, TRUE, zCuser))
		ulog (LOG_FATAL, "Not permitted to send from %s",
		      zfile);
	      strcpy (abtname, "D.0");
	    }
	  else
	    {
	      char *zdata;

	      zconst = zsysdep_data_file_name (qCdestsys, bCgrade,
					       abtname, (char *) NULL,
					       (char *) NULL);
	      if (zconst == NULL)
		ucabort ();
	      zdata = xstrdup (zconst);

	      ucrecord_file (zdata);
	      if (! fcopy_file (zfile, zdata, FALSE, TRUE))
		ucabort ();

	      xfree ((pointer) zdata);
	    }

	  s.bcmd = 'S';
	  s.pseq = NULL;
	  s.zfrom = zfile;
	  s.zto = zdest;
	  s.zuser = zCuser;
	  s.zoptions = abCsend_options;
	  s.ztemp = xstrdup (abtname);
	  s.imode = imode;
	  s.znotify = zCnotify;
	  s.cbytes = -1;

	  ucadd_cmd (qCdestsys, &s);
	}
    }
  else
    {
      int clen;
      char *zcopy;
      struct ssysteminfo *qfromsys;

      if (! fCexpand)
	zconst = zexclam + 1;
      else
	{
	  /* Add the current directory to the filename if it's not
	     already there.  */
	  zconst = zsysdep_add_cwd (zexclam + 1, FALSE);
	  if (zconst == NULL)
	    ucabort ();
	  zconst = xstrdup (zconst);
	}

      /* Read the system information.  */
      clen = zexclam - zfile;
      zcopy = (char *) xmalloc (clen + 1);
      memcpy ((pointer) zcopy, (pointer) zfile, clen);
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
	ucabort ();

      if (fClocaldest)
	{
	  const char *zconst_to;

	  /* Fetch a file from a remote system.  */

	  /* Check that we have permission to receive into the desired
	     directory.  If we don't have permission, uucico will
	     fail.  */
	  if (! fin_directory_list (qfromsys, zdest,
				    qfromsys->zlocal_receive,
				    TRUE, FALSE, zCuser))
	    ulog (LOG_FATAL, "Not permitted to receive to %s", zdest);

	  /* If the remote filespec is wildcarded, we must generate an
	     'X' request.  We currently check for Unix shell
	     wildcards.  Note that it should do no harm to mistake a
	     non-wildcard for a wildcard.  */
	  if (zconst[strcspn (zconst, "*?[")] != '\0')
	    {
	      const char *zuse;

	      if (qfromsys->zlocalname != NULL)
		zuse = qfromsys->zlocalname;
	      else
		zuse = zLocalname;

	      s.bcmd = 'X';
	      zto = (char *) xmalloc (strlen (zuse)
				      + strlen (zdest)
				      + sizeof "!");
	      sprintf (zto, "%s!%s", zuse, zdest);
	      zconst_to = zto;
	    }
	  else
	    {
	      s.bcmd = 'R';
	      zconst_to = zdest;
	    }

	  s.pseq = NULL;
	  s.zfrom = zconst;
	  s.zto = zconst_to;
	  s.zuser = zCuser;
	  s.zoptions = abCrec_options;
	  s.ztemp = "";
	  s.imode = 0;
	  s.znotify = "";
	  s.cbytes = -1;

	  ucadd_cmd (qfromsys, &s);
	}
      else
	{
	  /* Move a file from one remote system to another.  */

	  zto = (char *) xmalloc (strlen (qCdestsys->zname)
				  + strlen (zdest)
				  + sizeof "!");
	  sprintf (zto, "%s!%s", qCdestsys->zname, zdest);

	  s.bcmd = 'X';
	  s.pseq = NULL;
	  s.zfrom = zconst;
	  s.zto = zdest;
	  s.zuser = zCuser;
	  s.zoptions = abCrec_options;
	  s.ztemp = "";
	  s.imode = 0;
	  s.znotify = "";
	  s.cbytes = -1;

	  ucadd_cmd (qfromsys, &s);
	}
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
ucspool_cmds (fjobid)
     boolean fjobid;
{
  struct sjob *qjob;
  const char *zjobid;

  for (qjob = qCjobs; qjob != NULL; qjob = qjob->qnext)
    {
      ulog_system (qjob->qsys->zname);
      zjobid = zsysdep_spool_commands (qjob->qsys, bCgrade, qjob->ccmds,
				       qjob->pascmds);
      if (zjobid != NULL)
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

	  if (fjobid)
	    printf ("%s\n", zjobid);
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

/* Keep track of all files we have created so that we can delete them
   if we get a signal.  The argument will be on the heap.  */

static int ccfiles;
static const char **pcaz;

static void
ucrecord_file (zfile)
     const char *zfile;
{
  pcaz = (const char **) xrealloc ((pointer) pcaz,
				   (ccfiles + 1) * sizeof (const char *));
  pcaz[ccfiles] = zfile;
  ++ccfiles;
}

/* Delete all the files we have recorded and exit.  */

static void
ucabort ()
{
  int i;

  for (i = 0; i < ccfiles; i++)
    (void) remove (pcaz[i]);
  ulog_close ();
  usysdep_exit (FALSE);
}
