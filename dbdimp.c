/* $Id: dbdimp.c,v 1.12 1997/07/14 19:22:48 tom Exp $
 * 
 * Copyright (c) 1997  Thomas K. Wenrich
 * portions Copyright (c) 1994,1995,1996  Tim Bunce
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the Perl README file.
 *
 * Autocommit note:
 *   Solid 2.2 AUTOCOMMIT is broken (rollback in disconnect() 
 *   _MAY_ undo inserts done from within a solid procedure), so we 
 *   handle AutoCommit *additional* to the ODBC connection
 *   attribute (this is controlled by SOL22_AUTOCOMMIT_BUG definition
 *   within Makefile.PL).
 */

#include "Solid.h"


#define DESCRIBE_IN_PREPARE 1 
	/* Fixes problem with bind_columns immediate after prepare,
	 * but breaks $sth->{blob_size} attribute.
 	 */


typedef struct {
    const char *str;
    UWORD fOption;
    UDWORD true;
    UDWORD false;
} db_params;

typedef struct {
    const char *str;
    unsigned len:8;
    unsigned array:1;
    unsigned filler:23;
} T_st_params;

static const char *
S_SqlTypeToString (
    SWORD sqltype);
static const char *
S_SqlCTypeToString (
    SWORD sqltype);

DBISTATE_DECLARE;

void
dbd_init(dbistate)
    dbistate_t *dbistate;
{
    DBIS = dbistate;
    }

void
dbd_db_destroy(dbh)
    SV *dbh;
{
    D_imp_dbh(dbh);

    if (DBIc_ACTIVE(imp_dbh))
	{
	dbd_db_disconnect(dbh);
	}
    /* Nothing in imp_dbh to be freed	*/

    DBIc_IMPSET_off(imp_dbh);
}

/*------------------------------------------------------------
  connecting to a data source.
  Allocates henv and hdbc.
------------------------------------------------------------*/
int
dbd_db_login(dbh, dbname, uid, pwd)
    SV *dbh;
    char *dbname;
    char *uid;
    char *pwd;
{
    D_imp_dbh(dbh);
    D_imp_drh_from_dbh;
    int ret;

    RETCODE rc;

    static int s_first = 1;

    if (s_first)
	{
	s_first = 0;
	imp_drh->connects = 0;
	imp_drh->henv = SQL_NULL_HENV;
	}

    if (!imp_drh->connects)
	{
	rc = SQLAllocEnv(&imp_drh->henv);
	solid_error(dbh, rc, "db_login/SQLAllocEnv");
	if (rc != SQL_SUCCESS)
	    {
	    return 0;
	    }
	}

    rc = SQLAllocConnect(imp_drh->henv, &imp_dbh->hdbc);
    solid_error(dbh, rc, "db_login/SQLAllocConnect");
    if (rc != SQL_SUCCESS)
	{
	if (imp_drh->connects == 0)
	    {
	    SQLFreeEnv(imp_drh->henv);
	    imp_drh->henv = SQL_NULL_HENV;
	    }
	return 0;
	}

    if (dbis->debug >= 2)
	fprintf(DBILOGFP, "connect '%s', '%s', '%s'",
		dbname, uid, pwd);
    rc = SQLConnect(imp_dbh->hdbc,
		    dbname, strlen(dbname),
		    uid, strlen(uid),
		    pwd, strlen(pwd));

    solid_error(dbh, rc, "db_login/SQLConnect");
    if (rc != SQL_SUCCESS)
	{
	SQLFreeConnect(imp_dbh->hdbc);
	if (imp_drh->connects == 0)
	    {
	    SQLFreeEnv(imp_drh->henv);
	    imp_drh->henv = SQL_NULL_HENV;
	    }
	return 0;
	}

    /* DBI spec requires AutoCommit on
     */
    rc = SQLSetConnectOption(imp_dbh->hdbc, 
    			     SQL_AUTOCOMMIT, 
			     SQL_AUTOCOMMIT_ON);
    solid_error(dbh, rc, "dbd_db_login/SQLSetConnectOption");
    if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO)
    	{
	DBIc_on(imp_dbh, DBIcf_AutoCommit);
	}
    imp_drh->connects++;
    DBIc_IMPSET_on(imp_dbh);	/* imp_dbh set up now			*/
    DBIc_ACTIVE_on(imp_dbh);	/* call disconnect before freeing	*/
    return 1;
}

int
dbd_db_disconnect(dbh)
    SV *dbh;
{
    RETCODE rc;
    D_imp_dbh(dbh);
    D_imp_drh_from_dbh;

    /* We assume that disconnect will always work	*/
    /* since most errors imply already disconnected.	*/
    DBIc_ACTIVE_off(imp_dbh);

    /* DBI spec: rolling back or committing depends
     * on AutoCommit attribute
     */
#ifdef SOL22_AUTOCOMMIT_BUG
    rc = SQLTransact(imp_drh->henv, 
		     imp_dbh->hdbc,
		     DBIc_is(imp_dbh, DBIcf_AutoCommit) 
		     	? SQL_COMMIT : SQL_ROLLBACK);
    solid_error(dbh, rc, "db_disconnect/SQLTransact");
#else
    rc = SQLTransact(imp_drh->henv, 
		     imp_dbh->hdbc,
		     SQL_ROLLBACK);
    solid_error(dbh, rc, "db_disconnect/SQLTransact");
#endif
    rc = SQLDisconnect(imp_dbh->hdbc);
    solid_error(dbh, rc, "db_disconnect/SQLDisconnect");
    if (rc != SQL_SUCCESS)
	{
	return 0;
	}
    SQLFreeConnect(imp_dbh->hdbc);
    imp_dbh->hdbc = SQL_NULL_HDBC;
    imp_drh->connects--;
    if (imp_drh->connects == 0)
	{
	SQLFreeEnv(imp_drh->henv);
	}
    /* We don't free imp_dbh since a reference still exists	*/
    /* The DESTROY method is the only one to 'free' memory.	*/
    /* Note that statement objects may still exists for this dbh!	*/

    return 1;
    }

int
dbd_db_commit(dbh)
    SV *dbh;
{
    D_imp_dbh(dbh);
    D_imp_drh_from_dbh;
    RETCODE rc;

    rc = SQLTransact(imp_drh->henv, 
		     imp_dbh->hdbc,
		     SQL_COMMIT);
    solid_error(dbh, rc, "db_commit/SQLTransact");
    if (rc != SQL_SUCCESS)
	{
	return 0;
	}
    return 1;
}

int
dbd_db_rollback(dbh)
    SV *dbh;
{
    D_imp_dbh(dbh);
    D_imp_drh_from_dbh;
    RETCODE rc;

    rc = SQLTransact(imp_drh->henv, 
		     imp_dbh->hdbc,
		     SQL_ROLLBACK);
    solid_error(dbh, rc, "db_rollback/SQLTransact");
    if (rc != SQL_SUCCESS)
	{
	return 0;
	}
    return 1;
}

