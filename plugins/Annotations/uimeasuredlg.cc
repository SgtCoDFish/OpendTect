/*+
________________________________________________________________________

    (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
    Author:        Nageswara
    Date:          May 2008
________________________________________________________________________

-*/
static const char* rcsID = "$Id: uimeasuredlg.cc,v 1.18 2010-01-25 11:34:20 cvsnanne Exp $";

#include "uimeasuredlg.h"

#include "bufstring.h"
#include "draw.h"
#include "position.h"
#include "settings.h"
#include "survinfo.h"

#include "uibutton.h"
#include "uicolor.h"
#include "uigeninput.h"
#include "uilabel.h"
#include "uisellinest.h"
#include "uispinbox.h"


static const char* sKeyLineStyle = "Measure LineStyle";


uiMeasureDlg::uiMeasureDlg( uiParent* p )
    : uiDialog( p, Setup("Measured Distance",mNoDlgTitle,"50.0.15")
	    		.modal(false) )
    , ls_(*new LineStyle(LineStyle::Solid,3))
    , appvelfld_(0)
    , zdist2fld_(0)
    , velocity_(2000)
    , lineStyleChange(this)
    , clearPressed(this)
    , velocityChange(this)
{
    setOkText( "" );
    setCancelText( "" );

    BufferString str;
    mSettUse(get,"dTect",sKeyLineStyle,str);
    if ( !str.isEmpty() )
	ls_.fromString( str.buf() );

    uiGroup* topgrp = new uiGroup( this, "Info fields" );
    BufferString hdistlbl ( "Horizontal Distance ", SI().getXYUnitString() );
    hdistfld_ = new uiGenInput( topgrp, hdistlbl, FloatInpSpec(0) );
    hdistfld_->setReadOnly( true );

    BufferString zdistlbl( "Vertical Distance ", SI().getZUnitString() ); 
    zdistfld_ = new uiGenInput( topgrp, zdistlbl, FloatInpSpec(0) );
    zdistfld_->setReadOnly( true );
    zdistfld_->attach( alignedBelow, hdistfld_ );

    if ( SI().zIsTime() )
    {
	BufferString lbl( "Vertical Distance ", SI().getXYUnitString() );
	zdist2fld_ = new uiGenInput( topgrp, lbl, FloatInpSpec(0) );
	zdist2fld_->attach( alignedBelow, zdistfld_ );

	lbl = "Velocity ";
	lbl += BufferString( "(", SI().getXYUnitString(false), "/sec)" );
	appvelfld_ = new uiGenInput( topgrp, lbl, FloatInpSpec(velocity_) );
	appvelfld_->valuechanged.notify( mCB(this,uiMeasureDlg,velocityChgd) );
	appvelfld_->attach( alignedBelow, zdist2fld_ );
    }

    BufferString distlbl( "Distance ", SI().getXYUnitString() );
    distfld_ = new uiGenInput( topgrp, distlbl, FloatInpSpec(0) );
    distfld_->setReadOnly( true );
    distfld_->attach( alignedBelow, appvelfld_ ? appvelfld_ : zdistfld_ );

    inlcrldistfld_ = new uiGenInput( topgrp, "Inl/Crl Distance",
	    			     IntInpIntervalSpec(Interval<int>(0,0))
				     .setName("InlDist",0)
				     .setName("CrlDist",1) );
    inlcrldistfld_->setReadOnly( true, -1 );
    inlcrldistfld_->attach( alignedBelow, distfld_ );

    uiGroup* botgrp = new uiGroup( this, "Button group" );
    uiPushButton* clearbut = new uiPushButton( botgrp, "&Clear",
				mCB(this,uiMeasureDlg,clearCB), true );
    uiPushButton* stylebut = new uiPushButton( botgrp, "&Line style",
				mCB(this,uiMeasureDlg,stylebutCB), false );
    stylebut->attach( rightTo, clearbut );

    botgrp->attach( centeredBelow, topgrp );
}


uiMeasureDlg::~uiMeasureDlg()
{
    delete &ls_;
}


