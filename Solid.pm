# $Id: Solid.pm,v 1.10 1997/07/14 19:22:48 tom Exp $
# Copyright (c) 1997  Thomas K. Wenrich
# portions Copyright (c) 1994,1995,1996  Tim Bunce
#
# You may distribute under the terms of either the GNU General Public
# License or the Artistic License, as specified in the Perl README file.
#
require 5.003;
{
    package DBD::Solid;

    use DBI ();
    use DynaLoader ();
    use DBD::Solid::Const qw(:sql_types);

    @ISA = qw(DynaLoader);

    $VERSION = '0.07';
    my $Revision = substr(q$Revision: 1.10 $, 10);

    require_version DBD::Solid::Const 0.03;
    require_version DBI 0.84;

    bootstrap DBD::Solid $VERSION;

    $err = 0;		# holds error code   for DBI::err
    $errstr = "";	# holds error string for DBI::errstr
    $sqlstate = "00000";
    $drh = undef;	# holds driver handle once initialised

    sub driver{
	return $drh if $drh;
	my($class, $attr) = @_;

	$class .= "::dr";

	# not a 'my' since we use it above to prevent multiple drivers

	$drh = DBI::_new_drh($class, {
	    'Name' => 'Solid',
	    'Version' => $VERSION,
	    'Err'    => \$DBD::Solid::err,
	    'Errstr' => \$DBD::Solid::errstr,
	    'State' => \$DBD::Solid::sqlstate,
	    'Attribution' => 'Solid DBD by Thomas K. Wenrich',
	    });

	$drh;
    }

    1;
}


{   package DBD::Solid::dr; # ====== DRIVER ======
    use strict;

    sub errstr {
	DBD::Solid::errstr(@_);
    }
    sub err {
	DBD::Solid::err(@_);
    }

    sub connect {
	my($drh, $dbname, $user, $auth)= @_;

	if ($dbname){	# application is asking for specific database
	}

	# create a 'blank' dbh

	my $this = DBI::_new_dbh($drh, {
	    'Name' => $dbname,
	    'USER' => $user, 
	    'CURRENT_USER' => $user,
	    });

	# Call Solid logon func in Solid.xs file
	# and populate internal handle data.

	DBD::Solid::db::_login($this, $dbname, $user, $auth)
	    or return undef;

	$this;
    }

}


{   package DBD::Solid::db; # ====== DATABASE ======
    use strict;

    sub errstr {
	DBD::Solid::errstr(@_);
    }

    sub prepare {
	my($dbh, $statement, @attribs)= @_;

	# create a 'blank' dbh

	my $sth = DBI::_new_sth($dbh, {
	    'Statement' => $statement,
	    });

	# Call Solid OCI oparse func in Solid.xs file.
	# (This will actually also call oopen for you.)
	# and populate internal handle data.

	DBD::Solid::st::_prepare($sth, $statement, @attribs)
	    or return undef;

	$sth;
    }

    sub tables {
	my($dbh) = @_;		# XXX add qualification
	my $sth = $dbh->prepare("select
		        table_catalog TABLE_CAT,
			table_schema  TABLE_SCHEMA,
			table_name,
			table_type,
			remarks TABLE_REMARKS
		  FROM  tables",
		  {'solid_blob_size' => 4096,
		  });
	$sth->execute or return undef;
	$sth;
    }

}


{   package DBD::Solid::st; # ====== STATEMENT ======
    use strict;

    sub errstr {
	DBD::Solid::errstr(@_);
    }
}
1;
__END__
# Below is the stub of documentation for your module. You better edit it!

=head1 NAME

DBD::Solid - DBD driver to access Solid database

=head1 SYNOPSIS

  require DBI;

  $dbh = DBI->connect('DBI:Solid:' . $database, $user, $pass);
  $dbh = DBI->connect($database, $user, $pass, 'Solid');

=head1 DESCRIPTION

This module is the low level driver to access the Solid database 
using the DBI interface. Please refer to the DBI documentation
for using it.

