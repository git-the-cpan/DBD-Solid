REM
REM build DBD::Solid using MS VC
REM
REM CALL d:\devstudio\vc\bin\vcvars32

REM ---- where solid files live
set SOLIDDIR=d:\solid

REM ---- solid database user and password, used for tests
REM set SOLID_USER=test/test
set DBI_DSN=DBI:Solid:
set DBI_USER=test
set DBI_PASS=test

SETLOCAL
REM ---- make - should be the same used to build perl
set MY_MAKE=nmake

REM --- here we go!

perl Makefile.PL
IF errorlevel 1 goto err_perl

@echo off
%MY_MAKE%
IF errorlevel 1 goto err_make

%MY_MAKE% test
IF errorlevel 1 goto err_test

%MY_MAKE% install
IF errorlevel 1 goto err_install

echo ***********************************************************
echo It looks like DBD::Solid has been built and installed O.K.
echo Have fun!
echo ***********************************************************
goto end

:err_perl
    echo *** error creating DBD-Solid's Makefile (perl Makefile.PL)
    goto end

:err_build
    echo *** error building DBD-Solid (nmake)
    goto end

:err_test
    echo *** error testing DBD-Solid
    goto end

:err_install
    echo *** error installing DBD-Solid
    goto end

:end
    end_local