/*------------------------------------------------------------
  replacement for ora_error.
  empties entire ODBC error queue.
------------------------------------------------------------*/
void
solid_error(h, badrc, what)
    SV *h;
    RETCODE badrc;
    char *what;
{
    D_imp_xxh(h);

    struct imp_drh_st *drh = NULL;
    struct imp_dbh_st *dbh = NULL;
    struct imp_sth_st *sth = NULL;
    HENV henv = SQL_NULL_HENV;
    HDBC hdbc = SQL_NULL_HDBC;
    HSTMT hstmt = SQL_NULL_HSTMT;

    int i = 2;			/* 2..0 hstmt..henv */

    SDWORD NativeError;
    UCHAR ErrorMsg[SQL_MAX_MESSAGE_LENGTH];
    SWORD ErrorMsgMax = sizeof(ErrorMsg)-1;
    SWORD ErrorMsgLen;
    UCHAR sqlstate[10];

    SV *errstr = DBIc_ERRSTR(imp_xxh);

    sv_setpvn(errstr, ErrorMsg, 0);
    sv_setiv(DBIc_ERR(imp_xxh), (IV)badrc);
    /* 
     * sqlstate isn't set for SQL_NO_DATA returns.
     */
    strcpy(sqlstate, "00000");
    sv_setpvn(DBIc_STATE(imp_xxh), sqlstate, 5);
    
    switch(DBIc_TYPE(imp_xxh))
	{
	case DBIt_DR:
	    drh = (struct imp_drh_st *)(imp_xxh);
	    break;
	case DBIt_DB:
	    dbh = (struct imp_dbh_st *)(imp_xxh);
	    drh = (struct imp_drh_st *)(DBIc_PARENT_COM(dbh));
	    break;
	case DBIt_ST:
	    sth = (struct imp_sth_st *)(imp_xxh);
	    dbh = (struct imp_dbh_st *)(DBIc_PARENT_COM(sth));
	    drh = (struct imp_drh_st *)(DBIc_PARENT_COM(dbh));
	    break;
	}

    if (sth != NULL) hstmt = sth->hstmt;
    if (dbh != NULL) hdbc = dbh->hdbc;
    if (drh != NULL) henv = drh->henv;

    while (i >= 0)
	{
	RETCODE rc = 0;
	if (dbis->debug >= 3)
	    fprintf(DBILOGFP, "solid_error: badrc=%d rc=%d i=%d hstmt %d hdbc %d henv %d\n", 
	       badrc, rc, i,
	       hstmt, hdbc, henv);
	switch(i--)
	    {
	    case 2:
		if (hstmt == SQL_NULL_HSTMT)
		    continue;
		break;
	    case 1:
		hstmt = SQL_NULL_HSTMT;
		if (hdbc == SQL_NULL_HDBC)
		    continue;
		break;
	    case 0:
		hdbc = SQL_NULL_HDBC;
		if (henv == SQL_NULL_HENV)
		    continue;
		break;
	    }
	do  
	    {
	    rc = SQLError(henv, hdbc, hstmt,
			  sqlstate,
			  &NativeError,
			  ErrorMsg,
			  ErrorMsgMax,
			  &ErrorMsgLen);
	    if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO)
		{
		sv_catpvn(errstr, ErrorMsg, ErrorMsgLen);
		sv_catpv(errstr, "\n");

		sv_catpv(errstr, "(SQL-");
		sv_catpv(errstr, sqlstate);
		sv_catpv(errstr, ")\n");
		sv_setpvn(DBIc_STATE(imp_xxh), sqlstate, 5);
		if (dbis->debug >= 3)
	    	    fprintf(DBILOGFP, 
		        "solid_error values: sqlstate %0.5s\n",
				sqlstate);
		}
	    }
	while (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO);
	}

    if (badrc != SQL_SUCCESS && what) 
	{
	sv_catpv(errstr, "(DBD: ");
	sv_catpv(errstr, what);
	sprintf(ErrorMsg, " rc=%d", badrc);
	sv_catpv(errstr, ErrorMsg);
	sv_catpv(errstr, ")");
	}
    if (badrc == SQL_NO_DATA_FOUND)
    	{
	/* For SQLFetch(): 
	 *      Must clear ODBC error queue, but may not set 
	 * 	$h->err to suppress weird messages when PrintError 
	 *	is active.
	 */
        sv_setiv(DBIc_ERR(imp_xxh), (IV)0);
	}
    if (badrc != SQL_SUCCESS)
	{
	DBIh_EVENT2(h, ERROR_event, DBIc_ERR(imp_xxh), errstr);
	if (dbis->debug >= 2)
	    fprintf(DBILOGFP, 
		    "%s error %d recorded: %s\n",
		    what, badrc, SvPV(errstr,na));
	}
    }

/*-------------------------------------------------------------------------
  dbd_preparse: 
     - scan for placeholders (? and :xx style) and convert them to ?.
     - builds translation table to convert positional parameters of the 
       execute() call to :nn type placeholders.
  We need two data structures to translate this stuff:
     - a hash to convert positional parameters to placeholders
     - an array, representing the actual '?' query parameters.
     %param = (name1=>plh1, name2=>plh2, ..., name_n=>plh_n)   #
     @qm_param = (\$param{'name1'}, \$param{'name2'}, ...) 
-------------------------------------------------------------------------*/
void
dbd_preparse(imp_sth, statement)
    imp_sth_t *imp_sth;
    char *statement;
{
    bool in_literal = FALSE;
    char *src, *start, *dest;
    phs_t phs_tpl;
    SV *phs_sv;
    int idx=0, style=0, laststyle=0;
    int param = 0;
    STRLEN namelen;
    char name[256];
    SV **svpp;
    SV *svref;
    char ch;
    /* allocate room for copy of statement with spare capacity	*/
    /* for editing '?' or ':1' into ':p1' so we can use obndrv.	*/
    imp_sth->statement = (char*)safemalloc(strlen(statement)+1);

    /* initialise phs ready to be cloned per placeholder	*/
    memset(&phs_tpl, 0, sizeof(phs_tpl));
    phs_tpl.ftype = 1;	/* VARCHAR2 */

    src  = statement;
    dest = imp_sth->statement;
    while(*src) 
	{
	if (*src == '\'')
	    in_literal = ~in_literal;
	if ((*src != ':' && *src != '?') || in_literal) 
	    {
	    *dest++ = *src++;
	    continue;
	    }
	start = dest;			/* save name inc colon	*/ 
	ch = *src++;
	if (ch == '?')                /* X/Open standard	*/ 
	    {		
	    idx++;
	    sprintf(name, "%d", idx);
	    *dest++ = ch;
	    style = 3;
	    }
	else if (isDIGIT(*src))         /* ':1'		*/
	    {
	    char *p = name;
	    *dest++ = '?';
	    idx = atoi(src);
	    if (idx <= 0)
		croak("Placeholder :%d must be a positive number", idx);
	    while(isDIGIT(*src))
		*p++ = *src++;
	    *p = 0;
	    style = 1;
	    } 
	else if (isALNUM(*src))         /* ':foo'	*/
	    {
	    char *p = name;
	    *dest++ = '?';

	    while(isALNUM(*src))	/* includes '_'	*/
		*p++ = *src++;
	    *p = 0;
	    style = 2;
	    } 
	else 
	    {			/* perhaps ':=' PL/SQL construct */
	    *dest++ = ch;
	    continue;
	    }
	*dest = '\0';			/* handy for debugging	*/
	if (laststyle && style != laststyle)
	    croak("Can't mix placeholder styles (%d/%d)",style,laststyle);
	laststyle = style;

	if (imp_sth->params_hv == NULL)
	    imp_sth->params_hv = newHV();
	namelen = strlen(name);

	svpp = hv_fetch(imp_sth->params_hv, name, namelen, 0);
	if (svpp == NULL)
	    {
	    /* create SV holding the placeholder 
	     */
	    phs_tpl.sv = &sv_undef;
	    phs_sv = newSVpv((char*)&phs_tpl, sizeof(phs_tpl)+namelen+1);
	    strcpy( ((phs_t*)SvPVX(phs_sv))->name, name);

	    /* store placeholder to params_hv
	     */
	    svpp = hv_store(imp_sth->params_hv, name, namelen, phs_sv, 0);
	    }

	svref = newRV(*svpp);

	/* store reference to placeholder to params_av
	 */
	if (imp_sth->params_av == NULL)
	    imp_sth->params_av = newAV();
	av_push(imp_sth->params_av, svref);

	}
    *dest = '\0';
    if (imp_sth->params_hv) 
	{
	DBIc_NUM_PARAMS(imp_sth) = (int)HvKEYS(imp_sth->params_hv);
	if (dbis->debug >= 2)
	    fprintf(DBILOGFP, "    dbd_preparse scanned %d distinct placeholders\n",
		(int)DBIc_NUM_PARAMS(imp_sth));
	}
    }

