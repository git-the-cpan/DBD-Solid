#!/usr/local/bin/perl -sw
#
# $Id: Makefile.PL,v 1.4 1997/05/10 06:31:30 tom Exp $
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

#
# build path to search for SOLID files
#
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
#    'OPTIMIZE'  => '-g -O0',
);
$opts{LINKTYPE} = 'static';


print "\nConfiguring DBD::Solid ...\n
>>>\tRemember to actually *READ* the README file!
   \tEspecially if you have any problems.\n
";

my $static_link = defined($opts{'LINKTYPE'}) && $opts{'LINKTYPE'} eq 'static';

print ">>>\tLinking STATIC\n" if ($static_link);
print ">>>\tLinking DYNAMIC (for TESTING only)\n" unless($static_link);

print ">>>\tSearching files of SOLID development kit\n";
print "   \tSOLID library: ";
find_lib($static_link, @search_dirs);
die("ERROR: NOT FOUND") 
     unless defined($::solid_lib[0]);
print $::solid_lib[0], "\n";

print "   \tSOLID include: ";
find_inc(@search_dirs);
die("ERRROR: NOT FOUND") 
     unless defined($::solid_inc[0]);
print $::solid_inc[0], "\n";

print "\n";

$opts{INC}  = "-I\$(INSTALLSITEARCH)/auto/DBI"
		. " -I\$(INSTALLSITEARCH)"
		. " -I\$(INSTALLARCHLIB)";
$opts{INC} .= " -I$::solid_inc[0]";
$opts{macro} = {
	SOLID_LIBRARY => $::solid_lib[0],
	SOLID_INCLUDE => $::solid_inc[0],
	};

if ($static_link)
    {
    $opts{macro}->{EXTRALIB} = '$(SOLID_LIBRARY)';
    }
else
    {
    my $lib = $::solid_lib_nam[0];
    if ($lib =~ s/^lib//)
        {
	$lib =~ s/\.a|\.so$//;
        $opts{LIBS} = ["-L$::solid_lib_dir[0] -l$lib"];
	}
    else
    	{
	print STDERR <<"_eot";
Hey! Seems you didn't look into the README.

Please create a symlink (lib$lib.a => $::solid_lib_nam[0]) 
for dynamic linking.

_eot
	exit(1);
	}
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
    
    #------------------------------------------------------------
    # data for linking; 
    #------------------------------------------------------------
    my $linux_stat_names = [qw(scllux22.a 
			       solcli.a)];
    my $linux_dyn_names  = [qw(libbcllux22.a
		 	          bcllux22.so
			       libscllux22.a
			          scllux22.a
			         libsolcli.a
			            solcli.a)];

    my $names = 
    	{
	'linux' => { 'static' => $linux_stat_names,
		     'dynamic' => $linux_dyn_names,
		   },
	};

    unless (defined($names->{$Config{osname}}))
	{
	print <<_eot;
Your O/S is currently not supported in Makefile.PL.
Please add your library names to the find_lib function, try it and
mail your changes to wenrich\@ping.at.
_eot
        die;
	}

    my $wanted = sub 
	{
	if ($_ eq $::wanted_lib && -r $_)
	    {
	    push(@::solid_lib, $File::Find::name);
	    push(@::solid_lib_dir, $File::Find::dir);
	    push(@::solid_lib_nam, $_);
	    }
    	};

    @::solid_lib = ();
    @::solid_lib_dir = ();
    @::solid_lib_nam = ();
    
    foreach (@{$names->{$Config{osname}}->{$link}})
	{
	$::wanted_lib = $_;
	find($wanted, @dirs);
	return if (@::solid_lib);
	}
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
    }
__END__


