/*+
___________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	K. Tingdahl
 Date:		Jul 2003
___________________________________________________________________

-*/

#include "uiodemsurftreeitem.h"

#include "datapointset.h"
#include "datacoldef.h"
#include "emhorizon.h"
#include "emhorizonztransform.h"
#include "emmanager.h"
#include "ioman.h"
#include "ioobj.h"
#include "mpeengine.h"
#include "posvecdataset.h"
#include "survinfo.h"
#include "threadwork.h"

#include "uiattribpartserv.h"
#include "uiempartserv.h"
#include "uiemattribpartserv.h"
#include "uimenu.h"
#include "uimenuhandler.h"
#include "uimpepartserv.h"
#include "uimsg.h"
#include "uinotsaveddlg.h"
#include "uiodapplmgr.h"
#include "uiodscenemgr.h"
#include "uistrings.h"
#include "uiviscoltabed.h"
#include "uivisemobj.h"
#include "uivispartserv.h"
#include "visemobjdisplay.h"
#include "vishorizondisplay.h"
#include "vishorizonsection.h"


uiODDataTreeItem* uiODEarthModelSurfaceTreeItem::createAttribItem(
					const Attrib::SelSpec* as ) const
{
    const char* parenttype = typeid(*this).name();
    uiODDataTreeItem* res = as
	? uiODDataTreeItem::factory().create( 0, *as, parenttype, false) : 0;
    if ( !res ) res = new uiODEarthModelSurfaceDataTreeItem( emid_, uivisemobj_,
							     parenttype );
    return res;
}


uiODEarthModelSurfaceTreeItem::uiODEarthModelSurfaceTreeItem(
						const EM::ObjectID& nemid )
    : uiODDisplayTreeItem()
    , emid_(nemid)
    , uivisemobj_(0)
    , createflatscenemnuitem_(tr("Create Flattened Scene"))
    , savemnuitem_(uiStrings::sSave(),-800)
    , saveasmnuitem_(uiStrings::sSaveAs(),-850)
    , enabletrackingmnuitem_(tr("Enable Tracking"))
    , changesetupmnuitem_(m3Dots(tr("Change Settings")))
    , reloadmnuitem_(uiStrings::sReload(),-750)
    , trackmenuitem_(uiStrings::sTracking())
    , starttrackmnuitem_(m3Dots(tr("Start Tracking")))
    , istrackingallowed_(true)
{
    savemnuitem_.iconfnm = "save";
    saveasmnuitem_.iconfnm = "saveas";
    enabletrackingmnuitem_.checkable = true;
    changesetupmnuitem_.iconfnm = "tools";
    NotSavedPrompter::NSP().promptSaving.notify(
	    mCB(this,uiODEarthModelSurfaceTreeItem,askSaveCB));
}


uiODEarthModelSurfaceTreeItem::~uiODEarthModelSurfaceTreeItem()
{
    NotSavedPrompter::NSP().promptSaving.remove(
	    mCB(this,uiODEarthModelSurfaceTreeItem,askSaveCB));
    mDynamicCastGet(visSurvey::EMObjectDisplay*,
		    emd,visserv_->getObject(displayid_));
    if ( emd )
    {
	emd->selection()->remove(
		mCB(this,uiODEarthModelSurfaceTreeItem,selChg) );
	emd->deSelection()->remove(
		mCB(this,uiODEarthModelSurfaceTreeItem,selChg) );
	ODMainWin()->colTabEd().setColTab( 0, 0, 0 );
    }

    if ( MPE::engine().hasTracker(emid_) )
	MPE::engine().unRefTracker( emid_ );
    delete uivisemobj_;
}


#define mDelRet { delete uivisemobj_; uivisemobj_ = 0; return false; }


bool uiODEarthModelSurfaceTreeItem::init()
{
    if ( !createUiVisObj() )
	return false;

    if ( !uiODDisplayTreeItem::init() )
	return false;

    if ( MPE::engine().hasTracker(emid_) )
	MPE::engine().refTracker( emid_ );

    initNotify();
    return true;
}