int
dbd_st_prepare(sth, statement, attribs)
    SV *sth;
    char *statement;
    SV *attribs;
{
    D_imp_sth(sth);
    D_imp_dbh_from_sth;
    RETCODE rc;
    SV **svp;
    char cname[128];		/* cursorname */

    imp_sth->done_desc = 0;

    rc = SQLAllocStmt(imp_dbh->hdbc, &imp_sth->hstmt);
    solid_error(sth, rc, "st_prepare/SQLAllocStmt");
    if (rc != SQL_SUCCESS)
	{
	return 0;
	}

    /* scan statement for '?', ':1' and/or ':foo' style placeholders	*/
    dbd_preparse(imp_sth, statement);

    /* parse the (possibly edited) SQL statement */

    rc = SQLPrepare(imp_sth->hstmt, 
		    imp_sth->statement,
		    strlen(imp_sth->statement));
    solid_error(sth, rc, "st_prepare/SQLPrepare");
    if (rc != SQL_SUCCESS)
        {
	SQLFreeStmt(imp_sth->hstmt, SQL_DROP);
	imp_sth->hstmt = SQL_NULL_HSTMT;
	return 0;
	}

    imp_sth->long_buflen   = 80; /* typical  default	*/
    imp_sth->long_trunc_ok = 0;	/* can use blob_read()		*/
    
    if (dbis->debug >= 2)
	fprintf(DBILOGFP, "    dbd_st_prepare'd sql f%d\n\t%s\n",
		imp_sth->hstmt, imp_sth->statement);

    /* init sth pointers */
    imp_sth->fbh = NULL;
    imp_sth->ColNames = NULL;
    imp_sth->RowBuffer = NULL;
    imp_sth->n_result_cols = -1;
    imp_sth->RowCount = -1;
    imp_sth->eod = -1;

    sprintf(cname, "dbd_cursor_%X", imp_sth->hstmt);
    rc = SQLSetCursorName(imp_sth->hstmt, cname, strlen(cname));
    if (rc != SQL_SUCCESS)
	warn("dbd_prepare: can't set cursor name, rc = %d", rc);
    if (dbis->debug >= 2)
	fprintf(DBILOGFP, "    CursorName is '%s', rc=%d\n", 
		cname, rc);

    if (attribs)
	{
	if ((svp=hv_fetch((HV*)SvRV(attribs), "blob_size",9, 0)) != NULL)
	    {
	    int len = SvIV(*svp);
	    imp_sth->long_buflen = len;
	    if (DBIc_WARN(imp_sth))
	    	warn("depreciated feature: blob_size will be replaced by solid_blob_size\n");
	    }
	if ((svp=hv_fetch((HV*)SvRV(attribs), "solid_blob_size",15, 0)) != NULL)
	    {
	    int len = SvIV(*svp);
	    imp_sth->long_buflen = len;
	    }
	
#if YET_NOT_IMPLEMENTED
	if ((svp=hv_fetch((HV*)SvRV(attribs), "concurrency",11, 0)) != NULL)
	    {
	    UDWORD param = SvIV(*svp);
	    rc = SQLSetStmtOption(imp_sth->hstmt, SQL_CONCURRENCY, param);
	    if (rc != SQL_SUCCESS)
		warn("prepare: can't set concurrency, rc = %d", rc);
	    }
#endif
	}

#if DESCRIBE_IN_PREPARE
    if (dbd_describe(sth, imp_sth) <= 0)
	return 0;
#endif

    DBIc_IMPSET_on(imp_sth);
    return 1;
    }

int 
dbtype_is_string(int bind_type)
{
    switch(bind_type)
	{
	case SQL_C_CHAR:
	case SQL_C_BINARY:
	    return 1;
	}
    return 0;
    }    


static const char *
S_SqlTypeToString (SWORD sqltype)
{
    switch(sqltype)
	{
	case SQL_CHAR: return "CHAR";
	case SQL_NUMERIC: return "NUMERIC";
	case SQL_DECIMAL: return "DECIMAL";
	case SQL_INTEGER: return "INTEGER";
	case SQL_SMALLINT: return "SMALLINT";
	case SQL_FLOAT: return "FLOAT";
	case SQL_REAL: return "REAL";
	case SQL_DOUBLE: return "DOUBLE";
	case SQL_VARCHAR: return "VARCHAR";
	case SQL_DATE: return "DATE";
	case SQL_TIME: return "TIME";
	case SQL_TIMESTAMP: return "TIMESTAMP";
	case SQL_LONGVARCHAR: return "LONG VARCHAR";
	case SQL_BINARY: return "BINARY";
	case SQL_VARBINARY: return "VARBINARY";
	case SQL_LONGVARBINARY: return "LONG VARBINARY";
	case SQL_BIGINT: return "BIGINT";
	case SQL_TINYINT: return "TINYINT";
	case SQL_BIT: return "BIT";
	}
    return "unknown";
    }
static const char *
S_SqlCTypeToString (SWORD sqltype)
{
static char s_buf[100];
#define s_c(x) case x: return #x
    switch(sqltype)
	{
	s_c(SQL_C_CHAR);
	s_c(SQL_C_BIT);
	s_c(SQL_C_STINYINT);
	s_c(SQL_C_UTINYINT);
	s_c(SQL_C_SSHORT);
	s_c(SQL_C_USHORT);
	s_c(SQL_C_FLOAT);
	s_c(SQL_C_DOUBLE);
	s_c(SQL_C_BINARY);
	s_c(SQL_C_DATE);
	s_c(SQL_C_TIME);
	s_c(SQL_C_TIMESTAMP);
	}
#undef s_c
    sprintf(s_buf, "(unknown CType %d)", sqltype);
    return s_buf;
    }
