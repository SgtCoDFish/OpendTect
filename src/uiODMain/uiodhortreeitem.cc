/*+
___________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	K. Tingdahl
 Date:		Jul 2003
___________________________________________________________________

-*/

#include "uiodhortreeitem.h"

#include "bendpointfinder.h"
#include "datapointset.h"
#include "emhorizon2d.h"
#include "emhorizon3d.h"
#include "emmanager.h"
#include "emioobjinfo.h"
#include "emsurfaceauxdata.h"
#include "mpeengine.h"
#include "randomlinegeom.h"
#include "survinfo.h"

#include "uiattribpartserv.h"
#include "uiemattribpartserv.h"
#include "uiempartserv.h"
#include "uihor2dfrom3ddlg.h"
#include "uimenu.h"
#include "uimenuhandler.h"
#include "uimpepartserv.h"
#include "uimsg.h"
#include "uiodapplmgr.h"
#include "uiodscenemgr.h"
#include "uiposprovider.h"
#include "uitaskrunner.h"
#include "uitreeview.h"
#include "uivisemobj.h"
#include "uivispartserv.h"
#include "uistrings.h"

#include "visemobjdisplay.h"
#include "vishorizondisplay.h"
#include "vishorizonsection.h"
#include "vissurvscene.h"
#include "visrgbatexturechannel2rgba.h"
#include "zaxistransform.h"
#include "od_helpids.h"


#define mAddIdx		0
#define mAddAtSectIdx	1
#define mAddCBIdx	2
#define mNewIdx		3
#define mCreateIdx	4
#define mSectIdx	5
#define mFullIdx	6
#define mSectFullIdx	7

#define mTrackIdx	100
#define mConstIdx	10

uiODHorizonParentTreeItem::uiODHorizonParentTreeItem()
    : uiODTreeItem(
	uiStrings::phrJoinStrings(uiStrings::s3D(),uiStrings::sHorizon()))
    , newmenu_(uiStrings::sNew())
    , trackitem_(m3Dots(tr("Auto and Manual Tracking")),mTrackIdx)
    , constzitem_(m3Dots(tr("With Constant Z")),mConstIdx)
    , handleMenu(this)
{
    newmenu_.addItem( &trackitem_ );
    newmenu_.addItem( &constzitem_ );
}


uiODHorizonParentTreeItem::~uiODHorizonParentTreeItem()
{
}


const char* uiODHorizonParentTreeItem::iconName() const
{ return "tree-horizon3d"; }


void uiODHorizonParentTreeItem::removeChild( uiTreeItem* itm )
{
    uiTreeItem::removeChild( itm );
}


static void setSectionDisplayRestoreForAllHors( const uiVisPartServer& visserv,
						bool yn )
{
    for ( int id=visserv.highestID(); id>=0; id-- )
    {
	mDynamicCastGet( visSurvey::HorizonDisplay*,hd,visserv.getObject(id) );
	if ( hd )
	    hd->setSectionDisplayRestore( yn );
    }
}


