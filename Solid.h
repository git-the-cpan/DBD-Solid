/*
 * $Id: Solid.h,v 1.2 1999/08/04 04:29:25 joe Exp $
 * Copyright (c) 1997  Thomas K. Wenrich
 * portions Copyright (c) 1994,1995,1996  Tim Bunce
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the Perl README file.
 *
 */
#define NEED_DBIXS_VERSION 7

#include <DBIXS.h>		/* installed by the DBI module	*/
#include <cli0cli.h>
#include <cli0defs.h>
#include <cli0env.h>

#include "dbdimp.h"

#ifndef DBIc_IADESTROY		/* IADESTROY added after DBI-0.87 */
#define DBIc_IADESTROY(x) 0
#endif


void dbd_init _((dbistate_t *dbistate));

int  dbd_db_login _((SV *dbh, char *dbname, char *uid, char *pwd));
int  dbd_db_do _((SV *sv, char *statement));
int  dbd_db_commit _((SV *dbh));
int  dbd_db_rollback _((SV *dbh));
int  dbd_db_disconnect _((SV *dbh));
void dbd_db_destroy _((SV *dbh));
int  dbd_db_STORE _((SV *dbh, SV *keysv, SV *valuesv));
SV  *dbd_db_FETCH _((SV *dbh, SV *keysv));


int  dbd_st_prepare _((SV *sth, char *statement, SV *attribs));
int  dbd_st_rows _((SV *sv));
int  dbd_bind_ph _((SV *h, SV *param, SV *value, SV *attribs, int is_inout, IV maxlen));
int  dbd_st_execute _((SV *sv));
AV  *dbd_st_fetch _((SV *sv));
int  dbd_st_finish _((SV *sth));
void dbd_st_destroy _((SV *sth));
int  dbd_st_readblob _((SV *sth, int field, long offset, long len,
			SV *destrv, long destoffset));
int  dbd_st_STORE _((SV *dbh, SV *keysv, SV *valuesv));
SV  *dbd_st_FETCH _((SV *dbh, SV *keysv));


/* end of Solid.h */
