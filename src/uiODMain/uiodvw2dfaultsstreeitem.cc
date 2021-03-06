/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Umesh Sinha
 Date:		June 2010
________________________________________________________________________

-*/

#include "uiodvw2dfaultsstreeitem.h"

#include "uiempartserv.h"
#include "uiflatviewstdcontrol.h"
#include "uimenu.h"
#include "uiodapplmgr.h"
#include "uiodviewer2d.h"
#include "uiodviewer2dmgr.h"
#include "uipixmap.h"
#include "uitreeview.h"
#include "uistrings.h"
#include "uivispartserv.h"

#include "emeditor.h"
#include "emfaultstickset.h"
#include "emmanager.h"
#include "emobject.h"
#include "ioman.h"
#include "ioobj.h"
#include "mpeengine.h"
#include "randcolor.h"

#include "view2dfaultss3d.h"
#include "view2ddataman.h"


uiODVw2DFaultSSParentTreeItem::uiODVw2DFaultSSParentTreeItem()
    : uiODVw2DTreeItem( uiStrings::sFaultStickSet())
{
}


uiODVw2DFaultSSParentTreeItem::~uiODVw2DFaultSSParentTreeItem()
{
}


bool uiODVw2DFaultSSParentTreeItem::showSubMenu()
{
    uiMenu mnu( getUiParent(), uiStrings::sAction() );
    uiMenu* addmenu = new uiMenu( uiStrings::sAdd() );
    addmenu->insertItem( new uiAction(tr("In all 2D Viewers")), 1 );
    addmenu->insertItem( new uiAction(tr("Only in this 2D Viewer")), 2 );
    mnu.insertItem( addmenu );
    mnu.insertItem( new uiAction(uiStrings::sNew()), 0 );
    insertStdSubMenu( mnu );
    return handleSubMenu( mnu.exec() );
}


bool uiODVw2DFaultSSParentTreeItem::handleSubMenu( int mnuid )
{
    handleStdSubMenu( mnuid );

    if ( mnuid == 0 )
    {
	RefMan<EM::EMObject> emo =
		EM::EMM().createTempObject( EM::FaultStickSet::typeStr() );
	if ( !emo )
	    return false;

	emo->setPreferredColor( getRandomColor(false) );
	emo->setNewName();
	emo->setFullyLoaded( true );
	addNewTempFaultSS( emo->id() );
	applMgr()->viewer2DMgr().addNewTempFaultSS(
		emo->id(), viewer2D()->getSyncSceneID() );
	applMgr()->viewer2DMgr().addNewTempFaultSS2D(
		emo->id(), viewer2D()->getSyncSceneID() );
    }
    else if ( mnuid == 1 || mnuid==2 )
    {
	ObjectSet<EM::EMObject> objs;
	applMgr()->EMServer()->selectFaultStickSets( objs );
	TypeSet<EM::ObjectID> emids;
	for ( int idx=0; idx<objs.size(); idx++ )
	    emids += objs[idx]->id();

	if ( mnuid==1 )
	{
	    addFaultSSs( emids );
	    applMgr()->viewer2DMgr().addFaultSSs( emids );
	    applMgr()->viewer2DMgr().addFaultSS2Ds( emids );
	}
	else
	    addFaultSSs( emids );

	deepUnRef( objs );
    }

    return true;
}


const char* uiODVw2DFaultSSParentTreeItem::iconName() const
{ return "tree-fltss"; }


bool uiODVw2DFaultSSParentTreeItem::init()
{ return uiODVw2DTreeItem::init(); }


void uiODVw2DFaultSSParentTreeItem::getFaultSSVwr2DIDs(
	EM::ObjectID emid, TypeSet<int>& vw2dobjids ) const
{
    for ( int idx=0; idx<nrChildren(); idx++ )
    {
	mDynamicCastGet(const uiODVw2DFaultSSTreeItem*,faultssitem,
			getChild(idx))
	if ( !faultssitem || faultssitem->emObjectID() != emid )
	    continue;

	vw2dobjids.addIfNew( faultssitem->vw2DObject()->id() );
    }
}


void uiODVw2DFaultSSParentTreeItem::getLoadedFaultSSs(
	TypeSet<EM::ObjectID>& emids ) const
{
    for ( int idx=0; idx<nrChildren(); idx++ )
    {
	mDynamicCastGet(const uiODVw2DFaultSSTreeItem*,faultitem,getChild(idx))
	if ( !faultitem )
	    continue;
	emids.addIfNew( faultitem->emObjectID() );
    }
}


void uiODVw2DFaultSSParentTreeItem::removeFaultSS( EM::ObjectID emid )
{
    for ( int idx=0; idx<nrChildren(); idx++ )
    {
	mDynamicCastGet(uiODVw2DFaultSSTreeItem*,faultitem,getChild(idx))
	if ( !faultitem || emid!=faultitem->emObjectID() )
	    continue;
	removeChild( faultitem );
    }
}