bool uiODEarthModelSurfaceTreeItem::createUiVisObj()
{
    if ( uivisemobj_ )
	return true;

    if ( displayid_!=-1 )
    {
	uivisemobj_ = new uiVisEMObject( ODMainWin(), displayid_, visserv_ );
	if ( !uivisemobj_->isOK() )
	    mDelRet;

	emid_ = uivisemobj_->getObjectID();
    }
    else
    {
	uivisemobj_ = new uiVisEMObject( ODMainWin(), emid_, sceneID(),
					visserv_ );
	displayid_ = uivisemobj_->id();
	if ( !uivisemobj_->isOK() )
	    mDelRet;
    }

    mDynamicCastGet(visSurvey::EMObjectDisplay*,
		    emd,visserv_->getObject(displayid_));
    if ( emd )
    {
	emd->selection()->notify(
		mCB(this,uiODEarthModelSurfaceTreeItem,selChg) );
	emd->deSelection()->notify(
		mCB(this,uiODEarthModelSurfaceTreeItem,selChg) );
    }

    return true;
}


void uiODEarthModelSurfaceTreeItem::selChg( CallBacker* )
{
    updateTrackingState();
}


void uiODEarthModelSurfaceTreeItem::checkCB( CallBacker* cb )
{
    uiODDisplayTreeItem::checkCB(cb);
    updateTrackingState();
}


void uiODEarthModelSurfaceTreeItem::updateTrackingState()
{
    uiMPEPartServer* mps = applMgr()->mpeServer();
    const int trackerid = mps->getTrackerID( emid_ );
    if ( trackerid == -1 )
	return;

    mDynamicCastGet( visSurvey::EMObjectDisplay*,
		     emod, visserv_->getObject(displayid_) );

    const bool enabletracking = istrackingallowed_ && isChecked() &&
				emod && emod->isSelected();

    if ( mps->isTrackingEnabled(trackerid) != enabletracking )
	mps->enableTracking( trackerid, enabletracking );
}


void uiODEarthModelSurfaceTreeItem::prepareForShutdown()
{
    uiODDisplayTreeItem::prepareForShutdown();
}


void uiODEarthModelSurfaceTreeItem::createMenu( MenuHandler* menu, bool istb )
{
    uiODDisplayTreeItem::createMenu( menu, istb );
    if ( !menu || menu->menuID()!=displayID() || istb )
	return;

    mDynamicCastGet(visSurvey::Scene*,scene,
		    ODMainWin()->applMgr().visServer()->getObject(sceneID()));
    const bool hastransform = scene && scene->getZAxisTransform();

    uiMPEPartServer* mps = applMgr()->mpeServer();
    const bool hastracker = mps->getTrackerID(emid_)>=0;
    if ( !hastracker && !visserv_->isLocked(displayid_) )
    {
	mAddMenuItem( &trackmenuitem_, &starttrackmnuitem_, true, false );
	mResetMenuItem( &changesetupmnuitem_ );
	mResetMenuItem( &enabletrackingmnuitem_ );
    }
    else if ( hastracker && !visserv_->isLocked(displayid_) &&
	      !hastransform )
    {
	mAddMenuItem( &trackmenuitem_, &starttrackmnuitem_, false, false );
	mAddMenuItem( &trackmenuitem_, &changesetupmnuitem_, true, false );
	mAddMenuItem( &trackmenuitem_, &enabletrackingmnuitem_, true,
		      istrackingallowed_ );
    }
    else
    {
	mResetMenuItem( &starttrackmnuitem_ );
	mResetMenuItem( &changesetupmnuitem_ );
	mResetMenuItem( &enabletrackingmnuitem_ );
    }

    const bool isshifted =
		!mIsZero( visserv_->getTranslation(displayID()).z, 1e-5 );
    const MultiID mid = EM::EMM().getMultiID( emid_ );
    const bool enab = trackmenuitem_.nrItems() && !isshifted && isChecked()
			&& EM::canOverwrite( mid );
    mAddMenuItem( menu, &trackmenuitem_, enab, false );

#ifdef __debug__
    mAddMenuItem( menu, &reloadmnuitem_, true, false );
#else
    mResetMenuItem( &reloadmnuitem_ );
#endif

    mAddMenuItem( menu, &savemnuitem_,
		  applMgr()->EMServer()->isChanged(emid_) &&
		  applMgr()->EMServer()->isFullyLoaded(emid_) &&
		  EM::canOverwrite( mid ) &&
		  !isshifted, false );

    const bool istransformedandshifted = hastransform && isshifted;
    mAddMenuItem( menu, &saveasmnuitem_, !istransformedandshifted, false );
    mResetMenuItem( &createflatscenemnuitem_ );
}


