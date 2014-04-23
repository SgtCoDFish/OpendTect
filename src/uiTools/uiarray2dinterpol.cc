/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        K. Tingdahl
 Date:          March 2008
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";

#include "uiarray2dinterpol.h"

#include "array2dinterpolimpl.h"
#include "iopar.h"
#include "survinfo.h"
#include "uigeninput.h"
#include "uibutton.h"
#include "uimsg.h"
#include "od_helpids.h"

mImplFactory1Param(uiArray2DInterpol,uiParent*,uiArray2DInterpolSel::factory);


uiArray2DInterpolSel::uiArray2DInterpolSel( uiParent* p, bool filltype,
					    bool maxholesz,
					    bool withclassification,
       					    const Array2DInterpol* oldvals,
					    bool withstep )
    : uiDlgGroup( p, "Array2D Interpolation" )
    , result_( 0 )
    , filltypefld_( 0 )
    , stepfld_( 0 )
    , maxholeszfld_( 0 )
    , methodsel_( 0 )
    , isclassificationfld_( 0 )
{
    params_.allowNull( true );
    uiObject* prevfld = 0;
    uiObject* halignobj = 0;
    if ( filltype )
    {
	filltypefld_ = new uiGenInput( this, "Scope",
		StringListInpSpec( Array2DInterpol::FillTypeNames() ));
	if ( oldvals ) filltypefld_->setValue( (int) oldvals->getFillType() );
	prevfld = filltypefld_->attachObj();
    }

    if ( withstep )
    {
	PositionInpSpec::Setup setup;
	PositionInpSpec spec( setup );
	stepfld_ = new uiGenInput( this, "Inl/Crl Step", spec );
	stepfld_->setValue( BinID(SI().inlStep(),SI().crlStep()) );
	if ( filltype )
	    stepfld_->attach( alignedBelow, prevfld );
	prevfld = stepfld_->attachObj();
    }

    if ( maxholesz )
    {
	maxholeszfld_ = new uiGenInput( this, 0, FloatInpSpec() );
	maxholeszfld_->setWithCheck( true );
	if ( prevfld )
	    maxholeszfld_->attach( alignedBelow, prevfld );
	if ( oldvals )
	{
	    if ( mIsUdf(oldvals->getMaxHoleSize() ) )
		maxholeszfld_->setChecked( false );
	    else
	    {
		maxholeszfld_->setChecked( true );
		maxholeszfld_->setValue( oldvals->getMaxHoleSize() );
	    }
	}

	prevfld = maxholeszfld_->attachObj();
    }


    if ( withclassification )
    {
	isclassificationfld_ = new uiGenInput( this,
		"Values are classifications", BoolInpSpec( false ) );
	if ( prevfld )
	    isclassificationfld_->attach( alignedBelow, prevfld );
	if ( oldvals )
	    isclassificationfld_->setValue( oldvals->isClassification() );
	prevfld = isclassificationfld_->attachObj();
    }

    const BufferStringSet& methods = Array2DInterpol::factory().getNames();
    int methodidx;
    if ( methods.size()>1 )
    {
	methodsel_ = new uiGenInput( this, "Algorithm",
	    StringListInpSpec(Array2DInterpol::factory().getUserNames() ) );

	if ( prevfld )
	    methodsel_->attach( alignedBelow, prevfld );

	methodsel_->valuechanged.notify(
		mCB( this, uiArray2DInterpolSel, selChangeCB ) );

	methodidx = oldvals ? methods.indexOf( oldvals->factoryKeyword() ) : 0;
	if ( oldvals )
	    methodsel_->setValue( methodidx );

	prevfld = methodsel_->attachObj();
	halignobj = prevfld;
    }
    else
	methodidx = 0;

    for ( int idx=0; idx<methods.size(); idx++ )
    {
	uiArray2DInterpol* paramfld =
	    factory().create( methods[idx]->buf(), this, true );

	if ( paramfld )
	{
	    if ( oldvals && idx==methodidx)
		paramfld->setValuesFrom( *oldvals );

	    if ( prevfld )
		paramfld->attach( alignedBelow, prevfld );
	}

	params_ += paramfld;
    }

    if ( halignobj )
	setHAlignObj( halignobj );
    else
    {
	for ( int idx=0; idx<params_.size(); idx++ )
	{
	    if ( params_[idx] )
	    {
		setHAlignObj( params_[idx] );
		break;
	    }
	}
    }

    selChangeCB( 0 );
    setDistanceUnit( 0 );
}


