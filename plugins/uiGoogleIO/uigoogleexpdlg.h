#ifndef uigoogleexpdlg_h
#define uigoogleexpdlg_h
/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : Bert
 * DATE     : Nov 2007
-*/

#include "uidialog.h"
#include "filepath.h"
class uiFileInput;

#define mDecluiGoogleExpStd \
    uiFileInput*	fnmfld_; \
    bool		acceptOK(CallBacker*)

#define mImplFileNameFld(nm) \
    BufferString deffnm( nm ); \
    deffnm.clean( BufferString::AllowDots ); \
    FilePath deffp( GetDataDir() ); deffp.add( deffnm ).setExtension( "kml" ); \
    uiFileInput::Setup fiinpsu( uiFileDialog::Gen, deffp.fullPath() ); \
    fiinpsu.forread( false ).filter( "*.kml" ); \
    fnmfld_ = new uiFileInput( this, uiStrings::sOutputFile(), fiinpsu );


#define mCreateWriter(typ,survnm) \
    const BufferString fnm( fnmfld_->fileName() ); \
    if ( fnm.isEmpty() ) \
	{ uiMSG().error(uiStrings::phrEnter(mJoinUiStrs( \
		    sOutputFile().toLower(), sName().toLower()))); \
	  return false; } \
 \
    ODGoogle::XMLWriter wrr( typ, fnm, survnm ); \
    if ( !wrr.isOK() ) \
	{ uiMSG().error(wrr.errMsg()); return false; }


#endif