int uiODEarthModelSurfaceTreeItem::reloadEMObject()
{
    applMgr()->visServer()->removeObject( displayid_, sceneID() );
    delete uivisemobj_; uivisemobj_ = 0;

    uiEMPartServer* ems = applMgr()->EMServer();
    const MultiID mid = ems->getStorageID(emid_);
    if ( !ems->loadSurface(mid) )
	return -1;

    emid_ = applMgr()->EMServer()->getObjectID(mid);
    uivisemobj_ = new uiVisEMObject(ODMainWin(),emid_,sceneID(),visserv_);
    displayid_ = uivisemobj_->id();
    return displayid_;
}


void uiODEarthModelSurfaceTreeItem::handleMenuCB( CallBacker* cb )
{
    uiODDisplayTreeItem::handleMenuCB(cb);
    mCBCapsuleUnpackWithCaller( int, mnuid, caller, cb );
    mDynamicCastGet(MenuHandler*,menu,caller);
    mDynamicCastGet(uiMenuHandler*,uimenu,caller);
    if ( menu->isHandled() || menu->menuID()!=displayID() || mnuid==-1 )
	return;

    EM::SectionID sectionid = -1;
    if ( uivisemobj_->nrSections()==1 )
	sectionid = uivisemobj_->getSectionID(0);
    else if ( uimenu && uimenu->getPath() )
	sectionid = uivisemobj_->getSectionID( uimenu->getPath() );

    uiMPEPartServer* mps = applMgr()->mpeServer();
    uiEMPartServer* ems = applMgr()->EMServer();
    mps->setCurrentAttribDescSet( applMgr()->attrServer()->curDescSet(false) );
    mps->setCurrentAttribDescSet( applMgr()->attrServer()->curDescSet(true) );

    if ( mnuid==savemnuitem_.id )
    {
	menu->setIsHandled( true );
	saveCB( 0 );
    }
    else if ( mnuid==saveasmnuitem_.id )
    {
	menu->setIsHandled(true);
	const MultiID oldmid = ems->getStorageID(emid_);
	mps->prepareSaveSetupAs( oldmid );

	MultiID storedmid;
	ems->storeObject( emid_, true, storedmid,
		(float) visserv_->getTranslation(displayID()).z);
	applMgr()->visServer()->setObjectName( displayid_,
	    ems->getName(emid_) );

	const MultiID midintree = ems->getStorageID(emid_);
	EM::EMM().getObject(emid_)->setMultiID( storedmid );
	mps->saveSetupAs( storedmid );
	EM::EMM().getObject(emid_)->setMultiID( midintree );

	updateColumnText( uiODSceneMgr::cNameColumn() );
    }
    else if ( mnuid==starttrackmnuitem_.id )
    {
	menu->setIsHandled(true);
	if ( sectionid < 0 ) return;

	applMgr()->enableMenusAndToolBars( false );
	applMgr()->enableTree( false );

	if ( mps->addTracker(emid_) != -1 )
	{
	    mps->useSavedSetupDlg( emid_, sectionid );
	    uivisemobj_->checkTrackingStatus();
	    applMgr()->visServer()->triggerTreeUpdate();
	    applMgr()->visServer()->turnSeedPickingOn( true );
	}

	applMgr()->enableMenusAndToolBars( true );
	applMgr()->enableTree( true );
    }
    else if ( mnuid==changesetupmnuitem_.id )
    {
	menu->setIsHandled(true);
	mps->showSetupDlg( emid_, sectionid );
    }
    else if ( mnuid==reloadmnuitem_.id )
    {
	menu->setIsHandled(true);
	reloadEMObject();
    }
    else if ( mnuid==enabletrackingmnuitem_.id )
    {
	menu->setIsHandled(true);
	istrackingallowed_ = !istrackingallowed_;
	updateTrackingState();
    }
    else if ( mnuid==createflatscenemnuitem_.id )
    {
	mDynamicCastGet(visSurvey::EMObjectDisplay*,
			emd,visserv_->getObject(displayid_));
	const EM::ObjectID objectid = emd->getObjectID();
	mDynamicCastGet(const EM::Horizon*,horizon,
			EM::EMM().getObject( objectid ) );

	if ( !horizon ) return;

    const uiString scenenm = tr("Flattened on '%1'").arg( horizon->uiName() );

	RefMan<EM::HorizonZTransform> transform = new EM::HorizonZTransform;
	transform->setHorizon( *horizon );
	ODMainWin()->sceneMgr().tile();
    ODMainWin()->sceneMgr().addScene( true, transform, scenenm );
    }
}