bool uiODHorizonParentTreeItem::showSubMenu()
{
    mDynamicCastGet(visSurvey::Scene*,scene,
		    ODMainWin()->applMgr().visServer()->getObject(sceneID()));

    const bool hastransform = scene && scene->getZAxisTransform();

    uiMenu mnu( getUiParent(), uiStrings::sAction() );
    mnu.insertItem( new uiAction(m3Dots(uiStrings::sAdd())), mAddIdx );
    mnu.insertItem( new uiAction(m3Dots(tr("Add at Sections Only"))),
		    mAddAtSectIdx);
    mnu.insertItem( new uiAction(m3Dots(tr("Add Color Blended"))), mAddCBIdx );

    uiMenu* newmenu = new uiMenu( newmenu_ );
    mnu.insertItem( newmenu );
    newmenu->setEnabled( !hastransform && SI().has3D() );

    if ( children_.size() )
    {
	mnu.insertSeparator();
	uiMenu* displaymnu =
		new uiMenu( getUiParent(), tr("Display All") );
	displaymnu->insertItem( new uiAction(tr("Only at Sections")),
				mSectIdx );
	displaymnu->insertItem( new uiAction(tr("In Full")), mFullIdx );
	displaymnu->insertItem( new uiAction(tr("At Sections and in Full")),
				mSectFullIdx );
	mnu.insertItem( displaymnu );
    }

    addStandardItems( mnu );

    const int mnuid = mnu.exec();
    handleMenu.trigger( mnuid );
    if ( mnuid == mAddIdx || mnuid==mAddAtSectIdx || mnuid==mAddCBIdx )
    {
	setSectionDisplayRestoreForAllHors( *applMgr()->visServer(), true );

	ObjectSet<EM::EMObject> objs;
	applMgr()->EMServer()->selectHorizons( objs, false );
	for ( int idx=0; idx<objs.size(); idx++ )
	{
	    if ( MPE::engine().getTrackerByObject(objs[idx]->id()) != -1 )
	    {
		MPE::engine().addTracker( objs[idx] );
		applMgr()->visServer()->turnSeedPickingOn( true );
	    }
	    uiODHorizonTreeItem* itm =
		new uiODHorizonTreeItem( objs[idx]->id(), mnuid==mAddCBIdx,
					 mnuid==mAddAtSectIdx );
	    addChld( itm, false, false );
	}
	deepUnRef( objs );

	setSectionDisplayRestoreForAllHors( *applMgr()->visServer(), false );
    }
    else if ( mnuid == trackitem_.id )
    {
	uiMPEPartServer* mps = applMgr()->mpeServer();
	mps->setCurrentAttribDescSet(
				applMgr()->attrServer()->curDescSet(false) );

	mps->addTracker( EM::Horizon3D::typeStr(), sceneID() );
	return true;
    }
    else if ( mnuid == mSectIdx || mnuid == mFullIdx || mnuid == mSectFullIdx )
    {
	const bool onlyatsection = mnuid == mSectIdx;
	const bool both = mnuid == mSectFullIdx;
	for ( int idx=0; idx<children_.size(); idx++ )
	{
	    mDynamicCastGet(uiODEarthModelSurfaceTreeItem*,itm,children_[idx])
	    if ( !itm || !itm->visEMObject() )
		continue;

	    const int displayid = itm->visEMObject()->id();
	    mDynamicCastGet(visSurvey::HorizonDisplay*,hd,
			    applMgr()->visServer()->getObject(displayid))
	    if ( !hd ) continue;

	    hd->displayIntersectionLines( both );
	    hd->setOnlyAtSectionsDisplay( onlyatsection );
	    hd->enableAttrib( (hd->nrAttribs()-1), !onlyatsection );
	    itm->updateColumnText( uiODSceneMgr::cColorColumn() );
	}
    }
    else if ( mnuid == constzitem_.id )
    {
	applMgr()->EMServer()->createHorWithConstZ( false );
    }
    else
	handleStandardItems( mnuid );

    return true;
}


static uiTreeItem* gtItm( const MultiID& mid, ObjectSet<uiTreeItem>& itms )
{
    for ( int idx=0; idx<itms.size(); idx++ )
    {
	mDynamicCastGet(const uiODEarthModelSurfaceTreeItem*,itm,itms[idx])
	const EM::ObjectID emid = itm && itm->visEMObject() ?
		     itm->visEMObject()->getObjectID() : -1;
	if ( mid == EM::EMM().getMultiID(emid) )
	    return itms[idx];
    }

    return 0;
}


void uiODHorizonParentTreeItem::sort()
{
    TypeSet<MultiID> mids, sortedmids;
    for ( int idx=0; idx<children_.size(); idx++ )
    {
	mDynamicCastGet(const uiODEarthModelSurfaceTreeItem*,itm,children_[idx])
	if ( !itm || !itm->visEMObject() )
	    continue;

	const EM::ObjectID emid = itm->visEMObject()->getObjectID();
	mids += EM::EMM().getMultiID( emid );
    }

    EM::IOObjInfo::sortHorizonsOnZValues( mids, sortedmids );
    uiTreeItem* previtm = 0;
    for ( int idx=sortedmids.size()-1; idx>=0; idx-- )
    {
	uiTreeItem* itm = gtItm( sortedmids[idx], children_ );
	if ( !itm ) continue;

	if ( !previtm )
	    itm->moveItemToTop();
	else
	    itm->moveItem( previtm );

	previtm = itm;
    }
}