=head1 REFERENCE

=over 4

=item Driver Level functions

  $dbh = DBI->connect('DBI:Solid:', $user, $pass);
  $dbh = DBI->connect('', $user, $pass, 'Solid');

	Connects to a local database.

  $dbh = DBI->connect('DBI:Solid:TCP/IP somewhere.com 1313', 
		      $user, $pass);
  $dbh = DBI->connect('TCP/IP somewhere.com 1313',
                      $user, $pass, 'Solid');

	Connects via tcp/ip to remote database listening on
	port 1313 at host "somewhere.com".
	NOTE: It depends on the Solid license whether 
	      TCP connections to 'localhost' are possible.

=item Common handle functions

  $h->err		full support
  $h->errstr		full support
  $h->state		full support

  $h->{Warn}		used to deactivate 'Depreciated 
			feature' warnings
  $h->{CompatMode}	not used
  $h->{InactiveDestroy}	handled by DBI ?
  $h->{PrintError}	handled by DBI ?
  $h->{RaiseError}	handled by DBI ?
  $h->{ChopBlanks}	full support
  $h->trace(...)	handled by DBI
  $h->func(...)		no functions defined yet

=item Database handle functions

  $sth = $dbh->prepare(	        	full support
		$statement)		
  $sth = $dbh->prepare(			full support
		$statement, 
		\%attr);

	DBD::Solid note: As the DBD driver looks for 
	placeholders within the statement, additional 
	to the ANSI style '?' placeholders the Solid 
	driver can parse :1, :2 and :foo style 
	placeholders (like Oracle).

 	\%attr values:

	{solid_blob_size => number}

	May be useful when you know that the LONG values 
	fetched from the query will have a maximum size.
	Allows to handle LONG columns like any other 
	column.
	pre 0.07 DBD drivers use the 'blob_size' syntax, 
	which is depreciated since DBD::Solid 0.07.

  $rc = $dbh->do($statement)		full support
  $rc = $dbh->commit()			full support
  $rc = $dbh->rollback()		full support
  $dbh->{AutoCommit}			full support

  $dbh->{solid_characterset} = $charset;

	This is a quick hack to activate Solid's 
	characterset translation, just in the case 
	Solid doesn't guess the default translation 
	(based on operating system and adjustable 
	by a solid.ini in the working directory) 
        right.

	Possible values are:

	$charset = 'default';
	$charset = 'nocnv';
	$charset = 'ansi';
	$charset = 'pcoem';
	$charset = '7bitscand';

  $rc = $dbh->disconnect()		full support
	does a ROLLBACK, so the application must
	commit the transaction before calling 
	disconnect

  $rc = $dbh->ping()			no support
					(due to ongoing 
					discussion about)

  $rc = $dbh->quote()			handled by DBI
  $rc = $sth->execute()			full support
  @array    = $sth->fetchrow_array()	full support
  @array    = $sth->fetchrow()		full support
  $arrayref = $sth->fetchrow_arrayref()	handled by DBI
  $hashref  = $sth->fetchrow_hashref()	handled by DBI
  $tbl_ary_ref = $sth->fetch_all()	handled by DBI
  $sth->rows()				full support

  $rv = $sth->bind_col(                  full support
	$column_number,
	\$var_to_bind);			

  $rv = $sth->bind_col(                  no attr defined yet
	$column_number, 
	\$var_to_bind, 
	\%attr);			

  $rv = $sth->bind_columns(              full support
	\%attr, 
	@refs_to_vars_to_bind);		

  $sth->{NUM_OF_FIELDS}			full support
  $sth->{NUM_OF_PARAMS}			full support
  $sth->{NAME}				full support
  $sth->{NULLABLE}			full support
  $sth->{CursorName}			full support

=head1 AUTHOR

T.Wenrich, wenrich@ping.at or wet@timeware.co.at

=head1 SEE ALSO

perl(1), DBI(perldoc), DBD::Solid::Const(perldoc), Solid documentation

=cut

