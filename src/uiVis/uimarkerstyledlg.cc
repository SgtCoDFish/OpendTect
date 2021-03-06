/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        N. Hemstra
 Date:          May 2002
________________________________________________________________________

-*/

#include "uimarkerstyledlg.h"

#include "uimarkerstyle.h"
#include "draw.h"


uiMarkerStyleDlg::uiMarkerStyleDlg( uiParent* p, const uiString& title )
	: uiDialog(p,
		   uiDialog::Setup(title,tr("Specify marker style properties"),
				   mNoHelpKey)
		   .canceltext(uiString::emptyString()))
{
    OD::MarkerStyle3D::Type excludedtypes[] =
				{ OD::MarkerStyle3D::None, OD::MarkerStyle3D::Point };
    stylefld_ = new uiMarkerStyle3D( this, true, Interval<int>( 1, 15 ),
	    2, excludedtypes );
    stylefld_->typeSel()->notify( mCB(this,uiMarkerStyleDlg,typeSel) );
    stylefld_->sliderMove()->notify( mCB(this,uiMarkerStyleDlg,sliderMove));
    stylefld_->colSel()->notify( mCB(this,uiMarkerStyleDlg,colSel) );

    preFinalise().notify( mCB(this,uiMarkerStyleDlg,doFinalise) );
}


bool uiMarkerStyleDlg::acceptOK( CallBacker* )
{
    OD::MarkerStyle3D style;
    stylefld_->getMarkerStyle( style ); //just to process text input on fld
    return true;
}