bool uiODHorizonParentTreeItem::addChld( uiTreeItem* child, bool below,
					  bool downwards )
{
    bool res = uiTreeItem::addChld( child, below, downwards );
    if ( res ) sort();
    return res;
}


uiTreeItem*
    uiODHorizonTreeItemFactory::createForVis( int visid, uiTreeItem* ) const
{
    const FixedString objtype = uiVisEMObject::getObjectType(visid);
    if ( !objtype ) return 0;

    mDynamicCastGet(visSurvey::HorizonDisplay*,hd,
	ODMainWin()->applMgr().visServer()->getObject(visid));
    if ( hd )
    {
	mDynamicCastGet( visBase::RGBATextureChannel2RGBA*, rgba,
			 hd->getChannels2RGBA() );
	const bool atsection = hd->displayedOnlyAtSections();
	return new uiODHorizonTreeItem( visid, rgba, atsection, true );
    }


    return 0;
}


// uiODHorizonTreeItem

uiODHorizonTreeItem::uiODHorizonTreeItem( const EM::ObjectID& emid, bool rgba,
					  bool atsect )
    : uiODEarthModelSurfaceTreeItem(emid)
    , rgba_(rgba)
    , atsections_(atsect)
{ initMenuItems(); }


uiODHorizonTreeItem::uiODHorizonTreeItem( int visid, bool rgba, bool atsect,
					  bool dummy )
    : uiODEarthModelSurfaceTreeItem(0)
    , rgba_(rgba)
    , atsections_(atsect)
{
    initMenuItems();
    displayid_ = visid;
}


void uiODHorizonTreeItem::initMenuItems()
{
    hordatamnuitem_.text = tr("Horizon Data");
    algomnuitem_.text = uiStrings::sTools();
    workflowsmnuitem_.text = tr("Workflows");
    positionmnuitem_.text = m3Dots(tr("Position"));
    shiftmnuitem_.text = m3Dots(uiStrings::sShift());
    fillholesmnuitem_.text = m3Dots(tr("Gridding"));
    filterhormnuitem_.text = m3Dots(tr("Filtering"));
    snapeventmnuitem_.text = m3Dots(tr("Snapping"));
    geom2attrmnuitem_.text = m3Dots(tr("Store Z as Attribute"));

    parentsrdlmnuitem_.text = tr("Show Parents Path");
    parentsmnuitem_.text = tr("Select Parents");
    childrenmnuitem_.text = tr("Select Children");
    delchildrenmnuitem_.text = tr("Delete Selected Children");
    lockmnuitem_.text = uiStrings::sLock();
    unlockmnuitem_.text = uiStrings::sUnlock();
}


void uiODHorizonTreeItem::initNotify()
{
    mDynamicCastGet(visSurvey::EMObjectDisplay*,
		    emd,visserv_->getObject(displayid_));
    if ( emd )
	emd->changedisplay.notify( mCB(this,uiODHorizonTreeItem,dispChangeCB) );
}


uiString uiODHorizonTreeItem::createDisplayName() const
{
    const uiVisPartServer* cvisserv =
	const_cast<uiODHorizonTreeItem*>(this)->visserv_;

    uiString res = cvisserv->getObjectName( displayid_ );

    if (  uivisemobj_ && uivisemobj_->getShift() )
    {
	res.append( toUiString(" (%1)").arg(
	  toUiString(uivisemobj_->getShift() * SI().zDomain().userFactor())));
    }

    return res;
}


