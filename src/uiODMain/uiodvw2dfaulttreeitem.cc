/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Umesh Sinha
 Date:		Mar 2008
 RCS:		$Id$
________________________________________________________________________

-*/

#include "uiodvw2dfaulttreeitem.h"

#include "uiempartserv.h"
#include "uiflatviewstdcontrol.h"
#include "uimenu.h"
#include "uiodapplmgr.h"
#include "uiodviewer2d.h"
#include "uiodviewer2dmgr.h"
#include "uitreeview.h"
#include "uistrings.h"

#include "emfault3d.h"
#include "emmanager.h"
#include "emobject.h"
#include "ioman.h"
#include "ioobj.h"
#include "randcolor.h"

#include "view2dfault.h"
#include "view2ddataman.h"


uiODVw2DFaultParentTreeItem::uiODVw2DFaultParentTreeItem()
    : uiODVw2DTreeItem( "Fault" )
{
}


uiODVw2DFaultParentTreeItem::~uiODVw2DFaultParentTreeItem()
{
}


bool uiODVw2DFaultParentTreeItem::showSubMenu()
{
    uiMenu mnu( getUiParent(), uiStrings::sAction() );
    mnu.insertItem( new uiAction(uiStrings::sNew(true)), 0 );
    mnu.insertItem( new uiAction(uiStrings::sLoad(false)), 1 );
    insertStdSubMenu( mnu );
    return handleSubMenu( mnu.exec() );
}


bool uiODVw2DFaultParentTreeItem::handleSubMenu( int mnuid )
{
    handleStdSubMenu( mnuid );

    if ( mnuid == 0 )
    {
	RefMan<EM::EMObject> emo =
			EM::EMM().createTempObject( EM::Fault3D::typeStr() );
	if ( !emo )
	    return false;

	emo->setPreferredColor( getRandomColor(false) );
	emo->setNewName();
	emo->setFullyLoaded( true );
	uiODVw2DFaultTreeItem* treeitem = new uiODVw2DFaultTreeItem(emo->id());
	addChld( treeitem, false, false );
	viewer2D()->viewControl()->setEditMode( true );
	treeitem->select();
    }
    else if ( mnuid == 1 )
    {
	ObjectSet<EM::EMObject> objs;
	applMgr()->EMServer()->selectFaults( objs, false );

	for ( int idx=0; idx<objs.size(); idx++ )
	    addChld( new uiODVw2DFaultTreeItem(objs[idx]->id()),false,false);

	deepUnRef( objs );
    }

    return true;
}


bool uiODVw2DFaultParentTreeItem::init()
{
    return true;
}


void uiODVw2DFaultParentTreeItem::tempObjAddedCB( CallBacker* cb )
{
    mCBCapsuleUnpack( const EM::ObjectID&, emid, cb );

    EM::EMObject* emobj = EM::EMM().getObject( emid );
    mDynamicCastGet(EM::Fault3D*,f3d,emobj);
    if ( !f3d ) return;

    if ( findChild(applMgr()->EMServer()->getName(emid)) )
	return;

    addChld( new uiODVw2DFaultTreeItem(emid), false, false );
}


uiODVw2DFaultTreeItem::uiODVw2DFaultTreeItem( const EM::ObjectID& emid )
    : uiODVw2DTreeItem(0)
    , emid_(emid)
    , faultview_(0)
{}


uiODVw2DFaultTreeItem::uiODVw2DFaultTreeItem( int id, bool )
    : uiODVw2DTreeItem(0)
    , emid_(-1)
    , faultview_(0)
{
    displayid_ = id;
}


uiODVw2DFaultTreeItem::~uiODVw2DFaultTreeItem()
{
    detachAllNotifiers();
    viewer2D()->dataMgr()->removeObject( faultview_ );
}


