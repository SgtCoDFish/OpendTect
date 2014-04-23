/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        N. Hemstra
 Date:          January  2008
________________________________________________________________________

-*/

static const char* rcsID mUsedVar = "$Id$";


#include "uisemblanceattrib.h"
#include "semblanceattrib.h"

#include "attribdesc.h"
#include "attribparam.h"
#include "uiattribfactory.h"
#include "uiattrsel.h"
#include "uigeninput.h"
#include "uisteeringsel.h"
#include "uistepoutsel.h"
#include "od_helpids.h"

using namespace Attrib;

static const char* extstrs3d[] =
{
	"None",
	"Mirror 90 degrees",
	"Mirror 180 degrees",
	"Full block",
	0
};

static const char* extstrs2d[] =
{
	"None",
	"Mirror 180 degrees",
	"Full block",
	0
};

mInitAttribUI(uiSemblanceAttrib,Semblance,"Semblance",sKeyBasicGrp())


uiSemblanceAttrib::uiSemblanceAttrib( uiParent* p, bool is2d )
	: uiAttrDescEd(p,is2d, mODHelpKey(mSimilarityAttribHelpID) )

{
    inpfld = createInpFld( is2d );

    gatefld = new uiGenInput( this, gateLabel(),
			      FloatInpIntervalSpec().setName("Z start",0)
			      			    .setName("Z stop",1) );

    gatefld->attach( alignedBelow, inpfld );

    extfld = new uiGenInput( this, "Extension",
	    		     StringListInpSpec( is2d_ ? extstrs2d : extstrs3d));
    extfld->valuechanged.notify( mCB(this,uiSemblanceAttrib,extSel) );
    extfld->attach( alignedBelow, gatefld );
    
    uiStepOutSel::Setup setup( is2d );
    setup.seltxt( "Trace positions" ).allowneg( true );
    pos0fld = new uiStepOutSel( this, setup );
    pos0fld->setFieldNames( "Trc1 Inl", "Trc1 Crl" );
    pos0fld->attach( alignedBelow, extfld );
    setup.seltxt( "&" );
    pos1fld = new uiStepOutSel( this, setup );
    pos1fld->setFieldNames( "Trc2 Inl", "Trc2 Crl" );
    pos1fld->attach( rightOf, pos0fld );

    stepoutfld = new uiStepOutSel( this, is2d );
    stepoutfld->attach( alignedBelow, extfld );
    stepoutfld->setFieldNames( "Inl Stepout", "Crl Stepout" );

    steerfld = new uiSteeringSel( this, 0, is2d );
    steerfld->attach( alignedBelow, stepoutfld );

    setHAlignObj( pos0fld );

    extSel(0);
}


void uiSemblanceAttrib::extSel( CallBacker* )
{
    const bool isfullblock = FixedString(extfld->text()) == extstrs3d[3];
    pos0fld->display( !isfullblock );
    pos1fld->display( !isfullblock );
    stepoutfld->display( isfullblock );
}


bool uiSemblanceAttrib::setParameters( const Attrib::Desc& desc )
{
    if ( desc.attribName() != Semblance::attribName() )
	return false;

    mIfGetFloatInterval( Semblance::gateStr(), gate, gatefld->setValue(gate) )
    mIfGetBinID( Semblance::stepoutStr(), stepout, 
	         stepoutfld->setBinID(stepout) )
    mIfGetBinID( Semblance::pos0Str(), pos0, pos0fld->setBinID(pos0) )
    mIfGetBinID( Semblance::pos1Str(), pos1, pos1fld->setBinID(pos1) )
    mIfGetEnum( Semblance::extensionStr(), extension,
		extfld->setText(extstrs3d[extension]) )

    extSel(0);
    return true;
}


bool uiSemblanceAttrib::setInput( const Attrib::Desc& desc )
{
    putInp( inpfld, desc, 0 );
    putInp( steerfld, desc, 1 );
    return true;
}


bool uiSemblanceAttrib::getParameters( Attrib::Desc& desc )
{
    if ( desc.attribName() != Semblance::attribName() )
	return false;

    const bool isfullblock = FixedString(extfld->text()) == extstrs3d[3];
    if ( isfullblock )
    {	mSetBinID( Semblance::stepoutStr(), stepoutfld->getBinID() ); }
    else
    {
	mSetBinID( Semblance::pos0Str(), pos0fld->getBinID() );
	mSetBinID( Semblance::pos1Str(), pos1fld->getBinID() );
    }

    BufferStringSet strs( extstrs3d );
    mSetEnum( Semblance::extensionStr(), strs.indexOf(extfld->text()) );
    mSetFloatInterval( Semblance::gateStr(), gatefld->getFInterval() );
    mSetBool( Semblance::steeringStr(), steerfld->willSteer() );

    return true;
}


bool uiSemblanceAttrib::getInput( Attrib::Desc& desc )
{
    inpfld->processInput();
    fillInp( inpfld, desc, 0 );
    fillInp( steerfld, desc, 1 );
    return true;
}


void uiSemblanceAttrib::getEvalParams( TypeSet<EvalParam>& params ) const
{
    params += EvalParam( timegatestr(), Semblance::gateStr() );

    const bool isfullblock = FixedString(extfld->text()) == extstrs3d[3];
    if ( isfullblock )
	params += EvalParam( stepoutstr(), Semblance::stepoutStr() );
    else
	params += EvalParam( "Trace positions", Semblance::pos0Str(),
			     Semblance::pos1Str() );
}
