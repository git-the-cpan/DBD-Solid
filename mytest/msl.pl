require DBI;
require "./insert.pm";

$| = 1;

my $dbh;
$dbh = DBI->connect('', 'test', 'test', 'Solid')
       or die($DBI::errstr) unless($dbh);

print "*** MSL: running CREATE\n";
msl_create($dbh);

$dbh->{AutoCommit} = 0;			# AutoCommit isn't fast

print "*** MSL: running INSERT\n";
my $table = 'message_log';
my $data = "gzip -d <$table.pl.gz |";
# my $data = "msl.dat";
my $colref = columns($dbh, $table);

my $t1 = time();
my $cnt = insert($dbh, $table, $colref, $data);
my $t2 = time();
my $dt = $t2 - $t1;


print "*** MSL: check overhead\n";
my $t1 = time();
my $cnt = insert($dbh, $table, $colref, $data, 1);
my $t2 = time();
$dt -= $t2 - $t1;
print sprintf("Insert AutoCommit=%d ($cnt rows) %d seconds, %02d:%02d\n",
	$dbh->{AutoCommit},
	$dt, $dt/60, $dt%60);

$dbh->disconnect() 
    or die($DBI->errstr);

sub msl_create
{
my $dbh=shift;
$dbh->do('drop table message_log');

$dbh->do(<<"/") or die ($DBI::errstr);
CREATE TABLE "MESSAGE_LOG" (
	"MSL_TIME" TIMESTAMP NOT NULL, 
	"MSL_LFDN" INTEGER(5) NOT NULL, 
	"MSL_ADAT" TIMESTAMP NOT NULL, 
	"MSL_PRIO" INTEGER(5), 
	"MSL_MESS_RO" INTEGER(5), 
	"MSL_TYPE_RO" INTEGER(5), 
	"MSL_QTIM" TIMESTAMP, 
	"MSL_VALU" INTEGER(5), 
	"MSL_VALT" VARCHAR(20), 
	"MSL_ORTC" VARCHAR(80), 
	"MSL_TKEY" VARCHAR(60), 
	"MSL_USER" VARCHAR(30), 
	"MSL_QUSR" VARCHAR(30), 
	"MSL_COMT" LONG VARCHAR(1024),
	PRIMARY KEY(msl_time, msl_lfdn))
/
$dbh->commit() 
    or die($DBI::errstr);
}
__END__
('1996-07-16 22:08:01', 1, '1996-07-16 22:08:01', 0, 678, 12, '1970-01-01 00:00:00', 0, undef,'Ereignis quittiert', undef,'adz', 'RTT_ACTION_LOG', 'G665 NOTAUSGANG 18 1.UG ENTRIEGELT')