uiArray2DInterpolSel::~uiArray2DInterpolSel()
{ delete result_; }


HelpKey uiArray2DInterpolSel::helpKey() const
{
    const int sel = methodsel_ ? methodsel_->getIntValue( 0 ) : 0;
    return params_[sel] ? params_[sel]->helpKey() : HelpKey::emptyHelpKey();
}


void uiArray2DInterpolSel::setDistanceUnit( const uiString& du )
{
    if ( maxholeszfld_ )
    {
	uiString res = tr("Keep holes larger than %1").arg( du );
	maxholeszfld_->setTitleText( res );
    }

    for ( int idx=0; idx<params_.size(); idx++ )
    {
	if ( params_[idx] )
	    params_[idx]->setDistanceUnit( du );
    }
}


uiParent* uiArray2DInterpolSel::getTopObject()
{
    if ( filltypefld_ ) return filltypefld_;
    if ( maxholeszfld_ ) return maxholeszfld_;
    return methodsel_;
}


void uiArray2DInterpolSel::selChangeCB( CallBacker* )
{
    const int sel = methodsel_ ? methodsel_->getIntValue( 0 ) : 0;
    for ( int idx=0; idx<params_.size(); idx++ )
    {
	if ( !params_[idx] )
	    continue;

	params_[idx]->display( idx==sel );
    }
}


void uiArray2DInterpolSel::fillPar( IOPar& iopar ) const
{
    if ( !result_ )
	return;

    iopar.set( sKey::Name(), methodsel_->text() );
    result_->fillPar( iopar );
}


bool uiArray2DInterpolSel::acceptOK()
{
    if ( maxholeszfld_ && maxholeszfld_->isChecked() &&
	 (mIsUdf( maxholeszfld_->getfValue() ) ||
	  maxholeszfld_->getfValue()<=0 ) )
    {
	uiMSG().error(
		tr("Maximum hole size not set or is less or equal to zero"));
	return false;
    }

    const BufferStringSet& methods = Array2DInterpol::factory().getNames();
    const int methodidx = methodsel_ ? methodsel_->getIntValue() : 0;
    
    if ( methodidx>=methods.size() )
    {
	pErrMsg("Invalid method selected");
	return false;
    }

    if ( result_ )
	delete result_;

    if ( !params_[methodidx] )
    {
	result_ = Array2DInterpol::factory().create(methods[methodidx]->buf());
	BufferString msg( result_->infoMsg() );
	if ( !msg.isEmpty() )
	{
	    uiMSG().message( msg );
	    return false;
	}
    }
    else
    {
	if ( !params_[methodidx]->acceptOK() )
	    return false;

	result_ = params_[methodidx]->getResult();
	if ( !result_ )
	    return false;
    }

    if ( isclassificationfld_ )
	result_->setClassification( isclassificationfld_->getBoolValue() );

    result_->setFillType( filltypefld_ 
	? (Array2DInterpol::FillType) filltypefld_->getIntValue()
	: Array2DInterpol::Full );

    result_->setMaxHoleSize( maxholeszfld_ && maxholeszfld_->isChecked() 
			     ? maxholeszfld_->getIntValue() : mUdf(float) );

    return true;
}


void uiArray2DInterpolSel::setStep( const BinID& steps )
{
    if ( stepfld_ )
	stepfld_->setValue( steps );
}


BinID uiArray2DInterpolSel::getStep() const
{
    return stepfld_ ? stepfld_->getBinID() : BinID::udf();
}


Array2DInterpol* uiArray2DInterpolSel::getResult()
{
    Array2DInterpol* res = result_;
    result_ = 0;
    return res;
}


uiArray2DInterpol::uiArray2DInterpol( uiParent* p, const uiString& caption )
    : uiDlgGroup( p, caption )
    , result_( 0 )
{
}


uiArray2DInterpol::~uiArray2DInterpol()
{ delete result_; }


Array2DInterpol* uiArray2DInterpol::getResult()
{
    Array2DInterpol* res = result_;
    result_ = 0;
    return res;
}


void uiInverseDistanceArray2DInterpol::initClass()
{
    uiArray2DInterpolSel::factory().addCreator( create,
	    InverseDistanceArray2DInterpol::sFactoryKeyword() );
}


uiArray2DInterpol* uiInverseDistanceArray2DInterpol::create( uiParent* p )
{ return new uiInverseDistanceArray2DInterpol( p ); }


