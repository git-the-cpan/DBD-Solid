#!/usr/local/bin/perl -sw
#
# $Id: Makefile.PL,v 1.7 1999/11/11 03:57:55 joe Exp $
# Copyright (c) 1997  Thomas K. Wenrich
#
# You may distribute under the terms of either the GNU General Public
# License or the Artistic License, as specified in the Perl README file.
#
# Major revisions made by Joe Slagel
#
require 5.003;	                # 5.003 fixes very important bugs

use strict;
use Config;
use DBI 0.86;
use ExtUtils::MakeMaker 5.16, qw(&WriteMakefile $Verbose);
use File::Find;			# for searching Solid files

# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.


# The location of SOLID is either set in the $solid_home variable or
# is automagically determined
my $solid_home;			  # base directory of SOLID installation
my $solid_user = 'solid';	  # username solid is installed as


# Options for MakeMaker...
my %opts = 
   (
   'NAME'	  => 'DBD::Solid',
   'VERSION_FROM' => 'Solid.pm',  # finds $VERSION
   'LIBS'	  => [''],        # e.g., '-lm' 
   'DEFINE'	  => '',          # e.g., '-DHAVE_SOMETHING' 
   'INC'          => '-I$(INSTALLSITEARCH)/auto/DBI '
	           . '-I$(INSTALLSITEARCH)',
   dist           => { DIST_DEFAULT => 'clean distcheck disttest tardist',
                       PREOP        => '$(MAKE) -f Makefile.old distdir' },
   'OBJECT'       => qw( $(O_FILES) ),
   );

my $requestedStatic = 0;
foreach my $arg ( @ARGV )         # scan cmd line args for MakeMaker option...
    {
    $requestedStatic = 1        if ( $arg =~ /LINKTYPE=static/i );
    $opts{OPTIMIZE}  = '-g -O0' if ( $arg eq '--nooptimize' );
    }

print "\nConfiguring DBD::Solid ...\n
>>>\tRemember to actually *READ* the README file!
   \tEspecially if you have any problems.\n
";

print ">>>\tSearching files of SOLID development kit\n";

my @searchDirs = getSearchDirs();

# Search for include files...
my $incDir = findIncludes( @searchDirs ) || die( $::err_solinc_notfound );
print "   \tSOLID include: $incDir\n";

# Search for static library (MUST be somewhere)
my %libInfo = findLibs( 'static', @searchDirs );
unless ( $libInfo{PATH} ) 
   { 
   die( $::err_sollib_notfound );
   }

# Search for dynamic library
my $linkType = 'static';               # ...default to static if dyn not found
if ( !$requestedStatic )
   {
   my %dynLibInfo = findLibs( 'dynamic', @searchDirs );
   if ( $dynLibInfo{PATH} )
      {
      %libInfo = %dynLibInfo;
      $linkType = 'dynamic';
      }
   }
print "   \tSOLID library: $libInfo{PATH}\n\n";

checkVersion( \%libInfo );

# Configure opts...
$opts{INC} .= " -I$incDir";
$opts{macro} = {
	SOLID_LIBRARY => $libInfo{PATH},
	SOLID_INCLUDE => $incDir,
	COMPRESS => 'gzip',
	SUFFIX => '.gz',
	};

if ($Config{osname} eq 'MSWin32')
    {
    print ">>>\tlinking dynamic (as we must for Win32)\n";
    $opts{dynamic_lib} = { OTHERLDFLAGS => ' $(SOLID_LIBRARY)' };
    $opts{LIBS}        = ['-L$(SOLID_LIBRARY)'];
    }
elsif ( $linkType eq 'dynamic' )
    {
    print ">>>\tUsing perl's default link style (dynamic)\n\n";

    $libInfo{NAME} =~ s/^lib//;                  # remove 'lib' prefix
    $libInfo{NAME} =~ s/\.a|\.so$//;             # remove extension
    push ( @{$opts{LIBS}}, "-L$libInfo{DIR} -l$libInfo{NAME}" );
    }
else
    {
    if ( $requestedStatic )
   	{
	print ">>>\tLinking STATIC (as requested)\n\n";
        }
    else
        {
	print ">>>\tAs there is no link lib$libInfo{NAME} => $libInfo{NAME}, we\n";
	print "\tassume that either\n";
	print "\t\t* your system doesn't support dynamic linking\n";
	print "\t\t* you don't want to link dynamic\n";
	print "\t\t* you didn't look into the README.\n";
	print "\tSo we'll use STATIC linking.\n\n";
	}

    $opts{macro}->{EXTRALIB} = '$(SOLID_LIBRARY)';
    $opts{'LINKTYPE'}        = 'static';

    push( @{$opts{LIBS}}, "-lpthread" ) if ( $Config{osname} eq 'linux' );
    }

checkForTestEnv();

OS_Options( \%opts );		  # add o/s compiler specific options
WriteMakefile(%opts);
exit( 0 );


#-- SUBROUTINES --------------------------------------------------------------#

sub MY::pasthru 
   {
   my ($inherited) = shift->ExtUtils::MM_Unix::pasthru(@_);
   $inherited =~ s/$/\\\n\tSOLID_INCLUDE="\$(INC)"/;
   $inherited =~ s/$/\\\n\tSOLID_LIBRARY="\$(SOLID_LIBRARY)"/;
   $inherited;
   }

