/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        N. Hemstra
 Date:          June 2002
 RCS:           $Id: uisetdatadir.cc,v 1.1 2004-01-12 15:02:44 bert Exp $
________________________________________________________________________

-*/

#include "uisetdatadir.h"
#include "uifileinput.h"
#include "uimsg.h"
#include "ioman.h"
#include "settings.h"
#include "filegen.h"


extern "C" { const char* GetBaseDataDir(); }


uiSetDataDir::uiSetDataDir( uiParent* p )
	: uiDialog(p,uiDialog::Setup("Set Data Directory",
		    		     "Specify a data storage directory",
		    		     "8.0.1"))
	, oddirfld(0)
	, olddatadir(GetBaseDataDir())
{
    const bool oldok = isOK( olddatadir );
    BufferString oddirnm, basedirnm;
    const char* titltxt = 0;

    if ( olddatadir != "" )
    {
	if ( oldok )
	{
	    titltxt = 
		  "Locate a data storage directory\n"
		  "or specify a new directory name to create";
	    basedirnm = olddatadir;
	}
	else
	{
	    titltxt = 
		  "OpendTect needs a place to store its data.\n"
		  "The current OpendTect data storage directory is invalid.\n"
		  "* Locate a valid data storage directory\n"
		  "* Or specify a new directory name to create";

	    oddirnm = File_getFileName( olddatadir );
	    basedirnm = File_getPathOnly( olddatadir );
	}
    }
    else
    {
	titltxt =
	"OpendTect needs a place to store its data.\n"
	"You have not yet specified a location for this datastore,\n"
	"and there is no 'DTECT_DATA or dGB_DATA' set in your environment.\n\n"
	"Please specify where the OpendTect Data Directory should\n"
	"be created or select an existing OpendTect data directory."
#ifndef __win__
	"\n\nNote that you can still put surveys and "
	"individual cubes on other disks;\nbut this is where the "
	"'base' data store will be."
#endif
	;
	oddirnm = "ODData";
	basedirnm = GetPersonalDir();
    }
    setTitleText( titltxt );

    const char* basetxt = oldok ? "OpendTect Data Storage Directory"
				: "Location";
    basedirfld = new uiFileInput( this, basetxt,
				  uiFileInput::Setup(basedirnm).directories() );

    if ( !oldok )
    {
	oddirfld = new uiGenInput( this, "Directory name", oddirnm );
	oddirfld->attach( alignedBelow, basedirfld );
	olddatadir = "";
    }
}


bool uiSetDataDir::isOK( const char* d )
{
    const BufferString datadir( d ? d : GetBaseDataDir() );
    const BufferString omffnm( File_getFullPath( datadir, ".omf" ) );
    const BufferString seisdir( File_getFullPath( datadir, "Seismics" ) );
    return File_isDirectory( datadir )
	&& File_exists( omffnm )
	&& !File_exists( seisdir );
}


#define mErrRet(msg) { uiMSG().error( msg ); return false; }

bool uiSetDataDir::acceptOK( CallBacker* )
{
    BufferString datadir = basedirfld->text();
    if ( datadir == "" || !File_isDirectory(datadir) )
	mErrRet( "Please enter a valid (existing) location" )

    if ( oddirfld )
    {
	BufferString oddirnm = oddirfld->text();
	if ( oddirnm == "" )
	    mErrRet( "Please enter a (sub-)directory name" )

	datadir = File_getFullPath( datadir, oddirnm );
    }
    else if ( datadir == olddatadir )
	return true;

    const BufferString omffnm = File_getFullPath( datadir, ".omf" );
    const BufferString stdomf( GetDataFileName("omf") );

    if ( !File_exists(datadir) )
    {
#ifdef __win__
	if ( !strncasecmp("C:\\Program Files", dirnm, 16)
	  || strstr( dirnm, "Program Files" )
	  || strstr( dirnm, "program files" )
	  || strstr( dirnm, "PROGRAM FILES" ) )
	    mErrRet( "Please do not try to use 'Program Files' for data.\n"
		     "A directory like 'My Documents' would be good." )
#endif
	if ( !File_createDir( datadir, 0 ) )
	    mErrRet( "Cannot create the new directory.\n"
		     "Please check if you have the required write permissions" )
	File_copy( stdomf, omffnm, NO );
	if ( !File_exists(omffnm) )
	    mErrRet( "Cannot copy a file to the new directory!" )
    }
    else if ( !isOK(datadir) )
    {
	if ( !File_isDirectory(datadir) )
	    mErrRet( "A file (not a directory) with this name already exists" )
	if ( !File_exists(omffnm) )
	{
	    if ( !uiMSG().askGoOn( "This is not an OpendTect data directory.\n"
				  "Do you want it to be converted into one?" ) )
		return false;
	    File_copy( stdomf, omffnm, NO );
	    if ( !File_exists(omffnm) )
		mErrRet( "Could not convert the directory.\n"
			 "Most probably you have no write permissions." )
	}
	else
	{
	    const BufferString seisdir( File_getFullPath(datadir,"Seismics") );
	    if ( File_exists(seisdir) )
	    {
		BufferString probdatadir( File_getPathOnly(datadir) );
		BufferString msg( "This seems to be a project directory.\n" );
		msg += "We need the directory containing the project dirs.\n"
			"Do you want to correct your input to\n"; 
		msg += probdatadir;
		msg += " ...?";
		int res = uiMSG().askGoOnAfter( msg );
		if ( res == 2 )
		    return false;
		else if ( res == 0 )
		    datadir = probdatadir;
	    }
	}
    }

    // OK - we're (almost) certain that the directory exists and is valid
    const bool haveenv = getenv("DTECT_DATA");
    if ( haveenv )
    {
	BufferString envsett("DTECT_DATA=");
	envsett += datadir;
	putenv( envsett.buf() );
    }

    Settings::common().set( "Default DATA directory", datadir );
    if ( !Settings::common().write() )
    {
	if ( !haveenv )
	    mErrRet( "Cannot write your user settings.\n"
		     "This means your selection cannot be used!" );
	uiMSG().warning( "Cannot write your user settings.\n"
			 "Preferences cannot be stored!" );
    }

    if ( haveenv )
	uiMSG().warning( "The selected directory will overrule your DTECT_DATA"
	       		 "\nfor this run of OpendTect only." );

    IOMan::newSurvey();
    return true;
}