bool uiODHorizonTreeItem::init()
{
    if ( !createUiVisObj() )
	return false;

    mDynamicCastGet(visSurvey::HorizonDisplay*,hd,
		    visserv_->getObject(displayid_));
    if ( rgba_ )
    {
	if ( !hd ) return false;

	mDynamicCastGet( visBase::RGBATextureChannel2RGBA*, rgba,
			hd->getChannels2RGBA() );
	if ( !rgba )
	{
	    if ( !hd->setChannels2RGBA(
			visBase::RGBATextureChannel2RGBA::create()) )
		return false;

	    hd->addAttrib();
	    hd->addAttrib();
	    hd->addAttrib();
	}
    }

    if ( hd ) hd->setOnlyAtSectionsDisplay( atsections_ );
    const bool res = uiODEarthModelSurfaceTreeItem::init();
    if ( !res ) return res;

    mDynamicCastGet(const EM::Horizon3D*,hor3d,EM::EMM().getObject(emid_))
    if ( hor3d )
    {
	hd->setDepthAsAttrib( 0 );
	if ( !applMgr()->isRestoringSession() )
	{
	    const int nrauxdata = hor3d->auxdata.nrAuxData();
	    for ( int idx=0; idx<nrauxdata; idx++ )
	    {
		if ( !hor3d->auxdata.auxDataName(idx) )
		    continue;

		DataPointSet vals( false, true );
		float shift;
		applMgr()->EMServer()->getAuxData( emid_, idx, vals, shift );

		uiODDataTreeItem* itm = addAttribItem();
		mDynamicCastGet(uiODEarthModelSurfaceDataTreeItem*,emitm,itm);
		if ( emitm ) emitm->setDataPointSet( vals );
	    }
	}

	if ( MPE::engine().getTrackerByObject(hor3d->id()) != -1 )
	{
	    for ( int idx=0; idx<nrChildren(); idx++ )
		getChild(idx)->setChecked( false, true );
	}
    }

    return res;
}


void uiODHorizonTreeItem::dispChangeCB(CallBacker*)
{
    updateColumnText( uiODSceneMgr::cColorColumn() );
}


bool uiODHorizonTreeItem::askContinueAndSaveIfNeeded( bool withcancel )
{
    return applMgr()->EMServer()->askUserToSave( emid_, withcancel );
}


void uiODHorizonTreeItem::createMenu( MenuHandler* menu, bool istb )
{
    uiODEarthModelSurfaceTreeItem::createMenu( menu, istb );
    if ( istb ) return;

    mDynamicCastGet(uiMenuHandler*,uimenu,menu)
    mDynamicCastGet(visSurvey::Scene*,scene,visserv_->getObject(sceneID()));
    const bool hastransform = scene && scene->getZAxisTransform();

    if ( !menu || menu->menuID()!=displayID() )
	return;

    const bool islocked = visserv_->isLocked( displayID() );
    const bool canadd = visserv_->canAddAttrib( displayID() );

    mAddMenuItem( &addmnuitem_, &hordatamnuitem_, !islocked && canadd, false );

    if ( hastransform )
    {
	mResetMenuItem( &positionmnuitem_ );
	mResetMenuItem( &shiftmnuitem_ );
	mResetMenuItem( &fillholesmnuitem_ );
	mResetMenuItem( &filterhormnuitem_ );
	mResetMenuItem( &snapeventmnuitem_ );
	mResetMenuItem( &geom2attrmnuitem_ );
	mResetMenuItem( &createflatscenemnuitem_ );
	return;
    }

    mAddMenuItem( &displaymnuitem_, &positionmnuitem_, true, false );
    mAddMenuItem( menu, &algomnuitem_, true, false );
    mAddMenuItem( &algomnuitem_, &filterhormnuitem_, !islocked, false );
    mAddMenuItem( &algomnuitem_, &fillholesmnuitem_, !islocked, false );
    mAddMenuItem( &algomnuitem_, &shiftmnuitem_, !islocked, false )
    mAddMenuItem( &algomnuitem_, &snapeventmnuitem_, !islocked, false );
    mAddMenuItem( &algomnuitem_, &geom2attrmnuitem_, !islocked, false );

    mAddMenuItem( menu, &workflowsmnuitem_, true, false );
    mAddMenuItem( &workflowsmnuitem_, &createflatscenemnuitem_,true, false);

    const bool hastracker = MPE::engine().getTrackerByObject(emid_) >= 0;
    MenuItem* trackmnu = menu->findItem(
	    uiStrings::sTracking().getFullString() );
    if ( hastracker && trackmnu )
    {
	const Coord3& crd = uimenu->getPickedPos();
	if ( crd.isDefined() )
	{
	    mAddMenuItem( trackmnu, &parentsmnuitem_, true, false );
	    mAddMenuItem( trackmnu, &parentsrdlmnuitem_, true, false );
	    mAddMenuItem( trackmnu, &childrenmnuitem_, true, false );
	    mAddMenuItem( trackmnu, &delchildrenmnuitem_, true, false );
	}
	else
	{
	    mResetMenuItem( &parentsmnuitem_ );
	    mResetMenuItem( &childrenmnuitem_ );
	    mResetMenuItem( &delchildrenmnuitem_ );
	    mResetMenuItem( &parentsrdlmnuitem_ );
	}

	mAddMenuItem( trackmnu, &lockmnuitem_, true, false );
	mAddMenuItem( trackmnu, &unlockmnuitem_, true, false );
    }
    else
    {
	mResetMenuItem( &parentsmnuitem_ );
	mResetMenuItem( &childrenmnuitem_ );
	mResetMenuItem( &delchildrenmnuitem_ );
	mResetMenuItem( &parentsrdlmnuitem_ );
	mResetMenuItem( &lockmnuitem_ );
	mResetMenuItem( &unlockmnuitem_ );
    }
}


