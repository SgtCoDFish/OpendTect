/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : K. Tingdahl
 * DATE     : Jan 2015
 * FUNCTION :
-*/


#include "systeminfo.h"

#include "testprog.h"


bool testSystemInfo()
{
    //Dummy test in a sense, as we cannot check the result
    mRunStandardTest( System::uniqueSystemID(),
		     "uniqueSystemID" );

    return true;
}

int main( int argc, char** argv )
{
    mInitTestProg();

    if ( !testSystemInfo() )
	ExitProgram( 1 );

    return ExitProgram( 0 );
}