uiInverseDistanceArray2DInterpol::uiInverseDistanceArray2DInterpol(uiParent* p)
    : uiArray2DInterpol( p, "Inverse distance" )
    , nrsteps_(mUdf(int))
    , cornersfirst_(false)
    , stepsz_(1)
{
    radiusfld_ = new  uiGenInput( this, 0, FloatInpSpec() );
    radiusfld_->setWithCheck( true );
    radiusfld_->setChecked( true );
    radiusfld_->checked.notify(
	    mCB(this,uiInverseDistanceArray2DInterpol,useRadiusCB));

    parbut_ = new uiPushButton( this, "&Parameters", 
		    mCB(this,uiInverseDistanceArray2DInterpol,doParamDlg),
		    false );
    parbut_->attach( rightOf, radiusfld_ );

    setHAlignObj( radiusfld_ );
    setDistanceUnit( 0 );
    useRadiusCB( 0 );
}


void uiInverseDistanceArray2DInterpol::useRadiusCB( CallBacker* )
{
    parbut_->display( radiusfld_->isChecked() );
}


void uiInverseDistanceArray2DInterpol::setValuesFrom( const Array2DInterpol& a )
{
    mDynamicCastGet(const InverseDistanceArray2DInterpol*, ptr, &a );
    if ( !ptr )
	return;

    if ( mIsUdf( ptr->getSearchRadius() ) )
	radiusfld_->setChecked( false );
    else
    {
	radiusfld_->setChecked( true );
	radiusfld_->setValue( ptr->getSearchRadius() );

	nrsteps_ = ptr->getNrSteps();
	cornersfirst_ = ptr->getCornersFirst();
	stepsz_ = ptr->getStepSize();
    }
}


void uiInverseDistanceArray2DInterpol::setDistanceUnit( const uiString& d )
{
    uiString res = tr("Search radius %1").arg( d );
    radiusfld_->setTitleText( res );


}


HelpKey uiInverseDistanceArray2DInterpol::helpKey() const
{ return mODHelpKey(mInverseDistanceArray2DInterpolHelpID); }


void uiTriangulationArray2DInterpol::initClass()
{
    uiArray2DInterpolSel::factory().addCreator( create,
	    TriangulationArray2DInterpol::sFactoryKeyword() );
}


uiArray2DInterpol* uiTriangulationArray2DInterpol::create( uiParent* p )
{ return new uiTriangulationArray2DInterpol( p ); }


uiTriangulationArray2DInterpol::uiTriangulationArray2DInterpol(uiParent* p)
    : uiArray2DInterpol( p, "Inverse distance" )
{
    useneighborfld_ = new uiCheckBox( this, "Use nearest neighbor" );
    useneighborfld_->setChecked( false );
    useneighborfld_->activated.notify(
	    mCB(this,uiTriangulationArray2DInterpol,intCB) );
    
    maxdistfld_ = new uiGenInput( this, 0, FloatInpSpec() );
    maxdistfld_->setWithCheck( true );
    maxdistfld_->attach( alignedBelow, useneighborfld_ );

    setHAlignObj( useneighborfld_ );
    setDistanceUnit( 0 );
    intCB( 0 );
}


void uiTriangulationArray2DInterpol::intCB( CallBacker* )
{
    maxdistfld_->display( !useneighborfld_->isChecked() );
}


void uiTriangulationArray2DInterpol::setDistanceUnit( const uiString& d )
{
    uiString res = tr("Max interpolate distance %1").arg( d );
    maxdistfld_->setTitleText( res );
}


Array2DInterpol* uiTriangulationArray2DInterpol::createResult() const
{ return new TriangulationArray2DInterpol; }


bool uiTriangulationArray2DInterpol::acceptOK()
{
    bool usemax = !useneighborfld_->isChecked() && maxdistfld_->isChecked();
    const float maxdist = maxdistfld_->getfValue();
    if ( usemax && !mIsUdf(maxdist) && maxdist<0 )
    {
	uiMSG().error( "Maximum distance must be > 0. " );
	return false;
    }

    if ( result_ )
    { 
	delete result_; 
	result_ = 0; 
    }
    
    mDynamicCastGet( TriangulationArray2DInterpol*, res, createResult() )
    res->doInterpolation( !useneighborfld_->isChecked() );
    if ( usemax )
    	res->setMaxDistance( maxdist );
    
    result_ = res;
    return true;
}


