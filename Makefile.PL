#!/usr/local/bin/perl -sw
#
# $Id: Makefile.PL,v 1.10 1997/07/21 23:13:32 tom Exp $
# Copyright (c) 1997  Thomas K. Wenrich
#
# You may distribute under the terms of either the GNU General Public
# License or the Artistic License, as specified in the Perl README file.
#
BEGIN { require 5.003 }	# 5.003 fixes very important bugs
use Config;
use ExtUtils::MakeMaker 5.16, qw(&WriteMakefile $Verbose);
use strict;
use File::Find;			# for searching Solid files

# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

my $solid_user = 'solid';	# SOLID RDBMS owner
my $solid_home;			# base directory of SOLID software
				# (determined automagically if undefined)

#-------------------------------------------------------------------------
# build path to search for SOLID files
#-------------------------------------------------------------------------
my @search_dirs;
my $d;
if ($solid_home) {
    push(@search_dirs, $solid_home);
    }
elsif ($d = $ENV{'SOLIDDIR'}) {
    push(@search_dirs, $d);
    }
elsif ($d = (getpwnam($solid_user))[7]) {
    push(@search_dirs, $d);
    }
else {
    push(@search_dirs, '/usr/local');
    }

my %opts = 
(
    'NAME'	=> 'DBD::Solid',
    'VERSION_FROM' => 'Solid.pm', # finds $VERSION
    'LIBS'	=> [''],   # e.g., '-lm' 
    'DEFINE'	=> '',     # e.g., '-DHAVE_SOMETHING' 
    'INC'	=> '',     # e.g., '-I/usr/include/other' 
    dist  => { DIST_DEFAULT=> 'clean distcheck disttest ci tardist',
                PREOP => '$(MAKE) -f Makefile.old distdir' },
    'OBJECT'    => qw( $(O_FILES) ),
);
OS_Options(\%opts);		# add o/s compiler specific options

my $static_request = 0;
my $arg;
my $i;
while ($arg = $ARGV[$i++])
    {
    if ($arg =~ /LINKTYPE=static/i)
    	{
	$static_request = 1;
	$opts{LINKTYPE} = 'static';
	}
    $opts{OPTIMIZE} = '-g -O0'
    	if ($arg eq '--nooptimize');
    }
# $opts{LINKTYPE} = 'static';

print "\nConfiguring DBD::Solid ...\n
>>>\tRemember to actually *READ* the README file!
   \tEspecially if you have any problems.\n
";

print ">>>\tSearching files of SOLID development kit\n";

#
# search static libraries
# (these MUST be somewhere)
#
my $static_link;
my ($lib, $libnam, $libdir);
my ($slib, $slibdir, $slibnam) = find_lib(1, @search_dirs);

die($::err_sollib_notfound) 
    unless defined($slib);

if (!defined($opts{'LINKTYPE'}))
    {
    ($lib, $libdir, $libnam) = find_lib(0, @search_dirs);
    $static_link = 0;
    unless (defined($lib))
        {
	$opts{'LINKTYPE'} = 'static';
	$static_link = 1;
	}
    }
else
    {
    $static_link = ($opts{'LINKTYPE'} eq 'static');
    }

if ($static_link)
    {
    ($lib, $libdir, $libnam) = ($slib, $slibdir, $slibnam);
    }

print "   \tSOLID library: ";
print $lib, "\n";

my ($inc) = find_inc(@search_dirs);
die($::err_solinc_notfound) 
	unless defined($inc);

print "   \tSOLID include: ";
print $inc, "\n";

print "\n";

$opts{INC}  = "-I\$(INSTALLSITEARCH)/auto/DBI"
	      . " -I\$(INSTALLSITEARCH)"
	###   . " -I\$(INSTALLSITEARCH)/DBI"  # OOOOLD DBI
	###   . " -I\$(INSTALLARCHLIB)"
		;
$opts{macro} = {
	SOLID_LIBRARY => $lib,
	SOLID_INCLUDE => $inc,
	COMPRESS => 'gzip',
	SUFFIX => '.gz',
	};
$opts{INC} .= " -I$inc";

if ($libnam =~ /^lib/)
    {
    print ">>>\tusing perl's default link style\n";

    $libnam =~ s/^lib//;
    $libnam =~ s/\.a|\.so$//;

    $libnam = "-L$libdir -l$libnam";
    push (@{$opts{LIBS}}, $libnam);
    }
else
    {
    if ($static_request)
    	{
	print ">>>\tlinking STATIC (as requested)\n";
        }
    else
        {
	print ">>>\tAs there is no link lib$libnam => $libnam, we\n";
	print "\tassume that either\n";
	print "\t\t* your system doesn't support dynamic linking\n";
	print "\t\t* you don't want to link dynamic\n";
	print "\t\t* you didn't look into the README.\n";
	print "\tSo we'll use STATIC linking.\n";
	}
    $opts{macro}->{EXTRALIB} = '$(SOLID_LIBRARY)';
    $opts{'LINKTYPE'} = 'static';
    }

sub MY::pasthru {
        my($inherited) = shift->ExtUtils::MM_Unix::pasthru(@_);
        $inherited=~ s/$/\\\n\tSOLID_INCLUDE="\$(INC)"/;
        $inherited=~ s/$/\\\n\tSOLID_LIBRARY="\$(SOLID_LIBRARY)"/;
        $inherited;
        }