void uiODEarthModelSurfaceTreeItem::askSaveCB( CallBacker* )
{
    uiEMPartServer* ems = applMgr()->EMServer();
    if ( !ems->isChanged( emid_ ) )
	return;

    bool savewithname = EM::EMM().getMultiID( emid_ ).isEmpty();
    if ( !savewithname )
    {
	PtrMan<IOObj> ioobj = IOM().get( EM::EMM().getMultiID(emid_) );
	savewithname = !ioobj;
    }

    const uiString obj = toUiString("%1 \"%2\"")
	.arg( ems->getType( emid_ ) ).arg(ems->getName(emid_));
    NotSavedPrompter::NSP().addObject(	obj,
		mCB( this, uiODEarthModelSurfaceTreeItem, saveCB ),
	        savewithname, 0 );

    Threads::WorkManager::twm().addWork(
	    Threads::Work( *new uiTreeItemRemover( parent_, this ), true ), 0,
	    NotSavedPrompter::NSP().queueID(), false );
}


void uiODEarthModelSurfaceTreeItem::saveCB( CallBacker* cb )
{
    uiMPEPartServer* mps = applMgr()->mpeServer();
    uiEMPartServer* ems = applMgr()->EMServer();
    mps->setCurrentAttribDescSet( applMgr()->attrServer()->curDescSet(false) );
    mps->setCurrentAttribDescSet( applMgr()->attrServer()->curDescSet(true) );

    if ( ems->isGeometryChanged(emid_) && ems->nrAttributes(emid_)>0 )
    {
	const bool res = uiMSG().askSave(
		tr("Geometry has been changed. Saved 'Horizon Data' is\n"
		   "not valid anymore and will be removed now.\n"
		   "Continue saving?") );
	if ( !res )
	    return;
    }

    bool savewithname = EM::EMM().getMultiID( emid_ ).isEmpty();
    if ( !savewithname )
    {
	PtrMan<IOObj> ioobj = IOM().get( EM::EMM().getMultiID(emid_) );
	savewithname = !ioobj;
    }

    if ( applMgr()->EMServer()->storeObject( emid_, savewithname ) && cb )
	NotSavedPrompter::NSP().reportSuccessfullSave();

    applMgr()->visServer()->setObjectName( displayid_, ems->getName(emid_) );
    const MultiID mid = ems->getStorageID(emid_);
    mps->saveSetup( mid );
    updateColumnText( uiODSceneMgr::cNameColumn() );
}


// uiODEarthModelSurfaceDataTreeItem
uiODEarthModelSurfaceDataTreeItem::uiODEarthModelSurfaceDataTreeItem(
							EM::ObjectID objid,
							uiVisEMObject* uv,
							const char* parenttype )
    : uiODAttribTreeItem( parenttype )
    , depthattribmnuitem_(uiStrings::sZValue(mPlural))
    , savesurfacedatamnuitem_(m3Dots(tr("Save as Horizon Data")))
    , loadsurfacedatamnuitem_(m3Dots(tr("Horizon Data")))
    , algomnuitem_(uiStrings::sTools())
    , filtermnuitem_(m3Dots(tr("Filtering")))
    , fillholesmnuitem_(m3Dots(tr("Gridding")))
    , horvariogrammnuitem_(m3Dots(tr("Variogram")))
    , attr2geommnuitm_(m3Dots(tr("Set Z values")))
    , changed_(false)
    , emid_(objid)
    , uivisemobj_(uv)
{
    horvariogrammnuitem_.iconfnm = "variogram";
}


