/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        N. Hemstra / Bert
 Date:          March 2003 / Feb 2008
________________________________________________________________________

-*/
static const char* rcsID = "$Id: uiattribcrossplot.cc,v 1.36 2009-02-26 13:00:53 cvsbert Exp $";

#include "uiattribcrossplot.h"

#include "attribdesc.h"
#include "attribdescset.h"
#include "attribengman.h"
#include "attribsel.h"
#include "attribstorprovider.h"
#include "datacoldef.h"
#include "datapointset.h"
#include "executor.h"
#include "ioman.h"
#include "ioobj.h"
#include "iopar.h"
#include "keystrs.h"
#include "posinfo.h"
#include "posprovider.h"
#include "posfilterset.h"
#include "posvecdataset.h"
#include "pickset.h"
#include "seisioobjinfo.h"
#include "seis2dline.h"

#include "mousecursor.h"
#include "uidatapointset.h"
#include "uiioobjsel.h"
#include "uilistbox.h"
#include "uicombobox.h"
#include "uimsg.h"
#include "uiposfilterset.h"
#include "uiposprovider.h"
#include "uitaskrunner.h"

using namespace Attrib;

uiAttribCrossPlot::uiAttribCrossPlot( uiParent* p, const Attrib::DescSet& d )
	: uiDialog(p,uiDialog::Setup("Attribute cross-plotting",
		     "Select attributes and locations for cross-plot"
		     ,"101.3.0").modal(false))
	, ads_(*new Attrib::DescSet(d.is2D()))
    	, lnmfld_(0)
    	, l2ddata_(0)
    	, selptps_(0)
    	, uidps_(0)
    	, pointsSelected(this)
{
    uiLabeledListBox* llb = new uiLabeledListBox( this,
	    					  "Attributes to calculate" );
    attrsfld_ = llb->box();
    attrsfld_->setMultiSelect( true );

    uiGroup* attgrp = llb;
    if ( ads_.is2D() )
    {
	uiLabeledComboBox* lcb = new uiLabeledComboBox( this, "Line name" );
	lnmfld_ = lcb->box();
	lcb->attach( alignedBelow, llb );
	attgrp = lcb;
	lnmfld_->selectionChanged.notify( mCB(this,uiAttribCrossPlot,lnmChg) );
    }

    uiPosProvider::Setup psu( ads_.is2D(), true, true );
    psu.seltxt( "Select locations by" ).choicetype( uiPosProvider::Setup::All );
    posprovfld_ = new uiPosProvider( this, psu );
    posprovfld_->setExtractionDefaults();
    posprovfld_->attach( alignedBelow, attgrp );

    uiPosFilterSet::Setup fsu( ads_.is2D() );
    fsu.seltxt( "Location filters" ).incprovs( true );
    posfiltfld_ = new uiPosFilterSetSel( this, fsu );
    posfiltfld_->attach( alignedBelow, posprovfld_ );

    setDescSet( d );
}


void uiAttribCrossPlot::setDescSet( const Attrib::DescSet& newads )
{
    const_cast<Attrib::DescSet&>(ads_) = newads;
    adsChg();
}


void uiAttribCrossPlot::adsChg()
{
    attrsfld_->empty();

    Attrib::SelInfo attrinf( &ads_, 0, ads_.is2D() );
    for ( int idx=0; idx<attrinf.attrnms.size(); idx++ )
	attrsfld_->addItem( attrinf.attrnms.get(idx), false );
    
    for ( int idx=0; idx<attrinf.ioobjids.size(); idx++ )
    {
	BufferStringSet bss;
	SeisIOObjInfo sii( MultiID( attrinf.ioobjids.get(idx) ) );
	sii.getDefKeys( bss, true );
	for ( int inm=0; inm<bss.size(); inm++ )
	{
	    const char* defkey = bss.get(inm).buf();
	    const char* ioobjnm = attrinf.ioobjnms.get(idx).buf();
	    attrsfld_->addItem(
		    SeisIOObjInfo::defKey2DispName(defkey,ioobjnm) );
	}
    }
    if ( !attrsfld_->isEmpty() )
	attrsfld_->setCurrentItem( int(0) );

    if ( !lnmfld_ ) return;

    lnmfld_->empty();
    const Attrib::Desc* desc = ads_.getFirstStored( false );
    if ( !desc ) return;
    BufferString storedid = desc->getStoredID();
    if ( storedid.isEmpty() ) return;
    SeisIOObjInfo sii( MultiID(storedid.buf()) );
    if ( !sii.isOK() || !sii.is2D() ) return;

    BufferStringSet bss; sii.getLineNames( bss );
    for ( int idx=0; idx<bss.size(); idx++ )
	lnmfld_->addItem( bss.get(idx) );

    lnmChg( 0 );
}


uiAttribCrossPlot::~uiAttribCrossPlot()
{
    delete const_cast<Attrib::DescSet*>(&ads_);
    delete l2ddata_;
    if ( selptps_ )
	delete selptps_;
}


#define mErrRet(s) { uiMSG().error(s); return; }


