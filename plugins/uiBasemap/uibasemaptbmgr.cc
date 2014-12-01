/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Henrique Mageste
 Date:		November 2014
 RCS:		$Id$
________________________________________________________________________

-*/

static const char* rcsID mUsedVar = "$Id$";

#include "uibasemaptbmgr.h"

#include "uidialog.h"
#include "uigeninput.h"
#include "uigraphicsview.h"
#include "uilistbox.h"
#include "uimainwin.h"
#include "uimenu.h"
#include "uimsg.h"
#include "uisellinest.h"
#include "uisurvmap.h"
#include "uistrings.h"
#include "uitoolbar.h"

#include "survinfo.h"


uiBaseMapTBMgr::uiBaseMapTBMgr( uiMainWin& mw, uiSurveyMap& sm )
    : mainwin_(mw)
    , basemapview_(sm)
    , pickmode_(false)
{
    createviewTB();
    createitemTB();
    updateViewMode();
}


uiBaseMapTBMgr::~uiBaseMapTBMgr()
{
}


void uiBaseMapTBMgr::createitemTB()
{
    itemtoolbar_ = new uiToolBar( &mainwin_, "Basemap Items" );
    CallBack cb = mCB(this,uiBaseMapTBMgr,iconClickCB);
    const ObjectSet<uiBasemapItem>& itms = BMM().items();

    for ( int idx=0; idx<itms.size(); idx++ )
    {
	const uiBasemapItem* itm = itms[idx];

	uiString str( "Add " ); str.append( itm->factoryDisplayName() );
	uiAction* action = new uiAction( str, cb, itm->iconName() );
	itemtoolbar_->insertAction( action, itm->ID() );
    }

    removebut_ = new uiToolButton( itemtoolbar_, "trashcan",
				   tr("Remove selected items"),
				   mCB(this, uiBaseMapTBMgr, removeCB) );
    itemtoolbar_->addButton( removebut_ );
}


void uiBaseMapTBMgr::createviewTB()
{
    vwtoolbar_ = new uiToolBar( &mainwin_, "Viewer Tools", uiToolBar::Left );

    viewbut_ = new uiToolButton( vwtoolbar_, "altview", tr(""),
				 mCB(this,uiBaseMapTBMgr,viewCB) );
    vwtoolbar_->addButton( viewbut_ );

    vwtoolbar_->addObject(
	    basemapview_.view().getSaveImageButton(vwtoolbar_) );

    vworientationid_ = vwtoolbar_->addButton( "northarrow",
				tr("Display Orientation"),
				mCB(this,uiBaseMapTBMgr,vworientationCB),
				true );
    vwtoolbar_->turnOn( vworientationid_, true );

    vwmapscaleid_ = vwtoolbar_->addButton( "scale",
				tr("Display Map Scale"),
				mCB(this,uiBaseMapTBMgr,vwmapscaleCB),
				true );
    uiMenu* scalemnu = new uiMenu( tr("Map Scale Menu") );

    uiAction* item = new uiAction( uiStrings::sSettings(false),
				   mCB(this,uiBaseMapTBMgr,barSettingsCB),
				   "disppars" );
    scalemnu->insertItem( item, vwmapscaleid_ );
    vwtoolbar_->setButtonMenu( vwmapscaleid_, scalemnu );
    vwtoolbar_->turnOn( vwmapscaleid_, true );
}


void uiBaseMapTBMgr::updateViewMode()
{
    viewbut_->setIcon( pickmode_ ? "altpick" : "altview" );
    viewbut_->setToolTip( pickmode_ ? "Switch to view mode"
				    : "Switch to pick mode" );
    basemapview_.view().setDragMode(
	pickmode_ ? uiGraphicsViewBase::NoDrag
		  : uiGraphicsViewBase::ScrollHandDrag);
}



class uiBarSettingsDialog : public uiDialog
{ mODTextTranslationClass(uiBarSettingsDialog)
public:
uiBarSettingsDialog( uiParent* p, uiMapScaleObject& ms )
    : uiDialog(p,Setup(tr("Scale Properties"),mNoDlgTitle,mTODOHelpKey))
    , scalelenfld_(0)
    , msobj_(ms)
{
    setCtrlStyle( CloseOnly );

    scalelen_ = msobj_.getScaleLen();

    const double mv = SI().maxCoord(false).x - SI().minCoord(false).x;
    maxscalelen_ = 100 * mCast(int, mv/100);

    const BufferString lbl( "Scale Size ", SI().getXYUnitString() );
    scalelenfld_ = new uiGenInput( this, lbl, FloatInpSpec(scalelen_) );
    scalelenfld_->valuechanged.notify(
		mCB(this,uiBarSettingsDialog,scalebarChg) );

    uiSelLineStyle::Setup stu; stu.drawstyle( false );
    const LineStyle lst( msobj_.getScaleStyle() );
    scalestylefld_ = new uiSelLineStyle( this, lst, stu );
    scalestylefld_->changed.notify(
		mCB(this,uiBarSettingsDialog,scalebarChg) );
    scalestylefld_->attach( alignedBelow, scalelenfld_ );
}


void scalebarChg( CallBacker* )
{
    const float newlen = scalelenfld_->getfValue();
    if ( newlen<=0 || newlen>maxscalelen_ )
    {
	scalelenfld_->setValue( scalelen_ );
	const BufferString msg(
		"Please enter a value between 0 and ", maxscalelen_ );
	uiMSG().error( msg );
    }
    else
    {
	msobj_.setScaleLen( newlen );
	scalelen_ = newlen;
    }

    msobj_.setScaleStyle( scalestylefld_->getStyle() );
}

    float		scalelen_;
    float		maxscalelen_;

    uiGenInput*		scalelenfld_;
    uiSelLineStyle*	scalestylefld_;
    uiMapScaleObject&	msobj_;
};



void uiBaseMapTBMgr::barSettingsCB( CallBacker* )
{
    uiBarSettingsDialog dlg( &mainwin_, *basemapview_.getMapScale() );
    if( !dlg.go() ) return;
}


void uiBaseMapTBMgr::iconClickCB( CallBacker* cb )
{
    mDynamicCastGet(uiAction*,action,cb)
    if ( !action ) return;

    const int id = action->getID();
    BMM().add( id );
}


void uiBaseMapTBMgr::removeCB( CallBacker* )
{
    uiMSG().warning( tr("Not implemented yet") );
}


void uiBaseMapTBMgr::viewCB( CallBacker* )
{
    pickmode_ = !pickmode_;
    updateViewMode();
}


void uiBaseMapTBMgr::vwmapscaleCB( CallBacker* )
{
    const bool ison = vwtoolbar_->isOn( vwmapscaleid_ );
    basemapview_.getMapScale()->show( ison );
}


void uiBaseMapTBMgr::vworientationCB( CallBacker* )
{
    const bool ison = vwtoolbar_->isOn( vworientationid_ );
    basemapview_.getNorthArrow()->show( ison );
}