void uiODEarthModelSurfaceDataTreeItem::createMenu( MenuHandler* menu,
						    bool istb )
{
    uiODAttribTreeItem::createMenu( menu, istb );
    if ( istb ) return;

    uiVisPartServer* visserv = ODMainWin()->applMgr().visServer();
    const Attrib::SelSpec* as = visserv->getSelSpec( displayID(),
						     attribNr() );

    const bool islocked = visserv->isLocked( displayID() );
    const int nrsurfdata = applMgr()->EMServer()->nrAttributes( emid_ );
    uiString itmtxt = m3Dots(tr("Horizon Data (%1)").arg( nrsurfdata ));
    loadsurfacedatamnuitem_.text = itmtxt;
    mAddMenuItem( &selattrmnuitem_, &loadsurfacedatamnuitem_,
		  !islocked && nrsurfdata>0, false );
    if ( uivisemobj_ )
    {
	mAddMenuItem( &selattrmnuitem_, &depthattribmnuitem_, !islocked,
		  as->id().asInt()==Attrib::SelSpec::cNoAttrib().asInt() );
    }
    else
    {
	mResetMenuItem( &depthattribmnuitem_ );
    }

    const bool enabsave = changed_ ||
	(as && as->id()!=Attrib::SelSpec::cNoAttrib() &&
	 as->id()!=Attrib::SelSpec::cAttribNotSel() );

    mAddMenuItem( menu, &savesurfacedatamnuitem_, enabsave, false );
    mAddMenuItem( menu, &algomnuitem_, true, false );
    mAddMenuItem( &algomnuitem_, &filtermnuitem_, true, false );
    mAddMenuItem( &algomnuitem_, &fillholesmnuitem_, true, false );
    mAddMenuItem( &algomnuitem_, &horvariogrammnuitem_, true, false );
    mAddMenuItem( &algomnuitem_, &attr2geommnuitm_, true, false );
}


void uiODEarthModelSurfaceDataTreeItem::handleMenuCB( CallBacker* cb )
{
    uiODAttribTreeItem::handleMenuCB(cb);
    mCBCapsuleUnpackWithCaller( int, mnuid, caller, cb );
    mDynamicCastGet(MenuHandler*,menu,caller);
    if ( mnuid==-1 || menu->isHandled() )
	return;

    const int visid = displayID();
    const int attribnr = attribNr();

    uiVisPartServer* visserv = ODMainWin()->applMgr().visServer();
    const Attrib::SelSpec* as = visserv->getSelSpec( visid, attribnr );
    if ( mnuid==savesurfacedatamnuitem_.id )
    {
	menu->setIsHandled( true );
	DataPointSet vals( false, true );
	vals.bivSet().setNrVals( 3 );
	visserv->getRandomPosCache( visid, attribnr, vals );
	if ( vals.size() )
	{
	    const float shift = (float) visserv->getTranslation( visid ).z;
	    const int validx = visserv->selectedTexture( visid, attribnr ) + 2;
	    const int auxnr = applMgr()->EMServer()->setAuxData( emid_, vals,
		    name_.getFullString(), validx, shift );
	    if ( auxnr<0 )
	    {
		pErrMsg( "Cannot find Horizon Data." );
		return;
	    }

	    BufferString auxdatanm;
	    const bool saved =
		applMgr()->EMServer()->storeAuxData( emid_, auxdatanm, true );
	    if ( saved )
	    {
		const Attrib::SelSpec newas( auxdatanm,
			Attrib::SelSpec::cOtherAttrib() );
		visserv->setSelSpec( visid, attribnr, newas );
		BufferStringSet* userrefs = new BufferStringSet;
		userrefs->add( "Section ID" );
		userrefs->add( auxdatanm );
		visserv->setUserRefs( visid, attribnr, userrefs );
		updateColumnText( uiODSceneMgr::cNameColumn() );
	    }
	    changed_ = !saved;
	}
    }
    else if ( mnuid==depthattribmnuitem_.id )
    {
	menu->setIsHandled( true );
	uivisemobj_->setDepthAsAttrib( attribnr );
	updateColumnText( uiODSceneMgr::cNameColumn() );
	updateColumnText( uiODSceneMgr::cColorColumn() );
	applMgr()->updateColorTable( displayID(), attribnr );
	changed_ = false;
    }
    else if ( mnuid==loadsurfacedatamnuitem_.id )
    {
	menu->setIsHandled( true );
	selectAndLoadAuxData();
	changed_ = false;
    }
    else if ( mnuid==fillholesmnuitem_.id || mnuid==filtermnuitem_.id
	   || mnuid==attr2geommnuitm_.id || mnuid==horvariogrammnuitem_.id )
    {
	menu->setIsHandled( true );

	if ( !as || as->id().asInt()!=Attrib::SelSpec::cOtherAttrib().asInt() )
	{
	   uiMSG().error(tr("This algorithm can only be applied on "
			    "'Horizon Data'.\nPlease save attribute first"));
	    return;
	}

	bool res = false;
	DataPointSet vals( false, true );
	visserv->getRandomPosCache( visid, attribnr, vals );
	if ( mnuid==filtermnuitem_.id )
	    res = applMgr()->EMServer()->filterAuxData( emid_,
				    name_.getFullString(), vals);
	else if ( mnuid==fillholesmnuitem_.id )
	    res = applMgr()->EMServer()->
			interpolateAuxData( emid_,name_.getFullString(), vals);
	else if ( mnuid==horvariogrammnuitem_.id )
	    res = applMgr()->EMServer()->
		computeVariogramAuxData( emid_, name_.getFullString(), vals );
	else if ( mnuid==attr2geommnuitm_.id )
	    res = applMgr()->EMServer()->
		attr2Geom( emid_, name_.getFullString(), vals );

	if ( !res || mnuid==horvariogrammnuitem_.id )
	    return;

	visserv->setSelSpec( visid, attribnr,
		Attrib::SelSpec(name_.getFullString(),
				Attrib::SelSpec::cOtherAttrib()) );
	visserv->setRandomPosData( visid, attribnr, &vals );
	changed_ = true;
    }
}


