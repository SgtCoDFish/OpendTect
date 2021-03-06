/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Nanne Hemstra
 Date:		April 2012
________________________________________________________________________

-*/



#include "uiodannottreeitem.h"

#include "ioman.h"
#include "ioobj.h"
#include "picksetmgr.h"
#include "trigonometry.h"
#include "visarrowdisplay.h"
#include "uiarrowdlg.h"
#include "uivispartserv.h"

const char* ArrowSubItem::parentType() const
{ return typeid(ArrowParentItem).name(); }


ArrowSubItem::ArrowSubItem( Pick::Set& pck, int displayid )
    : uiODAnnotSubItem( pck, displayid )
    , propmnuitem_( m3Dots(uiStrings::sProperties()) )
    , arrowtype_( 2 )
{
    defscale_ = mCast(float,set_->disp_.mkstyle_.size_);
    propmnuitem_.iconfnm = "disppars";
}


bool ArrowSubItem::init()
{
    if (  displayid_==-1 )
    {
	visSurvey::ArrowDisplay* ad = visSurvey::ArrowDisplay::create();
	visserv_->addObject( ad, sceneID(), true );
	displayid_ = ad->id();
	ad->setName( name_ );
    }

    mDynamicCastGet(visSurvey::ArrowDisplay*,ad,visserv_->getObject(displayid_))
    if ( !ad ) return false;

    Pick::SetMgr& mgr = Pick::SetMgr::getMgr( managerName() );
    const int setidx = mgr.indexOf( *set_ );
    PtrMan<IOObj> ioobj = IOM().get( mgr.id(setidx) );
    if ( !ioobj ) return false;

    if ( !ioobj->pars().get(sKeyArrowType(),arrowtype_) )
	set_->pars_.get( sKeyArrowType(), arrowtype_ );
    ad->setType( (visSurvey::ArrowDisplay::Type)arrowtype_ );

    int linewidth = 2;
    if ( !ioobj->pars().get(sKeyLineWidth(),linewidth) )
	set_->pars_.get( sKeyLineWidth(), linewidth );
    ad->setLineWidth( linewidth );

    //Read Old format orientation
    for ( int idx=set_->size()-1; idx>=0; idx-- )
    {
	Pick::Location& ploc( (*set_)[idx] );
	BufferString orientation;
	if ( ploc.getKeyedText("O", orientation ) )
	{
	    Sphere dir = ploc.dir();
	    if ( orientation[0] == '2' )
	    {
		dir.phi = (float) (-M_PI_2-dir.phi);
		dir.theta = M_PI_2;
	    }
	    else
	    {
		dir.phi = (float) (M_PI_2-dir.phi);
		dir.theta -= M_PI_2;
	    }
	    ploc.setDir( dir );
	}
	ploc.setText( 0 );
    }

    return uiODAnnotSubItem::init();
}


void ArrowSubItem::fillStoragePar( IOPar& par ) const
{
    uiODAnnotSubItem::fillStoragePar( par );
    mDynamicCastGet(visSurvey::ArrowDisplay*,ad,visserv_->getObject(displayid_))
    par.set( sKeyArrowType(), (int) ad->getType() );
    par.set( sKeyLineWidth(), ad->getLineWidth() );
}


void ArrowSubItem::createMenu( MenuHandler* menu, bool istb )
{
    uiODAnnotSubItem::createMenu( menu, istb );
    if ( !menu || menu->menuID()!=displayID() )
	return;

    mAddMenuOrTBItem(istb,menu,menu,&propmnuitem_,true,false );
}


void ArrowSubItem::handleMenuCB( CallBacker* cb )
{
    uiODAnnotSubItem::handleMenuCB( cb );
    mCBCapsuleUnpackWithCaller(int,mnuid,caller,cb);
    mDynamicCastGet(MenuHandler*,menu,caller);
    if ( menu->isHandled() || menu->menuID()!=displayID() )
	return;

    if ( mnuid==propmnuitem_.id )
    {
	menu->setIsHandled(true);

	uiArrowDialog dlg( getUiParent() );
	dlg.setColor( set_->disp_.mkstyle_.color_ );
	dlg.setArrowType( arrowtype_ );
	mDynamicCastGet(visSurvey::ArrowDisplay*,
			ad,visserv_->getObject(displayid_));
	dlg.setLineWidth( ad->getLineWidth() );
	dlg.propertyChange.notify( mCB(this,ArrowSubItem,propertyChange) );
	dlg.setScale( mCast(float,(set_->disp_.mkstyle_.size_/(defscale_))) );
	dlg.go();
	if ( set_->disp_.mkstyle_.color_!=dlg.getColor() )
	{
	    set_->disp_.mkstyle_.color_ = dlg.getColor();
	    Pick::SetMgr& mgr = Pick::SetMgr::getMgr( managerName() );
	    mgr.reportDispChange( this, *set_ );
	}

	arrowtype_ = dlg.getArrowType();
	updateColumnText( 1 );
    }
}


void ArrowSubItem::propertyChange( CallBacker* cb )
{
    mDynamicCastGet(uiArrowDialog*,dlg,cb)
    if ( !dlg ) return;

    const int arrowtype = dlg->getArrowType();
    setScale( defscale_*dlg->getScale() );
    setColor( dlg->getColor() );

    mDynamicCastGet(visSurvey::ArrowDisplay*,
	ad,visserv_->getObject(displayid_));
    ad->setType( (visSurvey::ArrowDisplay::Type) arrowtype );
    ad->setLineWidth( dlg->getLineWidth() );

    Pick::SetMgr& mgr = Pick::SetMgr::getMgr( managerName() );
    const int setidx = mgr.indexOf( *set_ );
    mgr.setUnChanged( setidx, false );
}