#define mUpdateTexture() \
{ \
    for ( int idx=0; idx<hd->nrAttribs(); idx++ ) \
    { \
	if ( hd->hasDepth(idx) ) hd->setDepthAsAttrib( idx ); \
	else applMgr()->calcRandomPosAttrib( visid, idx ); \
    } \
}

void uiODHorizonTreeItem::handleMenuCB( CallBacker* cb )
{
    uiODEarthModelSurfaceTreeItem::handleMenuCB( cb );
    mCBCapsuleUnpackWithCaller( int, mnuid, caller, cb );
    mDynamicCastGet(MenuHandler*,menu,caller)
    mDynamicCastGet(uiMenuHandler*,uimenu,caller)
    if ( menu->isHandled() || menu->menuID()!=displayID() || mnuid==-1 )
	return;

    const int visid = displayID();
    mDynamicCastGet( visSurvey::HorizonDisplay*, hd,
		     visserv_->getObject(visid) );
    mDynamicCastGet(EM::Horizon3D*,hor3d,EM::EMM().getObject(emid_))
    if ( !hd || !hor3d ) return;

    uiEMPartServer* emserv = applMgr()->EMServer();
    uiEMAttribPartServer* emattrserv = applMgr()->EMAttribServer();
    uiAttribPartServer* attrserv = applMgr()->attrServer();
    bool handled = true;
    if ( mnuid==fillholesmnuitem_.id )
    {
	const bool isoverwrite = emserv->fillHoles( emid_, false );
	if ( isoverwrite ) { mUpdateTexture(); }
    }
    else if ( mnuid==filterhormnuitem_.id )
    {
	const bool isoverwrite = emserv->filterSurface( emid_ );
	if ( isoverwrite ) { mUpdateTexture(); }
    }
    else if ( mnuid==snapeventmnuitem_.id )
	emattrserv->snapHorizon( emid_, false );
    else if ( mnuid==geom2attrmnuitem_.id )
    {
	if ( applMgr()->EMServer()->geom2Attr(emid_) )
	    mUpdateTexture();
    }
    else if ( mnuid==positionmnuitem_.id )
    {
	menu->setIsHandled(true);

	visBase::HorizonSection* section = hd->getHorizonSection( 0 );
	if ( !section )
	    return;

	TrcKeyZSampling maxcs = SI().sampling(true);;
	mDynamicCastGet(visSurvey::Scene*,scene,visserv_->getObject(sceneID()))
	if ( scene && scene->getZAxisTransform() )
	{
	    const Interval<float> zintv =
		scene->getZAxisTransform()->getZInterval( false );
	    maxcs.zsamp_.start = zintv.start;
	    maxcs.zsamp_.stop = zintv.stop;
	}

	TrcKeyZSampling curcs;
	curcs.zsamp_.setFrom( SI().zRange(true) );
	curcs.hsamp_.set( section->displayedRowRange(),
		       section->displayedColRange() );

	uiPosProvider::Setup setup( false, true, false );
	setup.allownone_ = true;
	setup.seltxt( tr("Area subselection") );
	setup.tkzs_ = maxcs;

	uiDialog dlg( getUiParent(),
	uiDialog::Setup( uiStrings::sPosition(mPlural),
			    tr("Specify positions"),
				mODHelpKey(mPosProvSelHelpID) ) );
	uiPosProvider pp( &dlg, setup );

	IOPar displaypar;
	pp.fillPar( displaypar );	//Get display type
	curcs.fillPar( displaypar );	//Get display ranges
	pp.usePar( displaypar );

	if ( !dlg.go() )
	    return;

	MouseCursorChanger cursorlock( MouseCursor::Wait );
	pp.fillPar( displaypar );

	TrcKeyZSampling newcs;
	if ( pp.isAll() )
	    newcs = maxcs;
	else
	    newcs.usePar( displaypar );

	section->setDisplayRange( newcs.hsamp_.inlRange(),
				  newcs.hsamp_.crlRange() );
	emserv->setHorizon3DDisplayRange( newcs.hsamp_ );

	for ( int idx=0; idx<hd->nrAttribs(); idx++ )
	{
	    if ( hd->hasDepth(idx) )
		hd->setDepthAsAttrib( idx );
	    else
		applMgr()->calcRandomPosAttrib( visid, idx );
	}
    }
    else if ( mnuid==shiftmnuitem_.id )
    {
	BoolTypeSet isenabled;
	const int nrattrib = visserv_->getNrAttribs( visid );
	for ( int idx=0; idx<nrattrib; idx++ )
	    isenabled += visserv_->isAttribEnabled( visid, idx );

	float curshift = (float) visserv_->getTranslation( visid ).z;
	if ( mIsUdf( curshift ) ) curshift = 0;

	emattrserv->setDescSet( attrserv->curDescSet(false) );
	emattrserv->showHorShiftDlg( emid_, visid, isenabled, curshift,
				     visserv_->canAddAttrib(visid,1) );
    }
    else if ( mnuid==hordatamnuitem_.id )
    {
	uiODDataTreeItem* itm = addAttribItem();
	mDynamicCastGet(uiODEarthModelSurfaceDataTreeItem*,emitm,itm);
	if ( emitm ) emitm->selectAndLoadAuxData();
    }
    else if ( mnuid==parentsrdlmnuitem_.id )
    {
	const TrcKey tk = SI().transform( uimenu->getPickedPos() );
	applMgr()->addMPEParentPath( hd->id(), tk );
    }
    else if ( mnuid==parentsmnuitem_.id )
    {
	const TrcKey tk = SI().transform( uimenu->getPickedPos() );
	hd->selectParent( tk );
    }
    else if ( mnuid==childrenmnuitem_.id )
    {
	const TrcKey tk = SI().transform( uimenu->getPickedPos() );
	hor3d->selectChildren( tk );
    }
    else if ( mnuid==delchildrenmnuitem_.id )
    {
	hor3d->deleteChildren();
	hd->showSelections( false );
    }
    else if ( mnuid==lockmnuitem_.id )
	hor3d->lockAll();
    else if ( mnuid==unlockmnuitem_.id )
	hor3d->unlockAll();
    else
	handled = false;

    menu->setIsHandled( handled );
}