void uiODEarthModelSurfaceDataTreeItem::selectAndLoadAuxData()
{
    if ( !applMgr()->EMServer()->showLoadAuxDataDlg(emid_) )
	return;

    mDynamicCastGet(visSurvey::HorizonDisplay*,vishor,
		    applMgr()->visServer()->getObject(displayID()) );
    if ( !vishor )
	return;

    const StepInterval<int>& grrg = vishor->geometryRowRange();
    const StepInterval<int>& gcrg = vishor->geometryColRange();
    const visBase::HorizonSection* horsect = vishor->getSection( 0 );
    StepInterval<int> loadrrg = horsect ? horsect->displayedRowRange()
					: grrg;
    StepInterval<int> loadcrg = horsect ? horsect->displayedColRange()
					: gcrg;
    TrcKeyZSampling cs( true );
    cs.hsamp_.set( loadrrg, loadcrg );

    TypeSet<float> shifts;
    DataPointSet vals( false, true );
    applMgr()->EMServer()->getAllAuxData( emid_, vals, &shifts, &cs );
    setDataPointSet( vals );
    vishor->setAttribShift( attribNr(), shifts );

    updateColumnText( uiODSceneMgr::cNameColumn() );
}


void uiODEarthModelSurfaceDataTreeItem::setDataPointSet(
						const DataPointSet& vals )
{
    const int visid = displayID();
    const int attribnr = attribNr();
    uiVisPartServer* visserv = ODMainWin()->applMgr().visServer();
    FixedString attrnm = vals.nrCols()>1 ? vals.colName(1) : "";
    visserv->setSelSpec( visid, attribnr,
	    Attrib::SelSpec(attrnm,Attrib::SelSpec::cOtherAttrib()) );
    visserv->setRandomPosData( visid, attribnr, &vals );
    visserv->selectTexture( visid, attribnr, 0 );
    updateColumnText( uiODSceneMgr::cNameColumn() );
}


uiString uiODEarthModelSurfaceDataTreeItem::createDisplayName() const
{
    uiVisPartServer* visserv = ODMainWin()->applMgr().visServer();
    const Attrib::SelSpec* as = visserv->getSelSpec( displayID(), attribNr() );

    if ( as && as->id().asInt()==Attrib::SelSpec::cNoAttrib().asInt() )
        return uiStrings::sZValue(mPlural);

    return uiODAttribTreeItem::createDisplayName();
}