/*
 * describes the output variables of a query,
 * allocates buffers for result rows,
 * and binds this buffers to the statement.
 */
int
dbd_describe(h, imp_sth)
    SV *h;
    imp_sth_t *imp_sth;
{
    RETCODE rc;

    UCHAR *cbuf_ptr;		
    UCHAR *rbuf_ptr;		

    int t_cbufl=0;		/* length of all column names */
    int i;
    imp_fbh_t *fbh;
    int t_dbsize = 0;		/* size of native type */
    int t_dsize = 0;		/* display size */

    if (imp_sth->done_desc)
	return 1;	/* success, already done it */
    imp_sth->done_desc = 1;

    rc = SQLNumResultCols(imp_sth->hstmt, &imp_sth->n_result_cols);
    solid_error(h, rc, "dbd_describe/SQLNumResultCols");
    if (rc != SQL_SUCCESS)
	{
	return 0;
	}

    if (dbis->debug >= 2)
	fprintf(DBILOGFP, "    dbd_describe sql %d: n_result_cols=%d\n",
		imp_sth->hstmt,
		imp_sth->n_result_cols);

    DBIc_NUM_FIELDS(imp_sth) = imp_sth->n_result_cols;

    if (imp_sth->n_result_cols == 0) 
	{
	if (dbis->debug >= 2)
	    fprintf(DBILOGFP, "\tdbd_describe skipped (no result cols) (sql f%d)\n",
		    imp_sth->hstmt);
	return 1;
	}

    /* allocate field buffers				*/
    Newz(42, imp_sth->fbh, imp_sth->n_result_cols, imp_fbh_t);

    /* Pass 1: Get space needed for field names, display buffer and dbuf */
    for (fbh=imp_sth->fbh, i=0; 
	 i<imp_sth->n_result_cols; 
	 i++, fbh++)
	{
	UCHAR ColName[256];

	rc = SQLDescribeCol(imp_sth->hstmt, 
			    i+1, 
			    ColName,
			    sizeof(ColName)-1,
			    &fbh->ColNameLen,
			    &fbh->ColSqlType,
			    &fbh->ColDef,
			    &fbh->ColScale,
			    &fbh->ColNullable);
        if (rc != SQL_SUCCESS)
	    break;
	ColName[fbh->ColNameLen] = 0;


	t_cbufl  += fbh->ColNameLen;

	rc = SQLColAttributes(imp_sth->hstmt,i+1,SQL_COLUMN_DISPLAY_SIZE,
                                NULL, 0, NULL ,&fbh->ColDisplaySize);
        if (rc != SQL_SUCCESS)
	    break;
	fbh->ColDisplaySize += 1; /* add terminator */

	rc = SQLColAttributes(imp_sth->hstmt,i+1,SQL_COLUMN_LENGTH,
                                NULL, 0, NULL ,&fbh->ColLength);
        if (rc != SQL_SUCCESS)
	    break;

	/* change fetched size for some types
	 */
	fbh->ftype = SQL_C_CHAR;
	switch(fbh->ColSqlType)
	    {
	    case SQL_LONGVARBINARY:
		fbh->ftype = SQL_C_BINARY;
		fbh->ColDisplaySize = imp_sth->long_buflen;
		break;
	    case SQL_LONGVARCHAR:
		fbh->ColDisplaySize = imp_sth->long_buflen+1;
		break;
	    case SQL_TIMESTAMP:
		fbh->ftype = SQL_C_TIMESTAMP;
		fbh->ColDisplaySize = sizeof(TIMESTAMP_STRUCT);
		break;
	    }
	if (fbh->ftype != SQL_C_CHAR)
	    {
	    t_dbsize += t_dbsize % sizeof(int);     /* alignment */
	    }
	t_dbsize += fbh->ColDisplaySize;

	if (dbis->debug >= 2)
	    fprintf(DBILOGFP, 
		    "\tdbd_describe: col %d: %s, Length=%d"
		    "\t\tDisp=%d, Prec=%d Scale=%d\n", 
		    i+1, S_SqlTypeToString(fbh->ColSqlType),
		    fbh->ColLength, fbh->ColDisplaySize,
		    fbh->ColDef, fbh->ColScale
		    );
	}

    /* allocate a buffer to hold all the column names	*/
    Newz(42, imp_sth->ColNames, t_cbufl + imp_sth->n_result_cols, UCHAR);
    /* allocate Row memory */
    Newz(42, imp_sth->RowBuffer, t_dbsize + imp_sth->n_result_cols, UCHAR);

    /* Second pass:
       - get column names
       - bind column output
     */

    cbuf_ptr = imp_sth->ColNames;
    rbuf_ptr = imp_sth->RowBuffer;

    for(i=0, fbh = imp_sth->fbh; 
	i < imp_sth->n_result_cols 
	&& rc == SQL_SUCCESS; 
	i++, fbh++)
	{
	int dbtype;

	switch(fbh->ftype)
	    {
	    case SQL_C_BINARY:
	    case SQL_C_TIMESTAMP:
		rbuf_ptr += (rbuf_ptr - imp_sth->RowBuffer) % sizeof(int);
		break;
	    }


	rc = SQLDescribeCol(imp_sth->hstmt, 
			    i+1, 
			    cbuf_ptr,
			    255,
			    &fbh->ColNameLen,
			    &fbh->ColSqlType,
			    &fbh->ColDef,
			    &fbh->ColScale,
			    &fbh->ColNullable);
	if (rc != SQL_SUCCESS)
	    break;
	
	fbh->ColName = cbuf_ptr;
	cbuf_ptr[fbh->ColNameLen] = 0;
	cbuf_ptr += fbh->ColNameLen+1;

	fbh->data = rbuf_ptr;
	rbuf_ptr += fbh->ColDisplaySize;

	/* Bind output column variables */
	rc = SQLBindCol(imp_sth->hstmt,
			i+1,
			fbh->ftype,
			fbh->data,
			fbh->ColDisplaySize,
			&fbh->datalen);
	if (dbis->debug >= 2)
	    fprintf(DBILOGFP, 
		    "\tdescribe/BindCol: col#%d-%s:\n\t\t"
		    "sqltype=%s, ctype=%s, maxlen=%d\n",
		    i+1, fbh->ColName,
		    S_SqlTypeToString(fbh->ColSqlType),
		    S_SqlCTypeToString(fbh->ftype),
		    fbh->ColDisplaySize
		    );
	solid_error(h, rc, "describe/SQLBindCol");
	if (rc != SQL_SUCCESS)
	    {
	    break;
	    }
	} /* end pass 2 */

    if (rc != SQL_SUCCESS)
	{
	warn("can't bind column %d (%s)",
	     i+1, fbh->ColName);
	return 0;
	}
    return 1;
    }

