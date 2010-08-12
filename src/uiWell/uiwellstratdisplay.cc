/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bruno
 Date:          Mar 2010
________________________________________________________________________

-*/
static const char* rcsID = "$Id: uiwellstratdisplay.cc,v 1.14 2010-08-12 09:36:01 cvsbruno Exp $";

#include "uiwellstratdisplay.h"

#include "stratunitrepos.h"
#include "uigraphicsscene.h"
#include "uistrattreewin.h"
#include "welld2tmodel.h"
#include "wellmarker.h"
#include "welldisp.h"

uiWellStratDisplay::uiWellStratDisplay( uiParent* p, bool nobg,
					const Well::Well2DDispData& dd)
    : uiAnnotDisplay(p,"")
    , dispdata_(dd)
    , uidatagather_(uiStratTreeToDispTransl(data_,StratTWin().mgr()))
{
    if ( nobg )
    {
	setNoSytemBackGroundAttribute();
	uisetBackgroundColor( Color( 255, 255, 255, 255 )  );
	scene().setBackGroundColor( Color( 255, 255, 255, 255 )  );
    }
    drawer_.setNewAxis( new uiAxisHandler(scene_,
					  uiAxisHandler::Setup(uiRect::Left)
					  .noborderspace(true)
					  .border(uiBorder(0))
					  .nogridline(true)), false );
    drawer_.setNewAxis( new uiAxisHandler(scene_,
	    				 uiAxisHandler::Setup(uiRect::Top)
					 .noborderspace(true)
					 .border(uiBorder(0))
					 .nogridline(true) ), true );
    drawer_.xAxis()->setBounds( StepInterval<float>( 0, 100, 10 ) );
   
    uidatagather_.newtreeRead.notify( mCB(this,uiWellStratDisplay,dataChanged));
    dataChanged(0);
}


void uiWellStratDisplay::gatherOrgUnits()
{
    deepErase( orgunits_ );
    for ( int colidx=0; colidx<nrCols(); colidx++ )
    {
	for ( int idx=0; idx<nrUnits( colidx ); idx++ )
	{
	    AnnotData::Column& col = *getColumn( colidx );
	    if ( col.isaux_ ) continue;
	    const AnnotData::Unit* un = getUnit( idx, colidx );
	    AnnotData::Unit* newun = 
		    new AnnotData::Unit( un->name_, un->zpos_, un->zposbot_ );
	    newun->id_ = un->id_;
	    orgunits_ += newun;
	}
    }
}


void uiWellStratDisplay::dataChanged( CallBacker* )
{
    gatherOrgUnits();
    for ( int colidx=0; colidx<nrCols(); colidx++ )
    {
	AnnotData::Column& col = *getColumn( colidx );
	col.isdisplayed_ = false;
	if ( col.isaux_ ) continue;
	for ( int idx=0; idx<nrUnits( colidx ); idx++ )
	{
	    AnnotData::Unit* unit = getUnit( idx, colidx );
	    if ( unit )
	    {
		unit->draw_ = false;
		setUnitTopPos( *unit );
		setUnitBotPos( *unit );
		if ( unit->draw_ )
		    col.isdisplayed_ = true;
	    }
	}
    }
    deepErase( orgunits_ );
    setZRange( Interval<float>( (dispData().zrg_.stop/1000), 
				(dispData().zrg_.start )/1000) );
    drawer_.draw();
}


void uiWellStratDisplay::setUnitTopPos( AnnotData::Unit& unit )
{
    float& toppos = unit.zpos_; 
    const Well::Marker* mrk = 0;
    toppos = getPosFromMarkers( unit );
    unit.draw_ = !mIsUdf( toppos );
}


void uiWellStratDisplay::setUnitBotPos( AnnotData::Unit& unit )
{
    if ( !unit.draw_ ) return;
    float& botpos = unit.zposbot_;
    const AnnotData::Unit* nextunit = getNextTimeUnit( botpos );
    if ( nextunit ) 
    {
	botpos = getPosFromMarkers( *nextunit );
    }
    else if ( unit.zposbot_ == uidatagather_.botzpos_ )
    {
	botpos = getPosMarkerLvlMatch( uidatagather_.botlvlid_ );
    }
    unit.draw_ = !mIsUdf( botpos );
}


const AnnotData::Unit* uiWellStratDisplay::getNextTimeUnit( float pos ) const
{
    for ( int idx=0; idx<orgunits_.size(); idx++ )
    {
	const AnnotData::Unit* unit = orgunits_[idx];
	if ( unit && unit->zpos_ == pos )
	    return unit;
    }
    return 0;
}


float uiWellStratDisplay::getPosFromMarkers( const AnnotData::Unit& unit ) const
{
    float pos = mUdf(float);
    if ( !dispdata_.markers_ ) return pos;
    const Strat::UnitRef* uref = Strat::RT().getByID( unit.id_ );
    if ( uref )
	pos = getPosMarkerLvlMatch( uref->props().lvlid_ );
    return pos;
}


float uiWellStratDisplay::getPosMarkerLvlMatch( int lvlid ) const
{
    float pos = mUdf( float );
    const Well::Marker* mrk = 0;
    for ( int idx=0; idx<dispdata_.markers_->size(); idx++ )
    {
	const Well::Marker* curmrk = (*dispdata_.markers_)[idx];
	if ( curmrk && curmrk->levelID() >=0 )
	{
	    if ( lvlid == curmrk->levelID() )
		mrk = curmrk;
	}
    }
    if ( mrk ) 
    {
	pos = mrk->dah();
	if ( dispdata_.zistime_ && dispdata_.d2tm_ ) 
	    pos = dispdata_.d2tm_->getTime( pos ); 
    }
    return pos;
}


