#!/usr/bin/perl -w

$|=1;
print "1..$tests\n";

require DBI;

my $SOLID_USER=$ENV{'SOLID_USER'};
$SOLID_USER = '' unless ($SOLID_USER);
my $SOLID_DSN=$ENV{'SOLID_DSN'};
$SOLID_DSN = '' unless ($SOLID_DSN);

# test user-supplied data.

my ($user, $pass) = split(/\W/, $SOLID_USER);
$user = uc($user);
print "not " unless($user && $pass);
print "ok 1\n";
print STDERR "Please define the SOLID_USER environment variable.\n"
	unless ($user && $pass);

my $dbh = DBI->connect($SOLID_DSN, $user, $pass, 'Solid');
print "not " unless($dbh);
print "ok 2\n";

open(PROG, $0);		# get some long data for testing
my $longdata='';
while (<PROG>)
    {
    $longdata .= $_;
    }
close(PROG);

#--------------------------------------------------
# test inserting LONG VARBINARY
# 1. using DBD::Solid's default bindings
#--------------------------------------------------
$dbh->do('drop table blob_test');
$dbh->do(<<"/");
CREATE TABLE blob_test (
    A integer,
    LVB LONG VARBINARY,
    LVC LONG VARCHAR)
/
$sth = $dbh->prepare(<<"/");
INSERT INTO blob_test(A,LVB) VALUES(:1, :2)
/
unless ($sth->execute(1, $longdata))
    {
    print STDERR $DBI::errstr, "\n";
    print "not ";
    }
print "ok 3\n";
#-----------------
# rebind works ?
#-----------------
unless ($sth->execute(2, $longdata))
    {
    print STDERR $DBI::errstr, "\n";
    print "not ";
    }
print "ok 4\n";
$sth->finish();

$dbh->commit();

#------------------------
# is this really there ?
#------------------------
$sth = $dbh->prepare("SELECT A, LVB FROM blob_test WHERE A=:1");
$sth->{blob_size} = 4096;
if ($sth->execute(1) && (@row = $sth->fetchrow()))
    {
    print "not " unless ($row[1] eq $longdata);
    }
else
    {
    print STDERR $DBI::errstr, "\n";
    print "not ";
    }
print "ok 5\n";
$sth->finish();

#
# 
#
$sth->{blob_size} = 80;
$sth->execute(1);
$sth->fetchrow();
my $offset = 0;
my $blob = "";
while ($frag = $sth->blob_read(1, $offset, 100))
    {
    $offset += length($frag);
    $blob .= $frag;
    }
print "not " unless $blob eq $longdata;
print "ok 6\n";
$sth->finish();

BEGIN { $tests = 6; }

$dbh->disconnect();