int
dbd_st_execute(sth)	/* <0 is error, >=0 is ok (row count) */
    SV *sth;
{
    D_imp_sth(sth);
    RETCODE rc;
    int debug = dbis->debug;

    if (!imp_sth->done_desc) 
	{
	/* describe and allocate storage for results (if any needed)	*/
	if (!dbd_describe(sth, imp_sth))
	    return -1; /* dbd_describe already called ora_error()	*/
	}

    /* bind input parameters */

    if (debug >= 2)
	fprintf(DBILOGFP,
	    "    dbd_st_execute (for sql f%d after)...\n",
			imp_sth->hstmt);

    rc = SQLExecute(imp_sth->hstmt);
    solid_error(sth, rc, "st_execute/SQLExecute");
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
	{
	return -1;
	}
    imp_sth->RowCount = -1;
    rc = SQLRowCount(imp_sth->hstmt, &imp_sth->RowCount);
    solid_error(sth, rc, "st_execute/SQLRowCount");
    if (rc != SQL_SUCCESS)
	{
	return -1;
	}

    DBIc_ACTIVE_on(imp_sth);	/* XXX should only set for select ?	*/
    imp_sth->eod = SQL_SUCCESS;
    return 1;
    }

/*----------------------------------------
 * running $sth->fetchrow()
 *----------------------------------------
 */
AV *
dbd_st_fetch(sth)
    SV *	sth;
{
    D_imp_sth(sth);
    int debug = dbis->debug;
    int i;
    AV *av;
    RETCODE rc;
    int num_fields;
    char cvbuf[512];
    char *p;

    /* Check that execute() was executed sucessfully. This also implies	*/
    /* that dbd_describe() executed sucessfuly so the memory buffers	*/
    /* are allocated and bound.						*/
    if ( !DBIc_ACTIVE(imp_sth) ) 
	{
	solid_error(sth, 0, "no statement executing");
	return Nullav;
	}
    
    rc = SQLFetch(imp_sth->hstmt);
    solid_error(sth, rc, "st_fetch/SQLFetch");
    if (dbis->debug >= 2)
	fprintf(DBILOGFP, "SQLFetch() returns %d\n",
		rc);
    switch(rc)
	{
	case SQL_SUCCESS:
	    imp_sth->eod = rc;
	    break;
	case SQL_SUCCESS_WITH_INFO:
	    imp_sth->eod = SQL_SUCCESS;
	    break;
	case SQL_NO_DATA_FOUND:
	    imp_sth->eod = rc;
	    return Nullav;
	default:
	    return Nullav;
	}

    if (imp_sth->RowCount == -1)
	imp_sth->RowCount = 0;

    imp_sth->RowCount++;

    av = DBIS->get_fbav(imp_sth);
    num_fields = AvFILL(av)+1;	/* ??? */

    for(i=0; i < num_fields; ++i) 
	{
	imp_fbh_t *fbh = &imp_sth->fbh[i];
	SV *sv = AvARRAY(av)[i]; /* Note: we (re)use the SV in the AV	*/

        if (dbis->debug >= 4)
	    fprintf(DBILOGFP, "fetch col#%d %s datalen=%d displ=%d\n",
		i, fbh->ColName, fbh->datalen, fbh->ColDisplaySize);
	if (fbh->datalen != SQL_NULL_DATA) 
	    {			/* the normal case		*/
	    TIMESTAMP_STRUCT *ts = (TIMESTAMP_STRUCT *)fbh->data;

	    if (fbh->datalen > fbh->ColDisplaySize)
	    	{ 
		/* truncated LONG ??? */
	        sv_setpvn(sv, (char*)fbh->data, fbh->ColDisplaySize);
	    	}
	    else switch(fbh->ftype)
	    	{
		case SQL_C_TIMESTAMP:
		    sprintf(cvbuf, "%04d-%02d-%02d %02d:%02d:%02d",
			    ts->year, ts->month, ts->day, 
			    ts->hour, ts->minute, ts->second,
			    ts->fraction);
		    sv_setpv(sv, cvbuf);
		    break;
		default:
		    if (fbh->ColSqlType == SQL_CHAR
		        && DBIc_is(imp_sth, DBIcf_ChopBlanks)
		        && fbh->datalen > 0)
		    	{
			int len = fbh->datalen;
			char *p0  = (char *)(fbh->data);
			char *p   = (char *)(fbh->data) + len;

			while (p-- != p0)
			    {
			    if (*p != ' ')
			    	break;
			    len--;
			    }
		        sv_setpvn(sv, p0, len);
			break;
			}
		    /* no ChopBlank */
		    sv_setpvn(sv, (char*)fbh->data, fbh->datalen);
		    break;
		}
	    }
	else 
	    {
	    SvOK_off(sv);
	    }
	}
    return av;
    }

int
dbd_st_finish(sth)
    SV *sth;
{
    D_imp_sth(sth);
    D_imp_dbh_from_sth;
    D_imp_drh_from_dbh;
    RETCODE rc;
    int ret = 0;

    /* Cancel further fetches from this cursor.                 */
    /* We don't close the cursor till DESTROY (dbd_st_destroy). */
    /* The application may re execute(...) it.                  */

    if (DBIc_ACTIVE(imp_sth) && imp_dbh->hdbc != SQL_NULL_HDBC)
	{
	rc = SQLFreeStmt(imp_sth->hstmt, SQL_CLOSE);
	solid_error(sth, rc, "st_finish/SQLFreeStmt(SQL_CLOSE)");

	if (rc == SQL_SUCCESS)
	    ret = 1;
#ifdef SOL22_AUTOCOMMIT_BUG
	if (DBIc_is(imp_dbh, DBIcf_AutoCommit))
	    {
    	    rc = SQLTransact(imp_drh->henv, 
		             imp_dbh->hdbc,
		             SQL_COMMIT);
	    }
#endif
	}
    DBIc_ACTIVE_off(imp_sth);

    return 1;
    }

void
dbd_st_destroy(sth)
    SV *sth;
{
    D_imp_sth(sth);
    D_imp_dbh_from_sth;
    D_imp_drh_from_dbh;
    RETCODE rc;

    /* SQLxxx functions dump core when no connection exists. This happens
     * when the db was disconnected before perl ending.
     */
    if (imp_dbh->hdbc != SQL_NULL_HDBC)
	{
	rc = SQLFreeStmt(imp_sth->hstmt, SQL_DROP);
	if (rc != SQL_SUCCESS)
	    {
	    warn("warning: DBD::Solid SQLFreeStmt(SQL_DROP) returns %d\n",
		 rc);
	    }
	}

    /* Free contents of imp_sth	*/

    Safefree(imp_sth->fbh);
    Safefree(imp_sth->ColNames);
    Safefree(imp_sth->RowBuffer);
    Safefree(imp_sth->statement);

    if (imp_sth->params_av)
	{
	av_undef(imp_sth->params_av);
	imp_sth->params_av = NULL;
	}

    if (imp_sth->params_hv)
	{
	hv_undef(imp_sth->params_hv);	
	imp_sth->params_hv = NULL;
	}

    DBIc_IMPSET_off(imp_sth);		/* let DBI know we've done it	*/
    }
