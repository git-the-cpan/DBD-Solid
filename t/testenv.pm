#
# testenv.pm
#
# environment for DBD::Solid tests
#

package testenv;
# use strict;
require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(soluser);

sub soluser
    {
    my ($user, $pass, $dsn, $dbh);
    $user = $ENV{'SOLID_USER'} or $user = '';
    $dsn = $ENV{'SOLID_DSN'} or $dsn = '';

    # test user-supplied data.
    ($user, $pass) = split(/\W/, $user);
    print "not " unless($user && $pass);
    $user = uc($user);
    unless ($user && $pass)
        {
        print STDERR "Please define the SOLID_USER environment variable.\n";
	exit(0);
	}
    ($dsn, $user, $pass);
    }
1;
