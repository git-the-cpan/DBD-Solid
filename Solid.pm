# $Id: Solid.pm,v 1.6 1997/05/10 06:31:30 tom Exp $
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

    $VERSION = '0.05';
    my $Revision = substr(q$Revision: 1.6 $, 10);

    require_version DBD::Solid::Const 0.03;
    require_version DBI 0.78;

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
		  FROM  tables");
	$sth->{blob_size} = 4096;
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