void uiAttribCrossPlot::lnmChg( CallBacker* )
{
    delete l2ddata_; l2ddata_ = 0;
    if ( !lnmfld_ ) return;

    const Attrib::Desc* desc = ads_.getFirstStored( false );
    if ( !desc )
	mErrRet("No line set information in attribute set")
    BufferString storedid = desc->getStoredID();
    LineKey lk( storedid.buf() );
    const MultiID mid( lk.lineName() );
    if ( mid.isEmpty() )
	mErrRet("No line set found in attribute set")
    PtrMan<IOObj> ioobj = IOM().get( mid );
    if ( !ioobj )
	mErrRet("Cannot find line set in object management")
    Seis2DLineSet ls( ioobj->fullUserExpr(true) );
    if ( ls.nrLines() < 1 )
	mErrRet("Line set is empty")
    lk.setLineName( lnmfld_->text() );
    const int idxof = ls.indexOf( lk );
    if ( idxof < 0 )
	mErrRet("Cannot find selected line in line set")
    l2ddata_ = new PosInfo::Line2DData;
    if ( !ls.getGeometry( idxof, *l2ddata_ ) )
    {
	delete l2ddata_; l2ddata_ = 0;
	mErrRet("Cannot get geometry of selected line")
    }

    //TODO: use l2ddata_ for something
}


#define mDPM DPM(DataPackMgr::PointID())

#undef mErrRet
#define mErrRet(s) \
{ \
    if ( dps ) mDPM.release(dps->id()); \
    if ( s ) uiMSG().error(s); return false; \
}

bool uiAttribCrossPlot::acceptOK( CallBacker* )
{
    DataPointSet* dps = 0;
    PtrMan<Pos::Provider> prov = posprovfld_->createProvider();
    if ( !prov )
	mErrRet("Internal: no Pos::Provider")

    mDynamicCastGet(Pos::Provider2D*,p2d,prov.ptr())
    if ( lnmfld_ )
    {
	if ( !l2ddata_ )
	    mErrRet("Cannot work without line set position information")
	p2d->setLineData( new PosInfo::Line2DData(*l2ddata_) );
    }

    uiTaskRunner tr( this );
    if ( !prov->initialize( &tr ) )
	return false;

    ObjectSet<DataColDef> dcds;
    for ( int idx=0; idx<attrsfld_->size(); idx++ )
    {
	if ( attrsfld_->isSelected(idx) )
	    dcds += new DataColDef( attrsfld_->textOfItem(idx) );
    }
    
    if ( dcds.isEmpty() )
	mErrRet("Please select at least one attribute to evaluate")

    MouseCursorManager::setOverride( MouseCursor::Wait );
    IOPar iop; posfiltfld_->fillPar( iop );
    PtrMan<Pos::Filter> filt = Pos::Filter::make( iop, prov->is2D() );
    mDynamicCastGet(Pos::Filter2D*,f2d,filt.ptr())
    if ( f2d )
	f2d->setLineData( new PosInfo::Line2DData(*l2ddata_) );
    MouseCursorManager::restoreOverride();
    if ( filt && !filt->initialize(&tr) )
	return false;

    MouseCursorManager::setOverride( MouseCursor::Wait );
    dps = new DataPointSet( *prov, dcds, filt );
    MouseCursorManager::restoreOverride();
    if ( dps->isEmpty() )
	mErrRet("No positions selected")

    BufferString dpsnm; prov->getSummary( dpsnm );
    if ( filt )
    {
	BufferString filtsumm; filt->getSummary( filtsumm );
	dpsnm += " / "; dpsnm += filtsumm;
    }
    dps->setName( dpsnm );
    IOPar descsetpars;
    ads_.fillPar( descsetpars );
    const_cast<PosVecDataSet*>( &(dps->dataSet()) )->pars() = descsetpars;
    mDPM.add( dps );

    BufferString errmsg; Attrib::EngineMan aem;
    if ( lnmfld_ )
	aem.setLineKey( lnmfld_->text() );
    MouseCursorManager::setOverride( MouseCursor::Wait );
    PtrMan<Executor> tabextr = aem.getTableExtractor( *dps, ads_, errmsg );
    MouseCursorManager::restoreOverride();
    if ( !errmsg.isEmpty() ) mErrRet(errmsg)
	    
    if ( !tr.execute(*tabextr) )
	return false;

    uidps_ = new uiDataPointSet( this, *dps,
				 uiDataPointSet::Setup("Attribute data") );
    uidps_->showSelectedPts.notify(
	                mCB(this,uiAttribCrossPlot,createPickSet) );
    return uidps_->go() ? true : false;
}


void uiAttribCrossPlot::createPickSet( CallBacker* )
{
    if ( selptps_ )
	delete selptps_;
    selptps_ = 0;
    selptps_ = new Pick::Set( "Selected Points from Attribute" );
    selptps_->disp_.color_ = Color::White();
    for ( int idx=0; idx<uidps_->selptcoord_.size(); idx++ )
    {
	Pick::Location pickloc( uidps_->selptcoord_[idx]->x,
				uidps_->selptcoord_[idx]->y,
				uidps_->selptcoord_[idx]->z );
	*selptps_ += pickloc;
    }
    pointsSelected.trigger();
}
