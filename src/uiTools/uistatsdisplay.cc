/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Bert
 Date:          Mar 2008
________________________________________________________________________

-*/
static const char* rcsID = "$Id: uistatsdisplay.cc,v 1.25 2009-06-09 08:18:19 cvsbert Exp $";

#include "uistatsdisplay.h"
#include "uistatsdisplaywin.h"

#include "uicombobox.h"
#include "uihistogramdisplay.h"
#include "uigeninput.h"
#include "uilabel.h"
#include "uiseparator.h"

#include "arraynd.h"
#include "draw.h"
#include "bufstring.h"
#include "datapackbase.h"
#include "statruncalc.h"

#define mPutCountInPlot() (histgramdisp_ && setup_.countinplot_)


uiStatsDisplay::uiStatsDisplay( uiParent* p, const uiStatsDisplay::Setup& su )
    : uiGroup( p, "Statistics display group" )
    , setup_(su)
    , histgramdisp_(0)
    , minmaxfld_(0)
    , countfld_(0)
    , namefld_(0)
{
    const bool putcountinplot = mPutCountInPlot();
    if ( setup_.withplot_ )
    {
	uiHistogramDisplay::Setup fsu;
	fsu.annoty( setup_.vertaxis_ );
	fsu.annoty( setup_.vertaxis_ );
	histgramdisp_ = new uiHistogramDisplay( this, fsu );
    }

    uiSeparator* sep = 0;
    if ( setup_.withplot_ && setup_.withtext_ )
    {
	sep = new uiSeparator( this, "Hor sep" );
	sep->attach( stretchedBelow, histgramdisp_ );
    }

    if ( setup_.withtext_ )
    {
	if ( setup_.withname_ )
	    namefld_ = new uiLabel( this, "Data Name" );

	uiGroup* valgrp = new uiGroup( this, "Values group" );
	if ( setup_.withname_ )
	    valgrp->attach( alignedBelow, namefld_ );	
	minmaxfld_ = new uiGenInput( valgrp, "Value range",
				     FloatInpSpec(), FloatInpSpec() );
	minmaxfld_->setReadOnly();
	avgstdfld_ = new uiGenInput( valgrp, "Mean/Std Deviation",
				     DoubleInpSpec(), DoubleInpSpec() );
	avgstdfld_->attach( alignedBelow, minmaxfld_ );	
	avgstdfld_->setReadOnly();
	medrmsfld_ = new uiGenInput( valgrp, "Median/RMS",
				     FloatInpSpec(), DoubleInpSpec() );
	medrmsfld_->attach( alignedBelow, avgstdfld_ );	
	medrmsfld_->setReadOnly();
	if ( !putcountinplot )
	{
	    countfld_ = new uiGenInput( valgrp, "Number of values" );
	    countfld_->setReadOnly();
	    countfld_->attach( alignedBelow, medrmsfld_ );	
	}

	if ( sep )
	{
	    valgrp->attach( centeredBelow, histgramdisp_ );
	    valgrp->attach( ensureBelow, sep );
	}
    }

    if ( putcountinplot )
	putN();
}


void uiStatsDisplay::setDataName( const char* nm )
{
    if ( !setup_.withname_ )
	return;
    namefld_->setText( nm );
    namefld_->setAlignment( Alignment::HCenter );
}


bool uiStatsDisplay::setDataPackID( DataPack::ID dpid, DataPackMgr::ID dmid )
{
    if ( !histgramdisp_ || 
	 (histgramdisp_ && !histgramdisp_->setDataPackID(dpid,dmid)) )
    {
	Stats::RunCalc<float> rc( (Stats::RunCalcSetup()
					    .require(Stats::Min)
					    .require(Stats::Max)
					    .require(Stats::Average)
					    .require(Stats::Median)
					    .require(Stats::StdDev)
					    .require(Stats::RMS)) );
	DataPackMgr& dpman = DPM( dmid );
	const DataPack* datapack = dpman.obtain( dpid );
	if ( !datapack ) return false;

	if ( dmid == DataPackMgr::CubeID() )
	{
	    mDynamicCastGet(const ::CubeDataPack*,cdp,datapack);
	    const Array3D<float>* arr3d = cdp ? &cdp->data() : 0;
	    if ( !arr3d ) return false;

	    const float* array = arr3d->getData();
	    for ( int idx=0; idx<arr3d->info().getTotalSz(); idx++ )
	    {
		const float val = array[idx];
		if ( mIsUdf(val) ) continue ;
		rc.addValue( array[idx] );
	    }
	}
	else if ( dmid == DataPackMgr::FlatID() )
	{
	    mDynamicCastGet(const FlatDataPack*,fdp,datapack);
	    if ( !fdp ) return false;

	    const Array2D<float>* array = &fdp->data();
	    if ( array->getData() )
		 rc.addValues( array->info().getTotalSz(), array->getData() );
	}
	    
	setData( rc );
	return false;
    }
    
    setData( histgramdisp_->getRunCalc() );

    return true;
}