void uiMeasureDlg::lsChangeCB( CallBacker* cb )
{
    mDynamicCastGet(uiColorInput*,uicol,cb)
    mDynamicCastGet(uiSpinBox*,uisb,cb)
    if ( !uicol && !uisb ) return;

    if ( uicol )
	ls_.color_ = uicol->color();
    else if ( uisb )
	ls_.width_ = uisb->getValue();
    lineStyleChange.trigger( cb );
}


void uiMeasureDlg::clearCB( CallBacker* cb )
{ clearPressed.trigger( cb ); }


void uiMeasureDlg::stylebutCB( CallBacker* )
{
    uiDialog dlg( this, uiDialog::Setup("Line Style",mNoDlgTitle,mNoHelpID) );
    dlg.setCtrlStyle( uiDialog::LeaveOnly );
    uiSelLineStyle* linestylefld =
	new uiSelLineStyle( &dlg, ls_, "", false, true, true );
    linestylefld->changed.notify( mCB(this,uiMeasureDlg,lsChangeCB) );
    dlg.go();

    BufferString str;
    ls_.toString( str );
    mSettUse(set,"dTect",sKeyLineStyle,str);
    Settings::common().write();
}


void uiMeasureDlg::velocityChgd( CallBacker* )
{
    if ( !appvelfld_ ) return;

    const float newvel = appvelfld_->getfValue();
    if ( mIsEqual(velocity_,newvel,mDefEps) )
	return;

    velocity_ = newvel;
    velocityChange.trigger();
}


void uiMeasureDlg::reset()
{
    hdistfld_->setValue( 0 );
    zdistfld_->setValue( 0 );
    if ( zdist2fld_ )
	zdist2fld_->setValue( 0 );
    if ( appvelfld_ )
    	appvelfld_->setValue( velocity_ );
    distfld_->setValue( 0 );
    inlcrldistfld_->setValue( Interval<int>(0,0) );
}


static const double cM2Ft2 = 10.76391;
static const double cFt2M2 = 0.09290304;

void uiMeasureDlg::fill( const TypeSet<Coord3>& points )
{
    const float velocity = appvelfld_ ? appvelfld_->getfValue() : 0 ;
    const int size = points.size();
    if ( size<2 )
    {
	reset();
	return;
    }

    int totinldist = 0, totcrldist = 0;
    double tothdist = 0, totzdist = 0, totrealdist = 0;
    for ( int idx=1; idx<size; idx++ )
    {
	const Coord xy = points[idx].coord();
	const Coord prevxy = points[idx-1].coord();
	const BinID bid = SI().transform( xy );
	const BinID prevbid = SI().transform( prevxy );
	const float zdist = fabs( points[idx-1].z - points[idx].z );

	totinldist += abs( bid.r() - prevbid.r() );
	totcrldist += abs( bid.c() - prevbid.c() );
	const double hdist = xy.distTo( prevxy );
	tothdist += hdist;
	totzdist += zdist;
    
	if ( SI().zIsTime() )
	    totrealdist += 
		Math::Sqrt( hdist*hdist + velocity*velocity*zdist*zdist/4 );
	else if ( SI().zInMeter() )
	{
	   if ( SI().xyInFeet() )
		totrealdist += Math::Sqrt( hdist*hdist + cM2Ft2*zdist*zdist );
	   else
		totrealdist += Math::Sqrt( hdist*hdist + zdist*zdist );
	}
	else
	{
	    if ( SI().xyInFeet() )
		totrealdist += Math::Sqrt( hdist*hdist + zdist*zdist );
	    else
		totrealdist += Math::Sqrt( cFt2M2*hdist*hdist + zdist*zdist );
	}
    }

    hdistfld_->setValue( tothdist );
    zdistfld_->setValue( totzdist*SI().zFactor() );
    if ( zdist2fld_ ) zdist2fld_->setValue( totzdist*velocity/2 );
    distfld_->setValue( totrealdist );
    inlcrldistfld_->setValue( Interval<int>(totinldist,totcrldist) );
}