void uiExtensionArray2DInterpol::initClass()
{
    uiArray2DInterpolSel::factory().addCreator( create,
	    ExtensionArray2DInterpol::sFactoryKeyword() );
}


uiArray2DInterpol* uiExtensionArray2DInterpol::create( uiParent* p )
{ return new uiExtensionArray2DInterpol( p ); }


uiExtensionArray2DInterpol::uiExtensionArray2DInterpol(uiParent* p)
    : uiArray2DInterpol( p, "Extension" )
{
    nrstepsfld_ = new uiGenInput( this, "Number of steps", IntInpSpec(20) );
    setHAlignObj( nrstepsfld_ );
}


Array2DInterpol* uiExtensionArray2DInterpol::createResult() const
{ return new ExtensionArray2DInterpol; }


bool uiExtensionArray2DInterpol::acceptOK()
{
    if ( nrstepsfld_->getIntValue()<1 )
    {
	uiMSG().error( "Nr steps must be > 0." );	
	return false;
    }

    if ( result_ )
	{ delete result_; result_ = 0; }
    
    mDynamicCastGet( ExtensionArray2DInterpol*, res, createResult() )
    res->setNrSteps( nrstepsfld_->getIntValue() );

    result_ = res;    
    return true;
}


uiInvDistInterpolPars::uiInvDistInterpolPars( uiParent* p, bool cornersfirst,
					      int stepsz, int nrsteps )
    : uiDialog(p,Setup("Inverse distance - parameters",
		       "Inverse distance with search radius",
                       mODHelpKey(mInverseDistanceArray2DInterpolHelpID) ) )
{
    cornersfirstfld_ = new  uiGenInput( this, "Compute corners first",
					BoolInpSpec(cornersfirst) );

    stepsizefld_ = new uiGenInput( this, "Step size", IntInpSpec(stepsz) );
    stepsizefld_->attach( alignedBelow, cornersfirstfld_ );

    nrstepsfld_ = new uiGenInput( this, "[Nr steps]", IntInpSpec(nrsteps) );
    nrstepsfld_->attach( alignedBelow, stepsizefld_ );
}

bool uiInvDistInterpolPars::isCornersFirst() const
{ return cornersfirstfld_->getBoolValue(); }

int uiInvDistInterpolPars::stepSize() const
{ return stepsizefld_->getIntValue(); }

int uiInvDistInterpolPars::nrSteps() const
{ return nrstepsfld_->getIntValue(); }

bool uiInvDistInterpolPars::acceptOK( CallBacker* )
{
    const int stepsize = stepsizefld_->getIntValue();
    const int nrsteps = nrstepsfld_->getIntValue();

    if ( mIsUdf(stepsize) || stepsize<1 )
    {
	uiMSG().error( "Step size must set and > 0. In doubt, use 1." );
	return false;
    }
    if ( (!mIsUdf(nrsteps) && nrsteps<1 ) )
    {
	uiMSG().error( "Nr steps must be > 0. In doubt, leave empty." );
	return false;
    }

    return true;
}


void uiInverseDistanceArray2DInterpol::doParamDlg( CallBacker* )
{
    uiInvDistInterpolPars dlg( this, cornersfirst_, stepsz_, nrsteps_ );
    if ( !dlg.go() ) return;

    cornersfirst_ = dlg.isCornersFirst();
    stepsz_ = dlg.stepSize();
    nrsteps_ = dlg.nrSteps();
}


Array2DInterpol* uiInverseDistanceArray2DInterpol::createResult() const
{ return new InverseDistanceArray2DInterpol; }


bool uiInverseDistanceArray2DInterpol::acceptOK()
{
    if ( result_ )
	{ delete result_; result_ = 0; }

    const bool hasradius = radiusfld_->isChecked();
    const float radius = hasradius ? radiusfld_->getfValue(0) : mUdf(float);

    if ( hasradius && (mIsUdf(radius) || radius<=0) )
    {
	uiMSG().error(
		"Please enter a positive value for the search radius\n"
		"(or uncheck the field)" );
	return false;
    }

    mDynamicCastGet( InverseDistanceArray2DInterpol*, res, createResult() )
    res->setSearchRadius( radius );
    if ( hasradius )
    {
	res->setCornersFirst( cornersfirst_ );
	res->setStepSize( stepsz_ );
	res->setNrSteps( nrsteps_ );
    }

    result_ = res;
    return true;
}