void uiStatsDisplay::setData( const float* array, int sz )
{
    if ( !histgramdisp_ )
	return;

    histgramdisp_->setData( array, sz );
    setData( histgramdisp_->getRunCalc() );
}


void uiStatsDisplay::setData( const Array2D<float>* array )
{
    if ( !histgramdisp_ )
	return;

    histgramdisp_->setData( array );
    setData( histgramdisp_->getRunCalc() );
}


void uiStatsDisplay::setData( const Stats::RunCalc<float>& rc )
{
    if ( mPutCountInPlot() )
	putN();
    if ( !minmaxfld_ ) return;

    if ( countfld_ )
	countfld_->setValue( rc.count() );
    minmaxfld_->setValue( rc.min(), 0 );
    minmaxfld_->setValue( rc.max(), 1 );
    avgstdfld_->setValue( rc.average(), 0 );
    avgstdfld_->setValue( rc.stdDev(), 1 );
    medrmsfld_->setValue( rc.median(), 0 );
    medrmsfld_->setValue( rc.rms(), 1 ); 
}


void uiStatsDisplay::setMarkValue( float val, bool forx )
{
    if ( histgramdisp_ )
	histgramdisp_->setMarkValue( val, forx );
}


void uiStatsDisplay::putN()
{
    histgramdisp_->putN();
}


uiStatsDisplayWin::uiStatsDisplayWin( uiParent* p,
					const uiStatsDisplay::Setup& su,
       					int nr, bool ismodal )
    : uiMainWin(p,"Data statistics",-1,false,ismodal)
    , statnmcb_(0)
{
    uiLabeledComboBox* lblcb=0;
    if ( nr > 1 )
    {
	lblcb = new uiLabeledComboBox( this, "Select Data" );
	statnmcb_ = lblcb->box(); 
    }

    for ( int idx=0; idx<nr; idx++ )
    {
	uiStatsDisplay* disp = new uiStatsDisplay( this, su );
	if ( statnmcb_ )
	    disp->attach( rightAlignedBelow, lblcb );
	disps_ += disp;
	disp->display( false );
    }

    showStat( 0 );
    if ( statnmcb_ )
	statnmcb_->selectionChanged.notify(
		mCB(this,uiStatsDisplayWin,dataChanged) );
}


void uiStatsDisplayWin::showStat( int idx )
{ disps_[idx]->display( true ); }


void uiStatsDisplayWin::dataChanged( CallBacker* )
{
    if ( !statnmcb_ )
	return;
    for ( int idx=0; idx<disps_.size(); idx++ )
	disps_[idx]->display( false );
    showStat( statnmcb_->currentItem() );
}


void uiStatsDisplayWin::setData( const Stats::RunCalc<float>& rc, int idx )
{
    disps_[idx]->setData( rc.vals_.arr(), rc.vals_.size() );
}


void uiStatsDisplayWin::addDataNames( const BufferStringSet& nms )
{ if ( statnmcb_ ) statnmcb_->addItems( nms ); }


void uiStatsDisplayWin::setDataName( const char* nm, int idx )
{
    if ( statnmcb_ )
    {
	idx > statnmcb_->size()-1 ? statnmcb_->addItem(nm)
	    			  : statnmcb_->setItemText(idx,nm);
	return;
    }

    disps_[idx]->setDataName( nm );
}


void uiStatsDisplayWin::setMarkValue( float val, bool forx, int idx )
{ disps_[idx]->setMarkValue( val, forx ); }
