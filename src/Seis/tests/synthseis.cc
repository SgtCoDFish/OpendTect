/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : A. Huck
 * DATE     : Oct 2013
-*/


#include "synthseis.h"
#include "testprog.h"

#include "ailayer.h"
#include "batchprog.h"
#include "commandlineparser.h"
#include "factory.h"
#include "ioman.h"
#include "ioobj.h"
#include "moddepmgr.h"
#include "multiid.h"
#include "ptrman.h"
#include "raytrace1d.h"
#include "raytracerrunner.h"
#include "seistrc.h"
#include "survinfo.h"
#include "wavelet.h"

static const char* sKeyWaveletID()	{ return "Wavelet"; }
#define cStep 0.004f
#define mTest( testname, test, message ) \
if ( (test)==true ) \
{ \
    if ( !quiet ) \
	strm << testname << ": OK" << od_newline; \
} \
else \
{ \
    strm << testname << ": Failed" << od_newline; \
    if ( message ) \
	strm << message << od_newline; \
    \
    return false; \
}


void initTest( bool onespike, bool onemodel, float start_depth,
	       TypeSet<ElasticModel>& models )
{
    AILayer layer1 = AILayer( start_depth, 2000.f, 2500.f );
    AILayer layer2 = AILayer( 520.f, 2600.f, 2300.f );
    AILayer layer3 = AILayer( 385.f, 3500.f, 2200.f );
    AILayer layer4 = AILayer( 350.f, 4000.f, 2800.f );

    models += ElasticModel();
    models[0] += layer1;
    models[0] += layer3;
    if ( !onespike )
	models[0] += layer4;

    if ( onemodel ) return;

    models += ElasticModel();
    models[1] += layer1;
    models[1] += layer2;
    if ( onespike ) return;
    models[1] += layer3;
    models[1] += layer4;
}


bool testSynthGeneration( od_ostream& strm, bool success,
			  Seis::RaySynthGenerator& synthgen )
{
    BufferString testname( "test Synthetic generation" );
    mTest( testname, success, synthgen.errMsg().getOriginalString() )

    return true;
}


bool testTraceSize( od_ostream& strm, SeisTrc& trc )
{
    BufferString testname( "test Trace size" );
    const StepInterval<float> zrg1( 0.028f, 0.688f, cStep );
    const StepInterval<float> zrg2( -0.012f, 0.728f, cStep );
    const StepInterval<float>& zrg = trc.zRange().isEqual(zrg1,1e-4f)
				   ? zrg1 : zrg2;
    BufferString msg( "Expected trace range: [", zrg.start, " " );
    msg.add( zrg.stop ).add( "] step " ).add( zrg.step ).add( "\n" );
    msg.add( "Output trace range: [" ).add( trc.startPos() ).add( " " );
    msg.add( trc.endPos() ).add( "] step " ).add( trc.info().sampling_.step );
    mTest( testname, trc.zRange().isEqual(zrg,1e-4f), msg )

    return true;
}


bool testSpike( od_ostream& strm, const SeisTrc& trc,
		const ReflectivitySpike& spike,	float scal, int nr )
{
    BufferString testname( "test Spike ", nr, " is defined" );
    mTest( testname, spike.isDefined(), "Spike is not defined" )

    testname = "test amplitude of the spike ";
    testname.add( nr ).add( " in the trace" );
    const float twt = spike.time_;
    const float ref = spike.reflectivity_.real();
    const float traceval = trc.getValue( twt, 0 );
    BufferString msg( "Trace amplitude: ", traceval, " at " );
    msg.add( twt ).add( "\n" ).add( "Expected amplitude is: " ).add( ref*scal );
    mTest( testname, mIsEqual(traceval,ref*scal,1e-3f), msg )

    return true;
}


bool testTracesAmplitudes( od_ostream& strm,
			   Seis::RaySynthGenerator& synthgen, float scal )
{
    BufferString testname( "test Traces amplitudes" );
    bool success = true;
    const int nrpos = synthgen.elasticModels().size();
    int nr = -1;
    for ( int ipos=0; ipos<nrpos; ipos++ )
    {
	Seis::RaySynthGenerator::RayModel& raymodel = synthgen.result( ipos );
	ObjectSet<SeisTrc> gather;
	raymodel.getTraces( gather, false );
	ObjectSet<const ReflectivityModel> refmodels;
	raymodel.getRefs( refmodels, false );
	for ( int ioff=0; ioff<refmodels.size(); ioff++ )
	{
	    const SeisTrc& trout = *gather[ioff];
	    const ReflectivityModel& refmodel = *refmodels[ioff];
	    for ( int idz=0; idz<refmodel.size(); idz++ )
	    {
		nr++;
		if ( !testSpike(strm,trout,refmodel[idz],scal,nr) )
		    success = false;
	    }
	}
    }

    mTest( testname, success, 0 )

    return true;
}


bool BatchProgram::go( od_ostream& strm )
{
    mInitBatchTestProg();
    OD::ModDeps().ensureLoaded( "Seis" );
    VrmsRayTracer1D::initClass();

    // Inputs
    TypeSet<ElasticModel> models;
    const bool singlespike = false;
    const int nrmodels = 2; // model1: 2 spikes, model2: 3 spikes
    const float start_depth = 48.f;

    ObjectSet<Wavelet> wvlts;
    Wavelet syntheticricker(true,50.f,cStep,1.f); //Ricker 50Hz, 4ms SR
    wvlts += &syntheticricker;
    MultiID wavid;
    if ( !pars().get(sKeyWaveletID(),wavid) )
    {
	strm << "Can not find wavelet from parameter file" << od_newline;
	return false;
    }
    PtrMan<IOObj> wavioobj = IOM().get( wavid );
    if ( !wavioobj )
    {
	strm << "Input wavelet is not available." << od_newline;
	return false;
    }

    PtrMan<Wavelet> realwav = Wavelet::get( wavioobj );
    if ( !realwav )
    {
	strm << "Input wavelet could not be read." << od_newline;
	return false;
    }

    wvlts += realwav;
    initTest( singlespike, nrmodels==1, start_depth, models );

    PtrMan<IOPar> raypar = pars().subselect( "Ray Tracer" );
    if ( !raypar )
    {
	strm << "Input RayTracer could not be found." << od_newline;
	return false;
    }

    // Run
    for ( int iwav=0; iwav<wvlts.size(); iwav++ )
    {
	const Wavelet* wav = wvlts[iwav];
	if ( !wav )
	    return false;

	const float scal = wav->get( wav->centerSample() );
	Seis::RaySynthGenerator synthgen( &models );
	synthgen.setWavelet( wav, OD::UsePtr );
	synthgen.enableFourierDomain( true );
	synthgen.usePar( *raypar );

	TaskRunner* taskr = new TaskRunner;
	if ( !testSynthGeneration(strm,TaskRunner::execute(taskr,synthgen),
				  synthgen) )
	    return false;

	Seis::RaySynthGenerator::RayModel& rm = synthgen.result( nrmodels-1 );
	SeisTrc stack = *rm.stackedTrc();
	if ( !testTraceSize(strm,stack) ||
	     !testTracesAmplitudes(strm,synthgen,scal) )
	    return false;
    }

    return true;
};
