/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Nanne Hemstra
 Date:		January 2008
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";


#include "uigraphicscoltab.h"

#include "uigraphicsitemimpl.h"
#include "uipixmap.h"

#include "coltabmapper.h"
#include "coltabsequence.h"

uiColTabItem::uiColTabItem( const uiColTabItem::Setup& su )
    : uiGraphicsItem()
    , setup_(su)
{
    ctseqitm_ = new uiPixmapItem();
    borderitm_ = new uiRectItem();
    minvalitm_ = new uiAdvancedTextItem( toUiString("0") );
    maxvalitm_ = new uiAdvancedTextItem( toUiString("1") );
    addChild( borderitm_ );
    addChild( ctseqitm_ );
    addChild( minvalitm_ );
    addChild( maxvalitm_ );

    setColTabSequence( ColTab::Sequence("") );

    uiRect boundrec = ctseqitm_->boundingRect();
    ctseqitm_->setPos( 0.f, 0.f );
    borderitm_->setRect( -1, -1, boundrec.width()+1, boundrec.height()+1 );

    ajustLabel();
}


uiColTabItem::~uiColTabItem()
{
    removeAll( true );
}


void uiColTabItem::ajustLabel()
{
    const uiRect rect = borderitm_->boundingRect();
    Alignment al;

    if ( setup_.hor_ )
    {
	const int starty =
	    setup_.startal_.vPos() == Alignment::VCenter? rect.centre().y
	 : (setup_.startal_.vPos() == Alignment::Top	? rect.top()
							: rect.bottom());

	al = Alignment( setup_.startal_.hPos(),
			Alignment::opposite(setup_.startal_.vPos()) );
	minvalitm_->setAlignment( al );
	minvalitm_->setPos( mCast(float,rect.left()), mCast(float,starty) );

	const int stopy =
	    setup_.stopal_.vPos() == Alignment::VCenter ? rect.centre().y
	 : (setup_.stopal_.vPos() == Alignment::Top	? rect.top()
							: rect.bottom());
	al = Alignment( setup_.stopal_.hPos(),
			Alignment::opposite(setup_.stopal_.vPos()) );
	maxvalitm_->setAlignment( al );
	maxvalitm_->setPos( mCast(float,rect.right()), mCast(float,stopy) );
    }
    else
    {
	const int startx =
	    setup_.startal_.hPos() == Alignment::HCenter? rect.centre().x
	 : (setup_.startal_.hPos() == Alignment::Left	? rect.left()
							: rect.right());
	al = Alignment( Alignment::opposite(setup_.startal_.hPos()),
			setup_.startal_.vPos() );
	minvalitm_->setAlignment( al );
	minvalitm_->setPos( mCast(float,startx), mCast(float,rect.top()) );

	const int stopx =
	    setup_.stopal_.hPos() == Alignment::HCenter ? rect.centre().x
	 : (setup_.stopal_.hPos() == Alignment::Left	? rect.left()
							: rect.right());
	al = Alignment( Alignment::opposite(setup_.stopal_.hPos()),
			setup_.stopal_.vPos() );
	maxvalitm_->setAlignment( al );
	maxvalitm_->setPos( mCast(float,stopx), mCast(float,rect.bottom()) );
    }
}


void uiColTabItem::setColTab( const char* nm )
{
    ColTab::Sequence seq( nm );
    setColTabSequence( seq );
}


void uiColTabItem::setColTabSequence( const ColTab::Sequence& ctseq )
{
    ctseq_ = ctseq;
    uiPixmap pm( setup_.sz_.hNrPics(), setup_.sz_.vNrPics() );
    pm.fill( ctseq_, setup_.hor_ );
    ctseqitm_->setPixmap( pm );
}


void uiColTabItem::setColTabMapperSetup( const ColTab::MapperSetup& ms )
{
    Interval<float> rg = ms.range_;
    minvalitm_->setPlainText( toUiString(rg.start) );
    maxvalitm_->setPlainText( toUiString(rg.stop) );
}
