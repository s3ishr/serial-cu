# This is the auto-Makefile for the uuconf subdirectory of Taylor UUCP.

noinst_LIBRARIES = libuuconf.a

libuuconf_a_SOURCES = addblk.c addstr.c allblk.c alloc.c base.c bool.c \
	callin.c calout.c cmdarg.c cmdfil.c cmdlin.c cnfnms.c \
	debfil.c deblev.c errno.c errstr.c \
	filnam.c freblk.c free.c freprt.c fresys.c grdcmp.c \
	iniglb.c init.c int.c lckdir.c lineno.c llocnm.c \
	local.c locnm.c logfil.c maxuxq.c mrgblk.c paramc.c port.c \
	prtsub.c pubdir.c rdlocs.c reliab.c remunk.c runuxq.c \
	sinfo.c snams.c split.c spool.c stafil.c strip.c syssub.c \
	tcalou.c tgcmp.c thread.c time.c \
	tinit.c tlocnm.c tport.c tportc.c tsinfo.c tsnams.c tsys.c \
	tval.c ugtlin.c unk.c val.c alloc.h syshdr.h uucnfi.h

AM_CFLAGS = -I.. -I$(srcdir)/.. $(WARN_CFLAGS) -DNEWCONFIGLIB=\"$(NEWCONFIGDIR)\" -DOLDCONFIGLIB=\"$(OLDCONFIGDIR)\"