#
# Returns a list of directories to search for the Solid include and library
# files
#
sub getSearchDirs()
   {
   my $dir;

   push( @searchDirs, $solid_home ) if ( $solid_home );
   push( @searchDirs, $dir )        if ( $dir = $ENV{'SOLIDDIR'} );
   unless ( @searchDirs )
      {
      if ($Config{osname} eq "MSWin32")     # ...is windows?
         {
         push( @searchDirs, 'c:/' );
         }
      else                                  # ...must be unix like
         {
         push( @searchDirs, $dir ) if ( $dir = (getpwnam($solid_user))[7] ); 
         push( @searchDirs, '/usr/local' );
         }
      }
   return( @searchDirs );
   }

#
# Descend one or more search directories looking for the Solid Client Include
# files.  Returns the directory the include files can be found in.
#
sub findIncludes
   {
   my @dirList = @_;              # one or more search directories to descend

   my @incDirs = ();
   my $incFile = 'cli0core\.h|cli0cli\.h';
   $incFile = uc($incFile) if ( $Config{osname} eq 'MSWin32' );

   my $wanted = sub 
      {
      push @incDirs, $File::Find::dir if ( $_ =~ /$incFile/ && -r $_ );
      };

   find( $wanted, @dirList );

   return $incDirs[0];       # return first directory found to contain includes
   }

# 
# Search for SOLID library files, returning the absolute path, the directory,
# and the name of the library to link to
# 
sub findLibs 
   {
   my $linkType = shift;          # type of link: 'static', or 'dynamic'
   my @dirList  = @_;             # list of directories to search

   # OS/Architecture Matrix for Solid 3.0/2.3
   my $libMatrix = {
      'freebsd.static'  => [qw(sclfbx(30|23).a)],
      'freebsd.dynamic' => [qw(clfbx(30|23).so)],
                           #      glibc2          libc5
      'linux.static'    => [qw(scllux(30|23).a scll2x(30|23).a)],
      'linux.dynamic'   => [qw(scllux(30|23).a scll2x(30|23).a)],

      'solaris.static'  => [qw(sclssx(30|23).a)],
      'solaris.dynamic' => [qw(sclssx(30|23).a sclssx(30|23).so)],

      'irix.static'     => [qw(sclirx(30|23).a)],
      'irix.dynamic'    => [qw(sclirx(30|23).a sclirx(30|23).so)],
                           #  10.20,11.00       9.05    
      'hpux.static'     => [qw(sclh0x30.a   sclh9x23.a)],          
      'hpux.dynamic'    => [qw(sclh0x30.sl  sclh9x23.sl )],

      'Win32.dynamic'   => [qw(SCLW32(30|23).LIB)],
      };

   # Determine possible list of library names
   my $libList = $libMatrix->{"$Config{osname}.$linkType"};
   unless ( $libList )
      {
      die( $::err_os_unsupported );
      }

   # For dynamic libraries we need a 'lib' on the front
   map { $_ = "lib$_" } @$libList if ( $linkType eq 'dynamic' );

   my @libPaths = ();             # absolute paths to libraries
   my @libDirs  = ();             # directories of libraries
   my @libNames = ();             # names of libraries
   my $wanted = sub 
      {
      my $file = ( $Config{osname} eq 'MSWin32' ) ? uc($_) : $_;
      if ( $file =~ /^$::libName$/ && -r $file )
         {
         push( @libPaths, $File::Find::name );
         push( @libDirs, $File::Find::dir );
         push( @libNames, $_ );
         }
      };

   foreach $::libName ( @$libList )
      {
      find( $wanted, @dirList );
      }

   return( PATH => $libPaths[0], DIR => $libDirs[0], NAME => $libNames[0] );
   }

#
# Sets Operating system specific options for MakeMaker
#
sub OS_Options
   {
   my $opts = shift;			# ref to %opts hash
   my $osname = $Config{osname};
   my $osvers = $Config{osvers};

   if ( $osname eq "sco" )
      {
      $opts->{DEFINE} .= " -DSS_SCO";
      # perl5.003+SCO 3.2 needs this ???
      # $opts{macro}->{EXTRALIBS} = '$(SOLID_LIBRARY)';	
      # $opts->{LINKTYPE} = 'static';
      }
   elsif ( $osname eq "solaris" )
      {
      push( @{$opts->{LIBS}}, "-lpthread" );
      }
   elsif ( $osname eq "irix" )
      {
      push( @{$opts->{LIBS}}, "-lpthread" );
      }
   elsif ($osname eq "MSWin32")
      {
      $opts->{DEFINE} .= " -DSS_WINDOWS";
      }
   }

#
# Check version of Solid database
#
sub checkVersion
   {
   my $libInfo = shift;
   return if ( $libInfo->{NAME} =~ /30\./ );

die <<eot;
ERROR: incompatible Solid version 

        Ths version of DBDSolid only functions with version 3.0 or later of 
        Solid. Only DBDSolid versions prior to 0.11a will work with your
        version of Solid...but it is recommended that you upgrade Solid.

eot
   }

#
# Check for DBI_USER and DBI_PASS enviroment variables for testing
#
sub checkForTestEnv
   {
   if ( !$ENV{DBI_USER} || !$ENV{DBI_PASS} )
      {
print <<eot;

WARNING: DBI_USER and/or DBI_PASS not set.

        The DBI_USER and DBI_PASS environment variables are used in order to
        connect to a database when running 'make test'.  Because one or both
        of these aren't defined, you may not be able to succesfully test 
        DBDSolid. See the README for more information about these and other 
        enviroment variables that affect testing.

eot
      }
   }

BEGIN 
   {
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
