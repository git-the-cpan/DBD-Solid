#!/usr/bin/perl -w -I./t

$|=1;
print "1..$tests\n";

require DBI;
use testenv;

my ($dsn, $user, $pass) = soluser();
print "ok 1\n";

my $dbh = DBI->connect($dsn, $user, $pass, 'Solid');
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
$sth = $dbh->prepare("SELECT A, LVB FROM blob_test WHERE A=:1");
$sth->{blob_size} = 80;
$sth->execute(1);
$sth->fetchrow();
my $offset = 0;
my $blob = "";
while ($frag = $sth->blob_read(1, $offset, 100))
    {
    $offset += length($frag);
    $blob .= $frag;
    last if ($sth->err == 100);		# end of data
    }
print "not " unless $blob eq $longdata;
print "ok 6\n";
$sth->finish();

$sth = $dbh->prepare("SELECT A, LVB FROM blob_test WHERE A=:1");
$sth->{blob_size} = 64;
$sth->execute(1);
my ($x, $y);
$sth->bind_columns(undef, \$x, \$y);
if ($sth->fetch())
    {
    print " expect string data right truncation error\n";
    print " err, state: ", $sth->err, ",", $dbh->state, "\n";
    print " errstr: ", $dbh->errstr, "\n";
    # print " y: >>", $y, "<<\n";
    # print "longdata: >>", substr($longdata, 0, 64), "<<\n";
    print "not " unless($y eq substr($longdata, 0, 64) && $sth->err);
    }
$sth->finish();
print "ok 7\n";

BEGIN { $tests = 7; }

$dbh->disconnect();
