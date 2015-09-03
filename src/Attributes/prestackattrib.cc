/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : B.Bril & H.Huck
 * DATE     : Jan 2008
-*/

static const char* rcsID mUsedVar = "$Id$";

#include "prestackattrib.h"

#include "attribdataholder.h"
#include "attribdesc.h"
#include "attribfactory.h"
#include "attribparam.h"
#include "datapackbase.h"
#include "posinfo.h"
#include "prestackanglecomputer.h"
#include "prestackprocessortransl.h"
#include "prestackprocessor.h"
#include "prestackgather.h"
#include "prestackprop.h"
#include "seispsioprov.h"
#include "seispsread.h"
#include "survinfo.h"
#include "raytrace1d.h"
#include "windowfunction.h"

#include "ioman.h"
#include "ioobj.h"

namespace Attrib
{
DefineEnumNames(PSAttrib,GatherType,0,"Gather type")
{
    "Offset",
    "Angle",
    0
};

DefineEnumNames(PSAttrib,XaxisUnit,0,"X-Axis unit")
{
    "in Degrees",
    "in Radians",
    0
};


mAttrDefCreateInstance(PSAttrib)

void PSAttrib::initClass()
{
    mAttrStartInitClassWithUpdate

    desc->addParam( new SeisStorageRefParam("id") );

#define mDefEnumPar(var,typ,defval) \
    epar = new EnumParam( var##Str() ); \
    epar->addEnums( typ##Names() ); \
    epar->setDefaultValue( defval ); \
    desc->addParam( epar )

    EnumParam*
    mDefEnumPar(calctype,PreStack::PropCalc::CalcType,0);
    mDefEnumPar(stattype,Stats::Type,Stats::Average);
    mDefEnumPar(lsqtype,PreStack::PropCalc::LSQType,0);
    mDefEnumPar(valaxis,PreStack::PropCalc::AxisType,0);
    mDefEnumPar(offsaxis,PreStack::PropCalc::AxisType,0);
    mDefEnumPar(gathertype,PSAttrib::GatherType,0);
    mDefEnumPar(xaxisunit,PSAttrib::XaxisUnit,0);

    IntParam* ipar = new IntParam( componentStr(), 0 , false );
    ipar->setLimits( Interval<int>(0,mUdf(int)) );
    desc->addParam( ipar );
    ipar = ipar->clone(); ipar->setKey( apertureStr() );
    desc->addParam( ipar );

    desc->addParam( new FloatParam( offStartStr(), 0, false ) );
    desc->addParam( new FloatParam( offStopStr(), mUdf(float), false ) );

    desc->addParam( new StringParam( preProcessStr(), "", false ) );

    desc->addParam( new StringParam( velocityIDStr(), "", false ) );

    desc->addParam( new IntParam( angleStartStr(), 0, false ) );
    desc->addParam( new IntParam( angleStopStr(), mUdf(int), false ) );
    desc->addParam( new IntParam(angleDPIDStr(),-1,true) );

    EnumParam* smoothtype = new EnumParam(
				PreStack::AngleComputer::sKeySmoothType() );
    smoothtype->addEnums( PreStack::AngleComputer::smoothingTypeNames() );
    smoothtype->setDefaultValue( PreStack::AngleComputer::FFTFilter );
    desc->addParam( smoothtype );

    desc->addParam( new StringParam( PreStack::AngleComputer::sKeyWinFunc(), "",
				     false ) );
    desc->addParam( new FloatParam( PreStack::AngleComputer::sKeyWinParam(),
				    mUdf(float), false ) );
    desc->addParam( new FloatParam( PreStack::AngleComputer::sKeyWinLen(),
				    mUdf(float), false ) );
    desc->addParam( new FloatParam( PreStack::AngleComputer::sKeyFreqF3(),
				    mUdf(float), false ) );
    desc->addParam( new FloatParam( PreStack::AngleComputer::sKeyFreqF4(),
				    mUdf(float), false ) );
    desc->addParam( new BoolParam( useangleStr(), true, false ) );
    desc->addParam( new StringParam( rayTracerParamStr(), "", false ) );

    desc->addOutputDataType( Seis::UnknowData );

    desc->setLocality( Desc::SingleTrace );
    desc->setUsesTrcPos( true );
    desc->setPS( true );
    mAttrEndInitClass
}


void PSAttrib::updateDesc( Desc& desc )
{
    const MultiID procid = desc.getValParam(preProcessStr())->getStringValue();
    const bool dopreproc = !procid.isEmpty() && !procid.isUdf();
    desc.setParamEnabled( preProcessStr(), dopreproc );

    const int calctype = desc.getValParam( calctypeStr() )->getIntValue();
    const bool dostats = calctype == PreStack::PropCalc::Stats;
    desc.setParamEnabled( stattypeStr(), dostats );
    desc.setParamEnabled( lsqtypeStr(), !dostats );
    desc.setParamEnabled( offsaxisStr(), !dostats );

    const bool useangle = desc.getValParam(useangleStr())->getBoolValue();
    desc.setParamEnabled( gathertypeStr(), !useangle );
    desc.setParamEnabled( xaxisunitStr(), !useangle );
    desc.setParamEnabled( offStartStr(), !useangle );
    desc.setParamEnabled( offStopStr(), !useangle );

    desc.setParamEnabled( velocityIDStr(), useangle );
    desc.setParamEnabled( angleStartStr(), useangle );
    desc.setParamEnabled( angleStopStr(), useangle );
    desc.setParamEnabled( rayTracerParamStr(), useangle );
    desc.setParamEnabled( PreStack::AngleComputer::sKeySmoothType(), useangle );

    const int smoothtype = desc.getValParam(
		     PreStack::AngleComputer::sKeySmoothType())->getIntValue();
    const bool fftfilter = smoothtype == PreStack::AngleComputer::FFTFilter;
    const bool movingavg = smoothtype == PreStack::AngleComputer::MovingAverage;
    desc.setParamEnabled( PreStack::AngleComputer::sKeyFreqF3(),
			  useangle && fftfilter );
    desc.setParamEnabled( PreStack::AngleComputer::sKeyFreqF4(),
			  useangle && fftfilter );
    desc.setParamEnabled( PreStack::AngleComputer::sKeyWinFunc(),
			  useangle && movingavg );
    desc.setParamEnabled( PreStack::AngleComputer::sKeyWinParam(),
			  useangle && movingavg );
    desc.setParamEnabled( PreStack::AngleComputer::sKeyWinLen(),
			  useangle && movingavg );

}


PSAttrib::PSAttrib( Desc& ds )
    : Provider(ds)
    , psioobj_(0)
    , psrdr_(0)
    , component_(0)
    , preprocessor_(0)
    , propcalc_(0)
    , anglecomp_(0)
    , anglegsdpid_(-1)
{
    if ( !isOK() ) return;

    const char* res;
    mGetString(res,"id")
    psid_ = res;

    BufferString preprocessstr;
    mGetString( preprocessstr, preProcessStr() );
    preprocid_ = preprocessstr;
    PtrMan<IOObj> preprociopar = IOM().get( preprocid_ );
    if ( preprociopar )
    {
	preprocessor_ = new PreStack::ProcessManager;
	uiString errmsg;
	if ( !PreStackProcTranslator::retrieve( *preprocessor_,preprociopar,
					       errmsg ) )
	{
	    errmsg_ = errmsg;
	    delete preprocessor_;
	    preprocessor_ = 0;
	}
    }
    else
	preprocid_.setEmpty();

    mGetInt( component_, componentStr() );
    mGetInt( setup_.aperture_, apertureStr() );

#define mGetSetupEnumPar(var,typ) \
    int tmp_##var = (int)setup_.var##_; \
    mGetEnum(tmp_##var,var##Str()); \
    setup_.var##_ = (typ)tmp_##var

    mGetSetupEnumPar(calctype,PreStack::PropCalc::CalcType);
    if ( setup_.calctype_ == PreStack::PropCalc::Stats )
    {
	mGetSetupEnumPar(stattype,Stats::Type);
    }
    else
    {
	mGetSetupEnumPar(lsqtype,PreStack::PropCalc::LSQType);
	mGetSetupEnumPar(offsaxis,PreStack::PropCalc::AxisType);
    }

    mGetSetupEnumPar(valaxis,PreStack::PropCalc::AxisType);

    bool useangle = setup_.useangle_;
    mGetBool( useangle, useangleStr() );
    setup_.useangle_ = useangle;
    mGetInt(anglegsdpid_,angleDPIDStr());
    if ( setup_.useangle_ && anglegsdpid_<0 )
    {
	mGetString( velocityid_, velocityIDStr() );
	if ( !velocityid_.isEmpty() && !velocityid_.isUdf() )
	{
	    PreStack::VelocityBasedAngleComputer* velangcomp =
		new PreStack::VelocityBasedAngleComputer;
	    velangcomp->setMultiID( velocityid_ );
	    anglecomp_ = velangcomp;
	    anglecomp_->ref();
	}
	else
	    velocityid_.setEmpty();

	if ( anglecomp_ )
	{
	    int anglestart, anglestop;
	    mGetInt( anglestart, angleStartStr() );
	    mGetInt( anglestop, angleStopStr() );
	    setup_.anglerg_ = Interval<int>( anglestart, anglestop );

	    BufferString raytracerparam;
	    mGetString( raytracerparam, rayTracerParamStr() );
	    IOPar raypar;
	    raypar.getParsFrom( raytracerparam );
	    anglecomp_->setRayTracer( raypar );
	    setSmootheningPar();
	}
    }
    else
    {
	float offstart, offstop;
	mGetFloat( offstart, offStartStr() );
	mGetFloat( offstop, offStopStr() );
	setup_.offsrg_ = Interval<float>( offstart, offstop );

	int gathertype = 0;
	mGetEnum( gathertype, gathertypeStr() );
	int xaxisunit = 0;
	mGetEnum( xaxisunit, xaxisunitStr() );
	setup_.xscaler_ = getXscaler( gathertype == PSAttrib::Off,
				      xaxisunit == PSAttrib::Deg );
	if ( gathertype == (int)(PSAttrib::Ang) && anglegsdpid_>=0 )
	{
	    if ( xaxisunit == (int)(PSAttrib::Rad) )
	    {
		setup_.anglerg_.start = mIsUdf(setup_.offsrg_.start)
		    ? mUdf(int)
		    : mNINT32(Math::toDegrees(setup_.offsrg_.start));
		setup_.anglerg_.stop = mIsUdf(setup_.offsrg_.stop)
		    ? mUdf(int)
		    : mNINT32(Math::toDegrees(setup_.offsrg_.stop));
	    }
	    else
	    {
		setup_.anglerg_.start = mIsUdf(setup_.offsrg_.start)
		    ? mUdf(int) : mNINT32( setup_.offsrg_.start );
		setup_.anglerg_.stop = mIsUdf(setup_.offsrg_.stop)
		    ? mUdf(int) : mNINT32( setup_.offsrg_.stop );
	    }
	}
    }

}


PSAttrib::~PSAttrib()
{
    delete propcalc_;
    delete preprocessor_;

    delete psrdr_;
    delete psioobj_;
    if ( anglecomp_ )
	anglecomp_->unRef();
}


void PSAttrib::setSmootheningPar()
{
    int smoothtype = 0;
    mGetEnum( smoothtype , PreStack::AngleComputer::sKeySmoothType() );
    if ( smoothtype == PreStack::AngleComputer::None )
    {
	anglecomp_->setNoSmoother();
    }
    else if ( smoothtype == PreStack::AngleComputer::MovingAverage )
    {
	BufferString smwindow;
	float windowparam, windowlength;
	mGetString( smwindow, PreStack::AngleComputer::sKeyWinFunc() );
	mGetFloat( windowparam, PreStack::AngleComputer::sKeyWinParam() );
	mGetFloat( windowlength, PreStack::AngleComputer::sKeyWinLen() );

	if ( smwindow == CosTaperWindow::sName() )
	    anglecomp_->setMovingAverageSmoother( windowlength, smwindow,
						  windowparam );
	else
	    anglecomp_->setMovingAverageSmoother( windowlength, smwindow );
    }
    else if ( smoothtype == PreStack::AngleComputer::FFTFilter )
    {
	float f3freq, f4freq;
	mGetFloat( f3freq, PreStack::AngleComputer::sKeyFreqF3() );
	mGetFloat( f4freq, PreStack::AngleComputer::sKeyFreqF4() );

	anglecomp_->setFFTSmoother( f3freq, f4freq );
    }
}


void PSAttrib::setAngleData( DataPack::ID angledpid )
{
    anglegsdpid_ = angledpid;
}


void PSAttrib::setAngleComp( PreStack::AngleComputer* ac )
{
    if ( anglecomp_ ) anglecomp_->unRef();
    if ( !ac ) return;
    anglecomp_ = ac;
    anglecomp_->ref();
    setSmootheningPar();
}


float PSAttrib::getXscaler( bool isoffset, bool isindegrees ) const
{
    return isoffset || !isindegrees ? 1.f : mDeg2RadF;
}


bool PSAttrib::getInputOutput( int input, TypeSet<int>& res ) const
{
    Interval<float>& rg = const_cast<Interval<float>&>(setup_.offsrg_);
    if ( rg.start > 1e28 ) rg.start = 0;
    if ( rg.stop > 1e28 ) rg.stop = mUdf(float);

    return Provider::getInputOutput( input, res );
}


bool PSAttrib::getAngleInputData()
{
    if ( propcalc_->hasAngleData() )
	return true;
    const PreStack::Gather* gather = propcalc_->getGather();
    if ( !gather || !anglecomp_ )
	return false;

    const FlatPosData& fp = gather->posData();
    anglecomp_->setOutputSampling( fp );
    anglecomp_->setTrcKey( TrcKey(gather->getBinID()) );
    PreStack::Gather* angledata = anglecomp_->computeAngles();

    if ( !angledata )
	return false;

    DPM(DataPackMgr::FlatID()).add( angledata );
    propcalc_->setAngleData( angledata->id() );

    return true;
}


bool PSAttrib::getGatherData( const BinID& bid, DataPack::ID& curgatherid,
			      DataPack::ID& curanglegatherid )
{
    if ( gatherset_.size() )
    {
	const PreStack::GatherSetDataPack* anglegsdp = 0;
	if ( anglegsdpid_>= 0 )
	{
	    ConstDataPackRef<DataPack> angledp =
		DPM( DataPackMgr::SeisID() ).obtain( anglegsdpid_ );
	    mDynamicCast( const PreStack::GatherSetDataPack*,anglegsdp,
		    	  angledp.ptr() );
	}

	const PreStack::Gather* curgather = 0;
	const PreStack::Gather* curanglegather = 0;
	for ( int idx=0; idx<gatherset_.size(); idx++ )
	{
	    const bool hasanglegather =
		anglegsdp && anglegsdp->getGathers().validIdx( idx );
	    const int trcnr = idx+1;
	    //TODO full support for 2d : idx is not really my nymber of traces
	    if ( (is2D() && trcnr == bid.crl()) ||
		 (gatherset_[idx]->getBinID() == bid) )
	    {
	       curgather = gatherset_[idx];
	       if ( hasanglegather )
		   curanglegather = anglegsdp->getGathers()[idx];
	       break;
	    }
	}

	if (!curgather ) return false;

	mDeclareAndTryAlloc( PreStack::Gather*, gather,
				PreStack::Gather(*curgather ) );
	
	if ( !gather )
	    return false;

	DPM(DataPackMgr::FlatID()).addAndObtain( gather );
	curgatherid = gather->id();
	
	if ( curanglegather )
	{
	    mDeclareAndTryAlloc( PreStack::Gather*, anglegather,
				 PreStack::Gather(*curanglegather ) );
	    DPM(DataPackMgr::FlatID()).add( anglegather );
	    curanglegatherid = anglegather->id();
	}
    }
    else
    {
	mDeclareAndTryAlloc( PreStack::Gather*, gather, PreStack::Gather );
	if ( !gather )
	    return false;

	if ( !gather->readFrom( *psioobj_, *psrdr_, bid, component_ ) )
	    return false;

	DPM(DataPackMgr::FlatID()).addAndObtain( gather );
	curgatherid = gather->id();
    }

    return true;
}


DataPack::ID PSAttrib::getPreProcessedID( const BinID& relpos )
{
    if ( !preprocessor_->reset() || !preprocessor_->prepareWork() )
	return -1;

    const BinID stepout = preprocessor_->getInputStepout();
    BinID relbid;
    TypeSet<int> gatheridstoberemoved;
    const BinID sistep( SI().inlRange(true).step, SI().crlRange(true).step );
    for ( relbid.inl()=-stepout.inl(); relbid.inl()<=stepout.inl();
	  relbid.inl()++ )
    {
	for ( relbid.crl()=-stepout.crl(); relbid.crl()<=stepout.crl();
	      relbid.crl()++ )
	{
	    if ( !preprocessor_->wantsInput(relbid) )
		continue;

	    const BinID bid = currentbid_+relpos+relbid*sistep;
	    PreStack::Gather* gather = 0;
	    if ( gatherset_.isEmpty() )
	    {
		gather = new PreStack::Gather;
		if (!gather->readFrom(*psioobj_,*psrdr_,bid,component_) )
		    continue;
		DPM(DataPackMgr::FlatID()).addAndObtain( gather );
		gatheridstoberemoved += gather->id();
	    }
	    else
	    {
		for ( int gidx=0; gidx<gatherset_.size(); gidx++ )
		{
		    if ( bid == gatherset_[gidx]->getBinID() )
		    {
			gather = gatherset_[gidx];
			break;
		    }
		}

		if ( !gather )
		    continue;
	    }

	    preprocessor_->setInput( relbid, gather->id() );
	    gather = 0;
	}
    }

    if ( !preprocessor_->process() )
    {
	errmsg_ = preprocessor_->errMsg();
	return -1;
    }

    for ( int idx=0; idx<gatheridstoberemoved.size(); idx++ )
	DPM(DataPackMgr::FlatID()).release( gatheridstoberemoved[idx] );

    return preprocessor_->getOutput();
}


bool PSAttrib::getInputData( const BinID& relpos, int zintv )
{
    if ( !psrdr_ && gatherset_.isEmpty() )
	return false;

    const BinID bid = currentbid_+relpos;
    DataPack::ID curgatherid = -1;
    DataPack::ID curanglegatherid = -1;
    if ( !getGatherData(bid,curgatherid,curanglegatherid) )
	return false;

    if ( preprocessor_ && preprocessor_->nrProcessors() )
    {
	DPM(DataPackMgr::FlatID()).release( curgatherid );
	curgatherid = getPreProcessedID( relpos );
    }

    propcalc_->setGather( curgatherid );
    if ( !propcalc_->hasAngleData() && anglecomp_ && !getAngleInputData() )
	return false;
    else if ( curanglegatherid >= 0 )
	propcalc_->setAngleData( curanglegatherid );

    return true;
}

#define mErrRet(s1) { errmsg_ = s1; return; }

void PSAttrib::prepPriorToBoundsCalc()
{
    delete psioobj_;

    bool isondisc = true;
    const char* fullidstr = psid_.buf();
    if ( fullidstr && *fullidstr == '#' )
    {
	DataPack::FullID fid( fullidstr+1 );

	DataPack* dtp = DPM( fid ).obtain( DataPack::getID(fid) );
	mDynamicCastGet(PreStack::GatherSetDataPack*,psgdtp, dtp)
	isondisc =  !psgdtp;
	if ( isondisc )
	    mErrRet(tr("Cannot obtain gathers kept in memory"))

	gatherset_ = psgdtp->getGathers();
    }
    else
    {
	psioobj_ = IOM().get( psid_ );
	if ( !psioobj_ && isondisc )
	    mErrRet( uiStrings::phrCannotFindDBEntry( toUiString(psid_)) )

	if ( is2D() )
	    psrdr_ = SPSIOPF().get2DReader( *psioobj_, 
					    Survey::GM().getName(geomid_) );
	else
	    psrdr_ = SPSIOPF().get3DReader( *psioobj_ );

	if ( !psrdr_ )
	    mErrRet( uiStrings::phrCannotRead( psioobj_
		    ? psioobj_->uiName()
		    : uiStrings::sVolDataName(true, true, true) ) )

	const uiString emsg = psrdr_->errMsg();
	if ( emsg.isSet() ) mErrRet( tr("PS Reader: %1").arg(emsg) );
    }

    mTryAlloc( propcalc_, PreStack::PropCalc( setup_ ) );
}


void PSAttrib::updateCSIfNeeded( TrcKeyZSampling& cs ) const
{
    if ( !psrdr_ )
	return;

    mDynamicCastGet( SeisPS3DReader*, reader3d, psrdr_ )

    if ( reader3d )
    {
	const PosInfo::CubeData& cd = reader3d->posData();
	StepInterval<int> rg;
	cd.getInlRange( rg );
	cs.hsamp_.setInlRange( rg );
	cd.getCrlRange( rg );
	cs.hsamp_.setCrlRange( rg );
    }

    //TODO: anything we would need to do in 2D?
}


bool PSAttrib::computeData( const DataHolder& output, const BinID& relpos,
			  int z0, int nrsamples, int threadid ) const
{
    if ( !propcalc_ )
	return false;

    float extrazfromsamppos = 0;
    if ( needinterp_ )
	extrazfromsamppos = getExtraZFromSampInterval( z0, nrsamples );

    for ( int idx=0; idx<nrsamples; idx++ )
    {
	const float z = (z0 + idx) * refstep_ + extrazfromsamppos;
	setOutputValue( output, 0, idx, z0, propcalc_->getVal(z) );
    }

    return true;
}

} // namespace Attrib
