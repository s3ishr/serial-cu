/* diasub.c
   Dialer information subroutines.

   Copyright (C) 1992 Ian Lance Taylor

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

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_diasub_rcsid[] = "$Id$";
#endif

/* Clear the information in a dialer.  */

void
_uuconf_uclear_dialer (struct uuconf_dialer *qdialer)
{
  qdialer->uuconf_zname = NULL;
  qdialer->uuconf_zdialtone = (char *) ",";
  qdialer->uuconf_zpause = (char *) ",";
  qdialer->uuconf_fcarrier = TRUE;
  qdialer->uuconf_ccarrier_wait = 60;
  qdialer->uuconf_fdtr_toggle = FALSE;
  qdialer->uuconf_fdtr_toggle_wait = FALSE;
  qdialer->uuconf_qproto_params = NULL;
  /* Note that we do not set RELIABLE_SPECIFIED; this just sets
     defaults, so that ``seven-bit true'' does not imply ``reliable
     false''.  */
  qdialer->uuconf_ireliable = (UUCONF_RELIABLE_RELIABLE
			       | UUCONF_RELIABLE_EIGHT
			       | UUCONF_RELIABLE_FULLDUPLEX);
  qdialer->uuconf_palloc = NULL;
}