/*------------------------------------------------------------
 * bind placeholder.
 *  Is called from Solid.xs execute()
 *  AND from Solid.xs bind_param()
 */
int
dbd_bind_ph(sth, ph_namesv, newvalue, attribs, is_inout, maxlen)
    SV *sth;
    SV *ph_namesv;		/* index of execute() parameter 1..n */
    SV *newvalue;
    SV *attribs;		/* may be set by Solid.xs bind_param call */
    int is_inout;		/* inout for procedure calls only */
    IV maxlen;			/* ??? */
{
    D_imp_sth(sth);
    SV **phs_svp;
    STRLEN name_len;
    char *name;
    char namebuf[30];
    phs_t *phs;

    if (SvNIOK(ph_namesv) ) 
	{	/* passed as a number	*/
	name = namebuf;
	sprintf(name, "%d", (int)SvIV(ph_namesv));
	name_len = strlen(name);
	} 
    else 
	{
	name = SvPV(ph_namesv, name_len);
	}

    if (dbis->debug >= 2)
	fprintf(DBILOGFP, "bind %s <== '%.200s' (attribs: %s)\n",
		name, SvPV(newvalue,na), attribs ? SvPV(attribs,na) : "" );

    phs_svp = hv_fetch(imp_sth->params_hv, name, name_len, 0);
    if (phs_svp == NULL)
	croak("Can't bind unknown placeholder '%s'", name);
    phs = (phs_t*)SvPVX(*phs_svp);	/* placeholder struct	*/

    if (phs->sv == &sv_undef)	/* first bind for this placeholder	*/
	{	
	phs->ftype = SQL_C_CHAR;  /* our default type VARCHAR2	*/
	phs->sv = newSV(0);
	}

    if (attribs) 
	{  /* only look for ora_type on first bind of var  */
	SV **svp;
	/* Setup / Clear attributes as defined by attribs.          */
	/* XXX If attribs is EMPTY then reset attribs to default?   */
	if ( (svp=hv_fetch((HV*)SvRV(attribs), "sol_type",8, 0)) != NULL) 
	    {
	    int dbd_type = SvIV(*svp);
	    if (!dbtype_is_string(dbd_type))        /* mean but safe
						     */
		croak("Can't bind %s, sol_type %d not a simple string type",                            phs->name, dbd_type);
	    phs->ftype = dbd_type;
            }
	}
 
    /* At the moment we always do sv_setsv() and rebind.	*/
    /* Later we may optimise this so that more often we can	*/
    /* just copy the value & length over and not rebind.	*/

    if (!SvOK(newvalue)) 
	{	/* undef == NULL		*/
	phs->isnull = 1;
	}
    else
	{
	sv_setsv(phs->sv, newvalue);
	}
    return _dbd_rebind_ph(sth, imp_sth, phs, maxlen);
    }

/* walks through param_av and binds each plh found
 */
int 
_dbd_rebind_ph(sth, imp_sth, phs, maxlen) 
    SV *sth;
    imp_sth_t *imp_sth;
    phs_t *phs;
    int maxlen;
{
    int n_qm;			/* number of '?' parameters */
    int avi;

    RETCODE rc;

    /* args of SQLBindParameter() call */
    SWORD fParamType;
    SWORD fCType;
    SWORD fSqlType;
    UDWORD cbColDef;
    SWORD ibScale;
    UCHAR *rgbValue;
    SDWORD cbValueMax;
    SDWORD *pcbValue;
    SWORD fNullable;

    n_qm = av_len(imp_sth->params_av) + 1;

    for (avi = 0; avi < n_qm; avi++)
	{
	STRLEN len;
	SV **ref = av_fetch(imp_sth->params_av, avi, 0);
	SV *refd;
	phs_t *phs_refd;

	refd = SvRV(*ref);
	phs_refd = (phs_t*)SvPVX(refd);	/* placeholder struct	*/

	if (phs_refd != phs)
	    continue;

	rc = SQLDescribeParam(imp_sth->hstmt,
			      avi+1,
			      &fSqlType,
			      &cbColDef,
			      &ibScale,
			      &fNullable);
	solid_error(sth, rc, "_rebind_ph/SQLDescribeParam");
	if (rc != SQL_SUCCESS)
	    return 0;

	fParamType = SQL_PARAM_INPUT;
	fCType = phs->ftype;

	/* When we fill a LONGVARBINARY, the CTYPE must be set 
	 * to SQL_C_BINARY.
	 */

	if (fCType == SQL_C_CHAR)	/* could be changed by bind_plh */
	    {
	    switch(fSqlType)
		{
		case SQL_LONGVARBINARY:
		    fCType = SQL_C_BINARY;
		    break;
		case SQL_LONGVARCHAR:
		    break;
	        case SQL_TIMESTAMP:
                case SQL_DATE:
                case SQL_TIME:
		    fSqlType = SQL_VARCHAR;
		    break;
		default:
		    break;
	        }
	    }
	pcbValue = &phs->cbValue;
	if (phs->isnull)
	    {
	    *pcbValue = SQL_NULL_DATA;
	    rgbValue = NULL;
	    }
	else
	    {
	    rgbValue = (UCHAR *)SvPV(phs->sv, len);
	    *pcbValue = (UDWORD) len;
	    }
	cbValueMax = 0;

	if (dbis->debug >=2)
	    fprintf(DBILOGFP,
		    "Bind: %d, CType=%d, SqlType=%s, ColDef=%d\n",
		    avi+1, fCType, 
		    S_SqlTypeToString(fSqlType), 
		    cbColDef);

	rc = SQLBindParameter(imp_sth->hstmt,
			      avi+1,
			      fParamType,
			      fCType,
			      fSqlType,
			      cbColDef,
			      ibScale,
			      rgbValue,
			      cbValueMax,
			      pcbValue);
	solid_error(sth, rc, "_rebind_ph/SQLBindParameter");
	if (rc != SQL_SUCCESS)
	    {
	    return 0;
	    }
	}


    return 1;
    }

int
dbd_st_rows(sth)
    SV *sth;
{
    D_imp_sth(sth);
    return imp_sth->RowCount;
}

/*------------------------------------------------------------
 * blob_read:
 * read part of a BLOB from a table.
 * GRRRRRRR - SOLID doesn't support this.
 */
dbd_st_blob_read(sth, field, offset, len, destrv, destoffset)
    SV *sth;
    int field;
    long offset;
    long len;
    SV *destrv;
    long destoffset;
{
    D_imp_sth(sth);
    SDWORD retl;
    SV *bufsv;
    RETCODE rc;

    bufsv = SvRV(destrv);
    sv_setpvn(bufsv,"",0);      /* ensure it's writable string  */
    SvGROW(bufsv, len+destoffset+1);    /* SvGROW doesn't do +1 */

    rc = SQLGetData(imp_sth->hstmt, (UWORD)field+1,
		    SQL_C_BINARY,
		    ((UCHAR *)SvPVX(bufsv)) + destoffset,
		    (SDWORD) len,
		    &retl);
    solid_error(sth, rc, "dbd_st_blob_read/SQLGetData");

    if (dbis->debug >= 2)
	fprintf(DBILOGFP, "SQLGetData(...,off=%d, len=%d)->rc=%d,len=%d SvCUR=%d\n",
		destoffset, len,
		rc, retl, SvCUR(bufsv));

    if (rc != SQL_SUCCESS) 
	{
	if (rc == SQL_SUCCESS_WITH_INFO)
	    {
	    retl = len;
	    }
	else
	    {
	    return 0;
	    }
        }

    SvCUR_set(bufsv, destoffset+retl);
    if (dbis->debug >= 2)
	fprintf(DBILOGFP, "blob_read: SvCUR=%d\n",
		SvCUR(bufsv));

    *SvEND(bufsv) = '\0'; /* consistent with perl sv_setpvn etc */
 
    return 1;
    }