void uiODVw2DFaultSSParentTreeItem::addFaultSSs(
					const TypeSet<EM::ObjectID>& emids )
{
    TypeSet<EM::ObjectID> emidstobeloaded, emidsloaded;
    getLoadedFaultSSs( emidsloaded );
    for ( int idx=0; idx<emids.size(); idx++ )
    {
	if ( !emidsloaded.isPresent(emids[idx]) )
	    emidstobeloaded.addIfNew( emids[idx] );
    }

    for ( int idx=0; idx<emidstobeloaded.size(); idx++ )
    {
	const EM::EMObject* emobj = EM::EMM().getObject( emidstobeloaded[idx] );
	if ( !emobj || findChild(emobj->name()) )
	    continue;

	MPE::ObjectEditor* editor =
	    MPE::engine().getEditor( emobj->id(), false );
	uiODVw2DFaultSSTreeItem* childitem =
	    new uiODVw2DFaultSSTreeItem( emidstobeloaded[idx] );
	addChld( childitem ,false, false );
	if ( editor )
	{
	    editor->addUser();
	    viewer2D()->viewControl()->setEditMode( true );
	    childitem->select();
	}
    }
}


void uiODVw2DFaultSSParentTreeItem::addNewTempFaultSS( EM::ObjectID emid )
{
    TypeSet<EM::ObjectID> emidsloaded;
    getLoadedFaultSSs( emidsloaded );
    if ( emidsloaded.isPresent(emid) )
	return;

    uiODVw2DFaultSSTreeItem* faulttreeitem = new uiODVw2DFaultSSTreeItem(emid);
    addChld( faulttreeitem,false, false );
    viewer2D()->viewControl()->setEditMode( true );
    faulttreeitem->select();
}


uiODVw2DFaultSSTreeItem::uiODVw2DFaultSSTreeItem( const EM::ObjectID& emid )
    : uiODVw2DEMTreeItem( emid )
    , fssview_(0)
{}


uiODVw2DFaultSSTreeItem::uiODVw2DFaultSSTreeItem( int id, bool )
    : uiODVw2DEMTreeItem( -1 )
    , fssview_(0)
{
    displayid_ = id;
}


uiODVw2DFaultSSTreeItem::~uiODVw2DFaultSSTreeItem()
{
    detachAllNotifiers();
    MPE::engine().removeEditor( emid_ );
    if ( fssview_ )
	viewer2D()->dataMgr()->removeObject( fssview_ );
}


bool uiODVw2DFaultSSTreeItem::init()
{
    EM::EMObject* emobj = 0;
    if ( displayid_ < 0 )
    {
	emobj = EM::EMM().getObject( emid_ );
	if ( !emobj ) return false;
	fssview_ = VW2DFaultSS3D::create( emid_, viewer2D()->viewwin(),
				     viewer2D()->dataEditor() );
	viewer2D()->dataMgr()->addObject( fssview_ );
	displayid_ = fssview_->id();
    }
    else
    {
	mDynamicCastGet(VW2DFaultSS3D*,hd,
			viewer2D()->dataMgr()->getObject(displayid_))
	if ( !hd )
	    return false;
	emid_ = hd->emID();
	emobj = EM::EMM().getObject( emid_ );
	if ( !emobj ) return false;

	fssview_ = hd;
    }

    emobj->change.notify( mCB(this,uiODVw2DFaultSSTreeItem,emobjChangeCB) );
    displayMiniCtab();
    name_ = applMgr()->EMServer()->getName( emid_ );
    uitreeviewitem_->setCheckable(true);
    uitreeviewitem_->setChecked( true );
    checkStatusChange()->notify( mCB(this,uiODVw2DFaultSSTreeItem,checkCB) );

    fssview_->draw();

    mAttachCB( viewer2D()->viewControl()->editPushed(),
	       uiODVw2DFaultSSTreeItem::enableKnotsCB );

    NotifierAccess* deselnotify =  fssview_->deSelection();
    if ( deselnotify )
	deselnotify->notify( mCB(this,uiODVw2DFaultSSTreeItem,deSelCB) );

    uiODVw2DTreeItem::addKeyBoardEvent();
    return true;
}


void uiODVw2DFaultSSTreeItem::displayMiniCtab()
{
    EM::EMObject* emobj = EM::EMM().getObject( emid_ );
    if ( !emobj ) return;

    uiTreeItem::updateColumnText( uiODViewer2DMgr::cColorColumn() );
    uitreeviewitem_->setPixmap( uiODViewer2DMgr::cColorColumn(),
				emobj->preferredColor() );
}