WriteMakefile(%opts);

# ------------------------------------------------------------
# search SOLID library and include files.
# ------------------------------------------------------------
sub find_lib 
    {
    my $link = shift(@_) ? 'static' : 'dynamic';
    my @dirs = @_;
    
    my $dyn = sub 
    	{
	my $aref = shift;
	my @names;
	foreach (@{$aref})
	    {
	    push @names, 'lib' . $_;
	    }
	push (@names, @{$aref});
	\@names
	};

    #-------------------------------------------------------------------
    # data for linking.
    #
    # solcli.a is usual the old (pre-2.2) Solid library.
    # sclxxxNN.a is the library of the current Solid version.
    #
    # libsolcli.a and libsclxxxNN.a are symlinks to the original
    # libraries (MakeMaker seems to require them to be named 'lib...')
    #
    #-------------------------------------------------------------------
    my $linux_stat_names = [qw(scllux22.a
			       solcli.a)];
    my $linux_dyn_names  = [qw(libbcllux22.so
			       libscllux22.a
		                 libsolcli.a)];
    my $sco3_stat_names  = [qw(sclscx22.a
			         solcli.a)];
    my $solaris_stat_names = [qw(sclssx22.a
                                   solcli.a)];
    my $irix_stat_names = [qw(sclirx22.a
                                solcli.a)];
    my $hpux_stat_names = [qw(sclh9x22.a
			      solcli.a)];
    my $names = 
    	{
	'linux' =>{ 	'static'  => $linux_stat_names,
		     	'dynamic' => $linux_dyn_names,
		   },
	'sco' =>   { 	'static'  => $sco3_stat_names,
			# no dyn linkining on SCO
		   },
	'solaris' => { 	'static'  => $solaris_stat_names,
		   	'dynamic' => &{$dyn}($solaris_stat_names),
		   },
	'irix' =>  { 	'static'  => $irix_stat_names,
		   	'dynamic' => &{$dyn}($irix_stat_names),
		   },
	'hpux' =>  {	'static'  => $hpux_stat_names,
		   	'dynamic' => &{$dyn}($hpux_stat_names),
		   }
	};

    unless (defined($names->{$Config{osname}}))
	{
	die($::err_os_unsupported);
	}

    my $wanted = sub 
	{
	if ($_ eq $::wanted_lib && -r $_)
	    {
	    push(@::solid_lib, $File::Find::name);
	    push(@::solid_lib_nam, $_);
	    push(@::solid_lib_dir, $File::Find::dir);
	    }
    	};

    @::solid_lib = ();
    @::solid_lib_dir = ();
    @::solid_lib_nam = ();
    
    unless (defined($names->{$Config{osname}}->{$link}))
	{
	return (undef, undef, undef);
	# die (sprintf($::err_os_link, $link));
	}
    foreach (@{$names->{$Config{osname}}->{$link}})
	{
	$::wanted_lib = $_;
	find($wanted, @dirs);
	}
    ($::solid_lib[0], $::solid_lib_dir[0], $::solid_lib_nam[0]);
    }

sub find_inc 
    {
    my @dirs = @_;
    my $wanted = sub 
	{
	push(@::solid_inc, $File::Find::dir) 
            if ($_ eq "cli0core.h" && -f $_ && -r _);
	};
    find($wanted, @dirs);
    ($::solid_inc[0]);
    }

#-----------------------------------------------------------------------------
# operating system specific options
#-----------------------------------------------------------------------------
sub OS_Options
    {
    my $opts = shift;			# \%opts
    my $osname = $Config{osname};
    my $osvers = $Config{osvers};

    if ($osname eq "sco")
    	{
        $opts->{DEFINE} .= " -DSS_SCO";
	# perl5.003+SCO 3.2 needs this ???
        # $opts{macro}->{EXTRALIBS} = '$(SOLID_LIBRARY)';	
	# $opts->{LINKTYPE} = 'static';
	}
    elsif ($osname eq "solaris")
    	{
        push(@{$opts->{LIBS}}, "-lpthread");
	}
    elsif ($osname eq "irix")
    	{
        push(@{$opts->{LIBS}}, "-lpthread");
	}
    }

BEGIN {
    $::err_solinc_notfound = <<_eot;
ERROR: 	solid include files not found.

        Use the SOLIDDIR environment variable to point to the 
	base of the directory tree where the SOLID files live.

	Example:
	    Assuming the SOLID files are in /foo/bar/include
	    and /foo/bar/lib, you may configure the DBD::Solid 
	    module by entering 
		SOLIDDIR=/foo/bar perl Makefile.PL
_eot

   $::err_os_unsupported = <<_eot;
ERROR: 	Your O/S ('$Config{osname}) is currently not supported in Makefile.PL.

	Please add your library names to the find_lib function, try it and
	mail your changes to wenrich\@ping.at.
_eot

   $::err_sollib_notfound = <<_eot;
ERROR: 	solid library wasn't found.

        Use the SOLIDDIR environment variable to point to the 
	base of the directory tree where the SOLID files live.

	Example:
	    Assuming the SOLID files are in /foo/bar/include
	    and /foo/bar/lib, you may configure the DBD::Solid 
	    module by entering 
		SOLIDDIR=/foo/bar perl Makefile.PL

_eot
}

__END__