uiODHorizon2DParentTreeItem::uiODHorizon2DParentTreeItem()
    : uiODTreeItem(
	uiStrings::phrJoinStrings(uiStrings::s2D(),uiStrings::sHorizon()) )
{}


const char* uiODHorizon2DParentTreeItem::iconName() const
{ return "tree-horizon2d"; }


void uiODHorizon2DParentTreeItem::removeChild( uiTreeItem* itm )
{
    uiTreeItem::removeChild( itm );
}


bool uiODHorizon2DParentTreeItem::showSubMenu()
{
    mDynamicCastGet(visSurvey::Scene*,scene,
		    ODMainWin()->applMgr().visServer()->getObject(sceneID()));
    const bool hastransform = scene && scene->getZAxisTransform();
    uiMenu mnu( getUiParent(), uiStrings::sAction() );
    mnu.insertItem( new uiAction( m3Dots(uiStrings::sAdd()) ), 0 );
    uiAction* newmenu = new uiAction( m3Dots(tr("Track New")) );
    mnu.insertItem( newmenu, 1 );
    mnu.insertItem( new uiAction(m3Dots(tr("Create from 3D"))), 2 );
    newmenu->setEnabled( !hastransform );
    if ( children_.size() )
    {
	mnu.insertSeparator();
	mnu.insertItem( new uiAction(tr("Display All Only at Sections")), 3 );
	mnu.insertItem( new uiAction(tr("Show All in Full")), 4 );
    }
    addStandardItems( mnu );

    const int mnuid = mnu.exec();
    if ( mnuid == 0 )
    {
	ObjectSet<EM::EMObject> objs;
	applMgr()->EMServer()->selectHorizons( objs, true );
	for ( int idx=0; idx<objs.size(); idx++ )
	{
	    if ( MPE::engine().getTrackerByObject(objs[idx]->id()) != -1 )
	    {
		MPE::engine().addTracker( objs[idx] );
		applMgr()->visServer()->turnSeedPickingOn( true );
	    }
	    addChld( new uiODHorizon2DTreeItem(objs[idx]->id()), false, false);
	}

	deepUnRef( objs );
    }
    else if ( mnuid == 1 )
    {
	uiMPEPartServer* mps = applMgr()->mpeServer();
	mps->setCurrentAttribDescSet(
			applMgr()->attrServer()->curDescSet(true) );

	mps->addTracker( EM::Horizon2D::typeStr(), sceneID() );
	return true;
    }
    else if ( mnuid == 2 )
    {
	uiHor2DFrom3DDlg dlg( getUiParent() );
	if( dlg.go() && dlg.doDisplay() )
	    addChld( new uiODHorizon2DTreeItem(dlg.getEMObjID()), true, false);
    }
    else if ( mnuid == 3 || mnuid == 4 )
    {
	const bool onlyatsection = mnuid == 3;
	for ( int idx=0; idx<children_.size(); idx++ )
	{
	    mDynamicCastGet(uiODHorizon2DTreeItem*,itm,children_[idx])
	    if ( itm && itm->visEMObject() )
	    {
		itm->visEMObject()->setOnlyAtSectionsDisplay( onlyatsection );
		itm->updateColumnText( uiODSceneMgr::cColorColumn() );
	    }
	}
    }
    else
	handleStandardItems( mnuid );

    return true;
}