bool uiODVw2DFaultTreeItem::init()
{
    EM::EMObject* emobj = 0;
    if ( displayid_ < 0 )
    {
	emobj = EM::EMM().getObject( emid_ );
	if ( !emobj ) return false;

	faultview_ = VW2DFault::create( emid_, viewer2D()->viewwin(),
				   viewer2D()->dataEditor() );
    }
    else
    {
	mDynamicCastGet(VW2DFault*,hd,
			viewer2D()->dataMgr()->getObject(displayid_))
	if ( !hd )
	    return false;
	emid_ = hd->emID();
	emobj = EM::EMM().getObject( emid_ );
	if ( !emobj ) return false;

	faultview_ = hd;
    }

    emobj->change.notify( mCB(this,uiODVw2DFaultTreeItem,emobjChangeCB) );
    displayMiniCtab();

    name_ = applMgr()->EMServer()->getName( emid_ );
    uitreeviewitem_->setCheckable(true);
    uitreeviewitem_->setChecked( true );
    checkStatusChange()->notify( mCB(this,uiODVw2DFaultTreeItem,checkCB) );

    faultview_->draw();

    if ( displayid_ < 0 )
	viewer2D()->dataMgr()->addObject( faultview_ );

    mAttachCB( viewer2D()->viewControl()->editPushed(),
	       uiODVw2DFaultTreeItem::enableKnotsCB );

    NotifierAccess* deselnotify = faultview_->deSelection();
    if ( deselnotify )
	deselnotify->notify( mCB(this,uiODVw2DFaultTreeItem,deSelCB) );

    return true;
}


void uiODVw2DFaultTreeItem::displayMiniCtab()
{
    EM::EMObject* emobj = EM::EMM().getObject( emid_ );
    if ( !emobj ) return;

    uiTreeItem::updateColumnText( uiODViewer2DMgr::cColorColumn() );
    uitreeviewitem_->setPixmap( uiODViewer2DMgr::cColorColumn(),
				emobj->preferredColor() );
}


void uiODVw2DFaultTreeItem::emobjChangeCB( CallBacker* cb )
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
	default: break;
    }
}


void uiODVw2DFaultTreeItem::enableKnotsCB( CallBacker* )
{
    if ( viewer2D()->dataMgr()->selectedID() == faultview_->id() )
	faultview_->selected();
}


bool uiODVw2DFaultTreeItem::select()
{
    if ( !uitreeviewitem_->isSelected() )
	return false;

    viewer2D()->dataMgr()->setSelected( faultview_ );
    faultview_->selected();
    return true;
}


bool uiODVw2DFaultTreeItem::showSubMenu()
{
    uiMenu mnu( getUiParent(), uiStrings::sAction() );
    uiAction* savemnu = new uiAction(uiStrings::sSave(false));
    mnu.insertItem( savemnu, 0 );
    savemnu->setEnabled( applMgr()->EMServer()->isChanged(emid_) );
    mnu.insertItem( new uiAction( uiStrings::sSaveAs(true) ), 1 );
    mnu.insertItem( new uiAction(uiStrings::sRemove(true)), 2 );

    const int mnuid = mnu.exec();
    if ( mnuid == 0 || mnuid == 1 )
    {
	bool savewithname = (mnuid == 1) ||
			    (EM::EMM().getMultiID( emid_ ).isEmpty());
	if ( !savewithname )
	{
	    PtrMan<IOObj> ioobj = IOM().get( EM::EMM().getMultiID(emid_) );
	    savewithname = !ioobj;
	}

	applMgr()->EMServer()->storeObject( emid_, savewithname );
	name_ = applMgr()->EMServer()->getName( emid_ );
	uiTreeItem::updateColumnText( uiODViewer2DMgr::cNameColumn() );
    }
    else if ( mnuid == 2 )
	parent_->removeChild( this );

    return true;
}


void uiODVw2DFaultTreeItem::updateCS( const TrcKeyZSampling& cs, bool upd )
{
    if ( upd )
	faultview_->draw();
}


void uiODVw2DFaultTreeItem::deSelCB( CallBacker* )
{
    //TODO handle on/off MOUSEEVENT
}


void uiODVw2DFaultTreeItem::checkCB( CallBacker* )
{
    faultview_->enablePainting( isChecked() );
}


void uiODVw2DFaultTreeItem::emobjAbtToDelCB( CallBacker* cb )
{
    mCBCapsuleUnpack( const EM::ObjectID&, emid, cb );
    if ( emid != emid_ ) return;

    EM::EMObject* emobj = EM::EMM().getObject( emid );
    mDynamicCastGet(EM::Fault3D*,f3d,emobj);
    if ( !f3d ) return;

    parent_->removeChild( this );
}


uiTreeItem* uiODVw2DFaultTreeItemFactory::createForVis(
				    const uiODViewer2D& vwr2d, int id ) const
{
    mDynamicCastGet(const VW2DFault*,obj,vwr2d.dataMgr()->getObject(id));
    return obj ? new uiODVw2DFaultTreeItem(id,true) : 0;
}

