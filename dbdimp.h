/*
 * $Id: dbdimp.h,v 1.5 1997/03/20 01:11:25 tom Exp $
 * Copyright (c) 1997  Thomas K. Wenrich
 * portions Copyright (c) 1994,1995,1996  Tim Bunce
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the Perl README file.
 *
 */
typedef struct imp_fbh_st imp_fbh_t;

/* This holds global data of the driver itself.
 */
struct imp_drh_st {
    dbih_drc_t com;		/* MUST be first element in structure	*/
    HENV henv;
    int connects;		/* connect count */
};

/* Define dbh implementor data structure 
   This holds everything to describe the database connection.
 */
struct imp_dbh_st {
    dbih_dbc_t com;		/* MUST be first element in structure	*/
    HDBC hdbc;
};


/* Define sth implementor data structure */
struct imp_sth_st {
    dbih_stc_t com;		/* MUST be first element in structure	*/

    HSTMT      hstmt;
    int        done_desc;   /* have we described this sth yet ?	*/

    /* Input Details	*/
    char      *statement;	/* sql (see sth_scan)		*/
    HV        *params_hv;	/* see preparse function */
    AV        *params_av;	/* see preparse function */

    int       long_buflen;      /* length for long/longraw (if >0)	*/
    int       long_trunc_ok;    /* is truncating a long an error	*/

    SWORD     n_result_cols;	/* number of result columns */

    UCHAR    *ColNames;		/* holds all column names; is referenced
				 * by ptrs from within the fbh structures
				 */
    UCHAR    *RowBuffer;	/* holds row data; referenced from fbh */
    imp_fbh_t *fbh;		/* array of imp_fbh_t structs	*/

    SDWORD   RowCount;		/* Rows affected by insert, update, delete
				 * (unreliable for SELECT)
				 */
    int eod;			/* End of data seen */
#if 0
    Cda_Def *cda;	/* currently just points to cdabuf below */
    Cda_Def cdabuf;


    /* Select Column Output Details	*/
    char      *fbh_cbuf;    /* memory for all field names       */
    int       t_dbsize;     /* raw data width of a row		*/
    /* Select Row Cache Details */
    int       cache_size;
    int       in_cache;
    int       next_entry;
    int       eod_errno;

    /* (In/)Out Parameter Details */
    bool  has_inout_params;
#endif
};
#define IMP_STH_EXECUTING	0x0001


#if 0
typedef struct fb_ary_st fb_ary_t;    /* field buffer array	*/
struct fb_ary_st { 	/* field buffer array EXPERIMENTAL	*/
    ub2  bufl;		/* length of data buffer		*/
    sb2  *aindp;	/* null/trunc indicator variable	*/
    ub1  *abuf;		/* data buffer (points to sv data)	*/
    ub2  *arlen;	/* length of returned data		*/
    ub2  *arcode;	/* field level error status		*/
};
#endif

struct imp_fbh_st { 	/* field buffer EXPERIMENTAL */
    imp_sth_t *imp_sth;	/* 'parent' statement */
    /* Solid's field description - SQLDescribeCol() */
    UCHAR *ColName;		/* zero-terminated column name */
    SWORD ColNameLen;
    UDWORD ColDef;		/* precision */
    SWORD ColScale;
    SWORD ColSqlType;
    SWORD ColNullable;
    SDWORD ColLength;		/* SqlColAttributes(SQL_COLUMN_LENGTH) */
    SDWORD ColDisplaySize;	/* SqlColAttributes(SQL_COLUMN_DISPLAY_SIZE) */
    /* Our storage space for the field data as it's fetched	*/
    SWORD ftype;		/* external datatype we wish to get.
				 * Used as parameter to SQLBindCol().
				 */
    UCHAR *data;		/* points into sth->RowBuffer */
    SDWORD datalen;		/* length returned from fetch for
				 * single row.
				 */

    
};


typedef struct phs_st phs_t;    /* scalar placeholder   */

struct phs_st {  	/* scalar placeholder EXPERIMENTAL	*/
    SWORD ftype;        /* external field type	       */
    SV	*sv;		/* the scalar holding the value		*/
    int isnull;
    SDWORD cbValue;	/* length of returned value */
                        /* in Input: SQL_NULL_DATA */
    char name[1];	/* struct is malloc'd bigger as needed	*/
#if 0
    sword ftype;	


    sb2 indp;		/* null indicator			*/
    char *progv;
    ub2 arcode;
    ub2 alen;

    bool is_inout;
    int alen_incnull;	/* 0 or 1 if alen should include null	*/
#endif
};




void	solid_error _((SV *h, RETCODE rc, char *what));
#if 0
void	ora_error _((SV *h, Lda_Def *lda, int rc, char *what));
void	fbh_dump _((imp_fbh_t *fbh, int i, int cacheidx));

void	dbd_init _((dbistate_t *dbistate));
void	dbd_preparse _((imp_sth_t *imp_sth, char *statement));
int 	dbd_describe _((SV *h, imp_sth_t *imp_sth));
int 	dbd_st_blob_read _((SV *sth, int field, long offset, long len,
			SV *destrv, long destoffset));

#endif
/* end */