void uiODVw2DFaultSSTreeItem::emobjChangeCB( CallBacker* cb )
{
    mCBCapsuleUnpackWithCaller( const EM::EMObjectCallbackData&,
				cbdata, caller, cb );
    mDynamicCastGet(EM::EMObject*,emobject,caller);
    if ( !emobject ) return;

    switch( cbdata.event )
    {
	case EM::EMObjectCallbackData::Undef:
	    break;
	case EM::EMObjectCallbackData::PrefColorChange:
	{
	    displayMiniCtab();
	    break;
	}
	case EM::EMObjectCallbackData::NameChange:
	{
	    name_ = mToUiStringTodo(applMgr()->EMServer()->getName( emid_ ));
	    uiTreeItem::updateColumnText( uiODViewer2DMgr::cNameColumn() );
	    break;
	}
	default: break;
    }
}


void uiODVw2DFaultSSTreeItem::enableKnotsCB( CallBacker* )
{
    if ( fssview_ && viewer2D()->dataMgr()->selectedID() == fssview_->id() )
	fssview_->selected();
}


bool uiODVw2DFaultSSTreeItem::select()
{
    uitreeviewitem_->setSelected( true );

    if ( fssview_ )
    {
	viewer2D()->dataMgr()->setSelected( fssview_ );
	fssview_->selected();
    }

    return true;
}


#define mPropID		0
#define mSaveID		1
#define mSaveAsID	2
#define mRemoveAllID	3
#define mRemoveID	4

bool uiODVw2DFaultSSTreeItem::showSubMenu()
{
    uiEMPartServer* ems = applMgr()->EMServer();
    uiMenu mnu( getUiParent(), uiStrings::sAction() );

//    addAction( mnu, uiStrings::sProperties(), mPropID, "disppars", true );

    const bool haschanged = ems->isChanged( emid_ );
    addAction( mnu, uiStrings::sSave(), mSaveID, "save", haschanged );
    addAction( mnu, m3Dots(uiStrings::sSaveAs()), mSaveAsID, "saveas", true );

    uiMenu* removemenu = new uiMenu( uiStrings::sRemove(), "remove" );
    mnu.addMenu( removemenu );
    addAction( *removemenu, tr("From all 2D Viewers"), mRemoveAllID );
    addAction( *removemenu, tr("Only from this 2D Viewer"), mRemoveID );

    const int mnuid = mnu.exec();
    if ( mnuid == mPropID )
    {
    // ToDo
    }
    else if ( mnuid==mSaveID || mnuid==mSaveAsID )
    {
	if ( mnuid==mSaveID )
	    doSave();
	if ( mnuid== mSaveAsID )
	    doSaveAs();
    }
    else if ( mnuid==mRemoveAllID || mnuid==mRemoveID )
    {
	if ( !applMgr()->EMServer()->askUserToSave(emid_,true) )
	    return true;

	name_ = applMgr()->EMServer()->getName( emid_ );
	renameVisObj();
	bool doremove = !applMgr()->viewer2DMgr().isItemPresent( parent_ ) ||
			mnuid==mRemoveID;
	if ( mnuid == mRemoveAllID )
	{
	    applMgr()->viewer2DMgr().removeFaultSS2D( emid_ );
	    applMgr()->viewer2DMgr().removeFaultSS( emid_ );
	}

	if ( doremove )
	    parent_->removeChild( this );
    }

    return true;
}


void uiODVw2DFaultSSTreeItem::updateCS( const TrcKeyZSampling& cs, bool upd )
{
    if ( upd && fssview_ )
	fssview_->setTrcKeyZSampling( cs, upd );
}


void uiODVw2DFaultSSTreeItem::deSelCB( CallBacker* )
{
    //TODO handle on/off MOUSEEVENT
}


void uiODVw2DFaultSSTreeItem::checkCB( CallBacker* )
{
    if ( fssview_ )
	fssview_->enablePainting( isChecked() );
}


void uiODVw2DFaultSSTreeItem::emobjAbtToDelCB( CallBacker* cb )
{
    mCBCapsuleUnpack( const EM::ObjectID&, emid, cb );
    if ( emid != emid_ ) return;

    EM::EMObject* emobj = EM::EMM().getObject( emid );
    mDynamicCastGet(EM::FaultStickSet*,fss,emobj);
    if ( !fss ) return;

    parent_->removeChild( this );
}


uiTreeItem* uiODVw2DFaultSSTreeItemFactory::createForVis(
				const uiODViewer2D& vwr2d, int id ) const
{
    mDynamicCastGet(const VW2DFaultSS3D*,obj,vwr2d.dataMgr()->getObject(id));
    return obj ? new uiODVw2DFaultSSTreeItem(id,true) : 0;
}
