/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bert
 Date:		July 2014
________________________________________________________________________

-*/

#include "uiseiswriteopts.h"

#include "datachar.h"
#include "iopar.h"
#include "seiscbvs.h"
#include "seiscbvsps.h"
#include "seispsioprov.h"

#include "uibutton.h"
#include "uigeninput.h"

#define mCBVSVolTranslInstance mTranslTemplInstance(SeisTrc,CBVS)
#define mCBVSPS3DTranslInstance mTranslTemplInstance(SeisPS3D,CBVS)


uiCBVSVolOpts::uiCBVSVolOpts( uiParent* p )
    : uiIOObjTranslatorWriteOpts(p,mCBVSVolTranslInstance)
{
    stortypfld_ = new uiGenInput( this, tr("Storage"),
		StringListInpSpec(DataCharacteristics::UserTypeDef()) );
    stortypfld_->setValue( (int)DataCharacteristics::Auto );

    optimdirfld_ =
		new uiCheckBox(this,
			       tr("Optimize for Z-slice viewing"));
    optimdirfld_->attach( alignedBelow, stortypfld_ );

    setHAlignObj( stortypfld_ );
}


void uiCBVSVolOpts::use( const IOPar& iop )
{
    const char* res = iop.find( sKey::DataStorage() );
    if ( res && *res )
	stortypfld_->setValue( (int)(*res - '0') );

    res = iop.find( CBVSSeisTrcTranslator::sKeyOptDir() );
    if ( res && *res )
	optimdirfld_->setChecked( *res == 'H' );
}


bool uiCBVSVolOpts::fill( IOPar& iop ) const
{
    iop.set( sKey::DataStorage(), stortypfld_->text() );

    iop.update( CBVSSeisTrcTranslator::sKeyOptDir(),
			optimdirfld_->isChecked() ? "Horizontal" : "" );
    return true;
}


void uiCBVSVolOpts::initClass()
{
    factory().addCreator( create, mCBVSVolTranslInstance.getDisplayName() );
}


uiCBVSPS3DOpts::uiCBVSPS3DOpts( uiParent* p )
    : uiIOObjTranslatorWriteOpts(p,mCBVSPS3DTranslInstance)
{
    stortypfld_ = new uiGenInput( this, tr("Storage"),
		 StringListInpSpec(DataCharacteristics::UserTypeDef()) );
    stortypfld_->setValue( (int)DataCharacteristics::Auto );
    setHAlignObj( stortypfld_ );
}


void uiCBVSPS3DOpts::use( const IOPar& iop )
{
    const char* res = iop.find( sKey::DataStorage() );
    if ( res && *res )
	stortypfld_->setValue( (int)(*res - '0') );
}


bool uiCBVSPS3DOpts::fill( IOPar& iop ) const
{
    iop.set( sKey::DataStorage(), stortypfld_->text() );
    return true;
}


void uiCBVSPS3DOpts::initClass()
{
    factory().addCreator( create, mCBVSPS3DTranslInstance.getDisplayName() );
}
