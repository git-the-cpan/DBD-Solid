
require DBI;
require insert;

$| = 1;

my $dbh;
$dbh = DBI->connect('', 'test', 'test', 'Solid')
       or die($DBI::errstr) unless($dbh);

print "*** PSF: running CREATE\n";
psf_create($dbh);

$dbh->{AutoCommit} = 0;			# AutoCommit isn't fast

print "*** PSF: running INSERT\n";
my $table = 'PERSONFIX';
my $data = "gzip -d <personfix.pl.gz |";
my $colref = columns($dbh, $table);

my $t1 = time();
my $cnt = insert($dbh, $table, $colref, $data);
my $t2 = time();
my $dt = $t2 - $t1;

$dbh->commit();


print "*** PSF: check overhead\n";
my $t1 = time();
my $cnt = insert($dbh, $table, $colref, $data, 1);
my $t2 = time();
$dt -= $t2 - $t1;
print sprintf("Insert AutoCommit=%d ($cnt rows) %d seconds, %02d:%02d\n",
	$dbh->{AutoCommit},
	$dt, $dt/60, $dt%60);

$dbh->disconnect() 
    or die($DBI->errstr);



sub psf_create
{
my $dbh=shift;
$dbh->do('drop table personfix');

$dbh->do(<<"/") or die ($DBI::errstr);
CREATE TABLE "PERSONFIX" (
	"PSF_PSID" NUMERIC(5, 0) NOT NULL, 
	"PSF_GUAB" DATE NOT NULL, 
	"PSF_GUBI" DATE NOT NULL, 
	"PSF_ADAT" TIMESTAMP NOT NULL, 
	"PSF_AWGV" DATE NOT NULL, 
	"PSF_BET_BEID" NUMERIC(10, 0) NOT NULL, 
	"PSF_ABT_ABID" NUMERIC(10, 0) NOT NULL, 
	"PSF_CPY_COID" NUMERIC(5, 0) NOT NULL, 
	"PSF_AWGB" DATE, 
	"PSF_SPL_SPID" NUMERIC(10, 0), 
	"PSF_URST" DATE, 
	"PSF_PINC" NUMERIC(5, 0), 
	"PSF_GRP_GRID" NUMERIC(5, 0), 
	"PSF_KST_KOID" NUMERIC(10, 0), 
	"PSF_EINT" DATE, 
	"PSF_AUST" DATE, 
	"PSF_PSNR" VARCHAR(12), 
	"PSF_ANRD" VARCHAR(6), 
	"PSF_NAME" VARCHAR(24), 
	"PSF_RELI" VARCHAR(1), 
	"PSF_DVER" VARCHAR(1), 
	"PSF_USER" VARCHAR(30), 
	"PSF_AWVF" VARCHAR(8), 
	"PSF_ZEM_ZMID" VARCHAR(4), 
	"PSF_ZUR_ZRID" VARCHAR(4), 
	"PSF_BUR_BRID" VARCHAR(4), 
	"PSF_BDE_BDID" VARCHAR(4), 
	"PSF_SORT" VARCHAR(24), 
	"PSF_BEMK" VARCHAR(80), 
	"PSF_ATXT" VARCHAR(20), 
	"PSF_ANSP" VARCHAR(4), 
	"PSF_ADM_CODE" VARCHAR(8),
	PRIMARY KEY(psf_psid, psf_guab)
	)
/
$dbh->commit() 
    or die($DBI::errstr);
}
__END__