/*----------------------------------------
 * db level interface
 * set connection attributes.
 *----------------------------------------
 */

static db_params S_db_storeOptions[] =  {
    { "AutoCommit", SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_ON, SQL_AUTOCOMMIT_OFF },
    { "solid_characterset", SQL_TRANSLATE_OPTION },
#if 0 /* not defined by DBI/DBD specification */
    { "TRANSACTION", 
                 SQL_ACCESS_MODE, SQL_MODE_READ_ONLY, SQL_MODE_READ_WRITE },
    { "solid_trace", SQL_OPT_TRACE, SQL_OPT_TRACE_ON, SQL_OPT_TRACE_OFF },
    { "solid_timeout", SQL_LOGIN_TIMEOUT },
    { "ISOLATION", SQL_TXN_ISOLATION },
    { "solid_tracefile", SQL_OPT_TRACEFILE },
#endif
    { NULL },
};

static const db_params *
S_dbOption(const db_params *pars, char *key, STRLEN len)
{
    /* search option to set */
    while (pars->str != NULL)
	{
	if (strncmp(pars->str, key, len) == 0
	    && len == strlen(pars->str))
	    break;
        pars++;
	}
    if (pars->str == NULL)
	return NULL;
    return pars;
    }
 
int
dbd_db_STORE(dbh, keysv, valuesv)
    SV *dbh;
    SV *keysv;
    SV *valuesv;
{
    D_imp_dbh(dbh);
    D_imp_drh_from_dbh;
    RETCODE rc;
    STRLEN kl;
    STRLEN plen;
    char *key = SvPV(keysv,kl);
    SV *cachesv = NULL;
    int on;
    UDWORD vParam;
    const db_params *pars;
    int parind;

    if ((pars = S_dbOption(S_db_storeOptions, key, kl)) == NULL)
	return FALSE;

    parind = pars - S_db_storeOptions;

    switch(pars->fOption)
	{
	case SQL_LOGIN_TIMEOUT:
	case SQL_TXN_ISOLATION:
	    vParam = SvIV(valuesv);
	    break;
	case SQL_OPT_TRACEFILE:
	    vParam = (UDWORD) SvPV(valuesv, plen);
	    break;
	case SQL_TRANSLATE_OPTION:
	    key = SvPV(valuesv, kl);
	    if (kl == 7 && !strncmp(key, "default", kl))
	    	vParam = SQL_SOLID_XLATOPT_DEFAULT;
	    else if (kl == 5 && !strncmp(key, "nocnv", kl))
	    	vParam = SQL_SOLID_XLATOPT_NOCNV;
	    else if (kl == 4 && !strncmp(key, "ansi", kl))
	    	vParam = SQL_SOLID_XLATOPT_ANSI;
	    else if (kl == 5 && !strncmp(key, "pcoem", kl))
	    	vParam = SQL_SOLID_XLATOPT_PCOEM;
	    else if (kl == 9 && !strncmp(key, "7bitscand", kl))
	    	vParam = SQL_SOLID_XLATOPT_7BITSCAND;
	    else
	        {
	    	warn("solid_characterset: invalid value '%.*s'\n", kl, key);
	        return FALSE;
		}
	    break;
	case SQL_AUTOCOMMIT:
	    on = SvTRUE(valuesv);
	    vParam = on ? pars->true : pars->false;
	    break;
	}

    rc = SQLSetConnectOption(imp_dbh->hdbc, pars->fOption, vParam);
    solid_error(dbh, rc, "db_STORE/SQLSetConnectOption");
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
	{
	return FALSE;
	}
    if (pars->fOption == SQL_AUTOCOMMIT)
    	{
	if (on) DBIc_set(imp_dbh, DBIcf_AutoCommit, 1);
	else    DBIc_set(imp_dbh, DBIcf_AutoCommit, 0);
	}
    return TRUE;
    }


static db_params S_db_fetchOptions[] =  {
    { "AutoCommit", SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_ON, SQL_AUTOCOMMIT_OFF },
#if 0 /* seems not supported by SOLID */
    { "sol_readonly", 
                 SQL_ACCESS_MODE, SQL_MODE_READ_ONLY, SQL_MODE_READ_WRITE },
    { "sol_trace", SQL_OPT_TRACE, SQL_OPT_TRACE_ON, SQL_OPT_TRACE_OFF },
    { "sol_timeout", SQL_LOGIN_TIMEOUT },
    { "sol_isolation", SQL_TXN_ISOLATION },
    { "sol_tracefile", SQL_OPT_TRACEFILE },
#endif
    { NULL }
};

SV *
dbd_db_FETCH(dbh, keysv)
    SV *dbh;
    SV *keysv;
{
    D_imp_dbh(dbh);
    D_imp_drh_from_dbh;
    RETCODE rc;
    STRLEN kl;
    STRLEN plen;
    char *key = SvPV(keysv,kl);
    int on;
    UDWORD vParam = 0;
    const db_params *pars;
    SV *retsv = NULL;

    if ((pars = S_dbOption(S_db_fetchOptions, key, kl)) == NULL)
	return Nullsv;

    /*
     * readonly, tracefile etc. isn't working yet. only AutoCommit supported.
     */

    if (pars->fOption == 0xffff)
    	{
	}
    else
    	{
        rc = SQLGetConnectOption(imp_dbh->hdbc, pars->fOption, &vParam);
        solid_error(dbh, rc, "db_FETCH/SQLGetConnectOption");
        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
	    {
	    if (dbis->debug >= 1)
	        fprintf(DBILOGFP,
		    "SQLGetConnectOption returned %d in dbd_db_FETCH\n", rc);
	    return Nullsv;
	    }
	}
    switch(pars->fOption)
	{
	case SQL_LOGIN_TIMEOUT:
	case SQL_TXN_ISOLATION:
	    newSViv(vParam);
	    break;
	case SQL_OPT_TRACEFILE:
	    retsv = newSVpv((char *)vParam, 0);
	    break;
	default:
	    if (vParam == pars->true)
		retsv = newSViv(1);
	    else
		retsv = newSViv(0);
	    break;
	} /* switch */
    return sv_2mortal(retsv);
    }

