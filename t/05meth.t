
$| = 1;
print "1..$tests\n";

require DBI;

my (@row);
my $SOLID_USER=$ENV{'SOLID_USER'};
$SOLID_USER = '' unless ($SOLID_USER);
my $SOLID_DSN=$ENV{'SOLID_DSN'};
$SOLID_DSN = '' unless ($SOLID_DSN);

# test user-supplied data.

my ($user, $pass) = split(/\W/, $SOLID_USER);
$user = uc($user);
print "not " unless($user && $pass);
print "ok 1\n";
print STDERR "Please define the SOLID_USER environment variable."
	unless ($user && $pass);
exit(1) unless($user && $pass);

my $dbh = DBI->connect($SOLID_DSN, $user, $pass, 'Solid'); 
print "not " unless($dbh);
print "ok 2\n";
exit(1) unless($dbh);

#### testing Tim's early draft DBI methods

my $r1 = $DBI::rows;
$dbh->{AutoCommit} = 0;
my $sth;
$sth = $dbh->prepare("DELETE FROM perl_dbd_test");
$sth->execute();
print "not " unless($sth->rows >= 0 
		    && $DBI::rows == $sth->rows);
$sth->finish();
$dbh->rollback();
print "ok 3\n";

$dbh->disconnect();

BEGIN { $tests = 3; }