void uiODHorizon2DParentTreeItem::sort()
{
    TypeSet<MultiID> mids, sortedmids;
    for ( int idx=0; idx<children_.size(); idx++ )
    {
	mDynamicCastGet(const uiODEarthModelSurfaceTreeItem*,itm,children_[idx])
	if ( !itm || !itm->visEMObject() )
	    continue;

	const EM::ObjectID emid = itm->visEMObject()->getObjectID();
	mids += EM::EMM().getMultiID( emid );
    }

    EM::IOObjInfo::sortHorizonsOnZValues( mids, sortedmids );
    uiTreeItem* previtm = 0;
    for ( int idx=sortedmids.size()-1; idx>=0; idx-- )
    {
	uiTreeItem* itm = gtItm( sortedmids[idx], children_ );
	if ( !itm ) continue;

	if ( !previtm )
	    itm->moveItemToTop();
	else
	    itm->moveItem( previtm );

	previtm = itm;
    }
}


bool uiODHorizon2DParentTreeItem::addChld( uiTreeItem* child, bool below,
					    bool downwards )
{
    bool res = uiTreeItem::addChld( child, below, downwards );
    if ( res ) sort();
    return res;
}


uiTreeItem*
    uiODHorizon2DTreeItemFactory::createForVis( int visid, uiTreeItem* ) const
{
    FixedString objtype = uiVisEMObject::getObjectType(visid);
    return objtype==EM::Horizon2D::typeStr()
	? new uiODHorizon2DTreeItem(visid,true) : 0;
}