#define s_A(str) { str, sizeof(str)-1 }
static T_st_params S_st_fetch_params[] = 
{
    s_A("NUM_OF_PARAMS"),	/* 0 */
    s_A("NUM_OF_FIELDS"),	/* 1 */
    s_A("NAME"),		/* 2 */
    s_A("NULLABLE"),		/* 3 */
    s_A("TYPE"),		/* 4 */
    s_A("PRECISION"),		/* 5 */
    s_A("SCALE"),		/* 6 */
    s_A("sol_type"),		/* 7 */
    s_A("sol_length"),		/* 8 */
    s_A("CursorName"),		/* 9 */
    s_A("blob_size"),		/* 10 */
    s_A("__handled_by_dbi__"),	/* 11 */	/* ChopBlanks */
    s_A("solid_blob_size"),	/* 12 */
    s_A("solid_type"),		/* 13 */
    s_A("solid_length"),	/* 14 */
    s_A(""),			/* END */
};

static T_st_params S_st_store_params[] = 
{
    s_A("blob_size"),		/* 0 */
    s_A("solid_blob_size"),	/* 1 */
    s_A(""),			/* END */
};
#undef s_A

/*----------------------------------------
 * dummy routines st_XXXX
 *----------------------------------------
 */
SV *
dbd_st_FETCH(sth, keysv)
    SV *sth;
    SV *keysv;
{
    D_imp_sth(sth);
    STRLEN kl;
    char *key = SvPV(keysv,kl);
    int i;
    SV *retsv = NULL;
    T_st_params *par;
    int n_fields;
    imp_fbh_t *fbh;
    char cursor_name[256];
    SWORD cursor_name_len;
    RETCODE rc;
    int par_index;

    for (par = S_st_fetch_params; 
	 par->len > 0;
	 par++)
	if (par->len == kl && strEQ(key, par->str))
	    break;

    if (par->len <= 0)
	return Nullsv;

    if (!imp_sth->done_desc && !dbd_describe(sth, imp_sth)) 
	{
	/* dbd_describe has already called ora_error()          */
	/* we can't return Nullsv here because the xs code will */
	/* then just pass the attribute name to DBI for FETCH.  */
        croak("Describe failed during %s->FETCH(%s)",
                SvPV(sth,na), key);
	}

    i = DBIc_NUM_FIELDS(imp_sth);
 
    switch(par_index = par - S_st_fetch_params)
	{
	AV *av;

	case 0:			/* NUM_OF_PARAMS */
	    return Nullsv;	/* handled by DBI */
        case 1:			/* NUM_OF_FIELDS */
	    retsv = newSViv(i);
	    break;
	case 2: 			/* NAME */
	    av = newAV();
	    retsv = newRV(sv_2mortal((SV*)av));
	    while(--i >= 0)
		av_store(av, i, newSVpv(imp_sth->fbh[i].ColName, 0));
	    break;
	case 3:			/* NULLABLE */
	    av = newAV();
	    retsv = newRV(sv_2mortal((SV*)av));
	    while(--i >= 0) switch(imp_sth->fbh[i].ColNullable)
		{
		case SQL_NULLABLE:
		    av_store(av, i, &sv_yes);
		    break;
		case SQL_NO_NULLS:
		    av_store(av, i, &sv_no);
		    break;
		case SQL_NULLABLE_UNKNOWN:
		    av_store(av, i, &sv_undef);
		    break;
		}
	    break;
	case 4:			/* TYPE */
	    av = newAV();
	    retsv = newRV(sv_2mortal((SV*)av));
	    while(--i >= 0) 
		{
		int type = imp_sth->fbh[i].ColSqlType;
		av_store(av, i, newSViv(type));
		}
	    break;
        case 5:			/* PRECISION */
	    av = newAV();
	    retsv = newRV(sv_2mortal((SV*)av));
	    while(--i >= 0) 
		{
		av_store(av, i, newSViv(imp_sth->fbh[i].ColDef));
		}
	    break;
	case 6:			/* SCALE */
	    av = newAV();
	    retsv = newRV(sv_2mortal((SV*)av));
	    while(--i >= 0) 
		{
		av_store(av, i, newSViv(imp_sth->fbh[i].ColScale));
		}
	    break;
	case 7:			/* dbd_type */
	    if (DBIc_WARN(imp_sth))
	    	warn("Depreciated feature 'sol_type'. "
		     "Please use 'solid_type' instead.");
	    /* fall through */
	case 13:		/* solid_type */
	    av = newAV();
	    retsv = newRV(sv_2mortal((SV*)av));
	    while(--i >= 0) 
		{
		av_store(av, i, newSViv(imp_sth->fbh[i].ColSqlType));
		}
	    break;
	case 8:			/* dbd_length */
	    if (DBIc_WARN(imp_sth))
	    	warn("Depreciated feature 'sol_length'. "
		     "Please use 'solid_length' instead.");
	    /* fall through */
	case 14:		/* solid_length */
	    av = newAV();
	    retsv = newRV(sv_2mortal((SV*)av));
	    while(--i >= 0) 
		{
		av_store(av, i, newSViv(imp_sth->fbh[i].ColLength));
		}
	    break;
	case 9:			/* CursorName */
	    rc = SQLGetCursorName(imp_sth->hstmt,
				  cursor_name,
				  sizeof(cursor_name),
				  &cursor_name_len);
	    solid_error(sth, rc, "st_FETCH/SQLGetCursorName");
	    if (rc != SQL_SUCCESS)
		{
		if (dbis->debug >= 1)
		    {
		    fprintf(DBILOGFP,
			    "SQLGetCursorName returned %d in dbd_st_FETCH\n", 
			    rc);
		    }
		return Nullsv;
		}
	    retsv = newSVpv(cursor_name, cursor_name_len);
	    break;
	case 10:		/* blob_size */
	    if (DBIc_WARN(imp_sth))
	    	warn("Depreciated feature 'blob_size'. "
		     "Please use 'solid_blob_size' instead.");
	    /* fall through */
	case 12:		/* solid_blob_size */
	    retsv = newSViv(imp_sth->long_buflen);
	    break;
	default:
	    return Nullsv;
	}

    return sv_2mortal(retsv);
    }

int
dbd_st_STORE(sth, keysv, valuesv)
    SV *sth;
    SV *keysv;
    SV *valuesv;
{
    D_imp_sth(sth);
    D_imp_dbh_from_sth;
    STRLEN kl;
    STRLEN vl;
    char *key = SvPV(keysv,kl);
    char *value = SvPV(valuesv, vl);
    T_st_params *par;
    RETCODE rc;
 
    for (par = S_st_store_params; 
	 par->len > 0;
	 par++)
	if (par->len == kl && strEQ(key, par->str))
	    break;

    if (par->len <= 0)
	return FALSE;

    switch(par - S_st_store_params)
	{
	case 0:/* blob_size */
	case 1:/* solid_blob_size */
#if DESCRIBE_IN_PREPARE
	    warn("$sth->{blob_size} isn't longer supported.\n"
	         "You may either use the 'solid_blob_size' "
		 "attribute to prepare()\nor the blob_read() "
		 "function.\n");
	    return FALSE;
#endif
	    imp_sth->long_buflen = SvIV(valuesv);
	    return TRUE;
	}
    return FALSE;
    }