// uiODHorizon2DTreeItem

uiODHorizon2DTreeItem::uiODHorizon2DTreeItem( const EM::ObjectID& objid )
    : uiODEarthModelSurfaceTreeItem( objid )
{ initMenuItems(); }


uiODHorizon2DTreeItem::uiODHorizon2DTreeItem( int id, bool )
    : uiODEarthModelSurfaceTreeItem( 0 )
{
    initMenuItems();
    displayid_=id;
}


void uiODHorizon2DTreeItem::initMenuItems()
{
    algomnuitem_.text = uiStrings::sTools();
    workflowsmnuitem_.text = tr("Workflows");
    derive3dhormnuitem_.text = m3Dots(tr("Derive 3D horizon"));
    snapeventmnuitem_.text = m3Dots(tr("Snapping"));
    interpolatemnuitem_.text = m3Dots(tr("Interpolate"));
}


void uiODHorizon2DTreeItem::initNotify()
{
    mDynamicCastGet(visSurvey::EMObjectDisplay*,
		    emd,visserv_->getObject(displayid_));
    if ( emd )
	emd->changedisplay.notify(mCB(this,uiODHorizon2DTreeItem,dispChangeCB));
}


void uiODHorizon2DTreeItem::dispChangeCB(CallBacker*)
{
    updateColumnText( uiODSceneMgr::cColorColumn() );
}


bool uiODHorizon2DTreeItem::askContinueAndSaveIfNeeded( bool withcancel )
{
    return applMgr()->EMServer()->askUserToSave( emid_, withcancel );
}


void uiODHorizon2DTreeItem::createMenu( MenuHandler* menu, bool istb )
{
    uiODEarthModelSurfaceTreeItem::createMenu( menu, istb );
    if ( istb ) return;

    if ( !menu || menu->menuID()!=displayID() )
    {
	mResetMenuItem( &derive3dhormnuitem_ );
	mResetMenuItem( &createflatscenemnuitem_ );
	mResetMenuItem( &snapeventmnuitem_ );
	mResetMenuItem( &interpolatemnuitem_ );
    }
    else
    {
	const bool islocked = visserv_->isLocked( displayID() );
	const bool isempty = applMgr()->EMServer()->isEmpty( emid_ );
	const bool enab = !islocked && !isempty;
	mAddMenuItem( menu, &algomnuitem_, true, false );
	mAddMenuItem( &algomnuitem_, &snapeventmnuitem_, enab, false );
	mAddMenuItem( &algomnuitem_, &interpolatemnuitem_, enab, false );

	mAddMenuItem( menu, &workflowsmnuitem_, true, false );
	mAddMenuItem( &workflowsmnuitem_, &derive3dhormnuitem_, enab, false );
	mAddMenuItem( &workflowsmnuitem_, &createflatscenemnuitem_, enab,false);
    }
}


void uiODHorizon2DTreeItem::handleMenuCB( CallBacker* cb )
{
    uiODEarthModelSurfaceTreeItem::handleMenuCB( cb );
    mCBCapsuleUnpackWithCaller( int, mnuid, caller, cb );
    mDynamicCastGet(MenuHandler*,menu,caller)
    if ( menu->isHandled() || menu->menuID()!=displayID() || mnuid==-1 )
	return;

    bool handled = true;
    if ( mnuid==interpolatemnuitem_.id )
    {
	const int visid = displayID();
	const bool isoverwrite = applMgr()->EMServer()->fillHoles( emid_, true);
	mDynamicCastGet(visSurvey::HorizonDisplay*,hd,
			visserv_->getObject(visid));
	if ( hd && isoverwrite )
	{
	    mUpdateTexture();
	}
    }
    else if ( mnuid==derive3dhormnuitem_.id )
	applMgr()->EMServer()->deriveHor3DFrom2D( emid_ );
    else if ( mnuid==snapeventmnuitem_.id )
	applMgr()->EMAttribServer()->snapHorizon( emid_,true );
    else
	handled = false;

    menu->setIsHandled( handled );
}
