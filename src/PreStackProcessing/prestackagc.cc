/*+
 * COPYRIGHT: (C) dGB Beheer B.V.
 * AUTHOR   : K. Tingdahl
 * DATE     : April 2005
-*/

static const char* rcsID = "$Id: prestackagc.cc,v 1.1 2007-03-15 17:28:52 cvskris Exp $";

#include "prestackagc.h"

#include "dataclipper.h"
#include "iopar.h"
#include "prestackgather.h"


using namespace PreStack;


void AGC::initClass()
{
    PF().addCreator( AGC::createFunc, AGC::sName() );
}


Processor* AGC::createFunc()
{ return new AGC; }


AGC::AGC()
    : window_( -100, 100 )
    , mutefraction_( 0 )
{}


bool AGC::prepareWork()
{
    if ( !Processor::prepareWork() )
	return false;

    samplewindow_.start = mNINT( window_.start/input_->zSampling().step );
    samplewindow_.stop = mNINT( window_.stop/input_->zSampling().step );

    return true;
}


void AGC::setWindow( const Interval<float>& iv )
{ window_ = iv; }


const Interval<float>& AGC::getWindow() const
{ return window_; }


void AGC::setLowEnergyMute( float iv )
{ mutefraction_ = iv; }


float AGC::getLowEnergyMute() const
{ return mutefraction_; }


void AGC::fillPar( IOPar& par ) const
{
    par.set( sKeyWindow(), window_.start, window_.stop );
    par.set( sKeyMuteFraction(), mutefraction_ );
}


bool AGC::usePar( const IOPar& par )
{
    par.get( sKeyWindow(), window_.start, window_.stop );
    par.get( sKeyMuteFraction(), mutefraction_ );
    return true;
}


bool AGC::doWork( int start, int stop, int )
{
    const int nrsamples = input_->data().info().getSize(Gather::zDim());

    const bool doclip = !mIsZero( mutefraction_, 1e-5 );
    DataClipper clipper( mutefraction_, 0 );

    for ( int offsetidx=start; offsetidx<=stop; offsetidx++ )
    {
	mVariableLengthArr( float, energies, nrsamples );
	for ( int sampleidx=0; sampleidx<nrsamples; sampleidx++ )
	{
	    const float value = input_->data().get(offsetidx,sampleidx);
	    energies[sampleidx] = mIsUdf( value ) ? value : value*value;
	}

	if ( doclip )
	{
	    clipper.putData( energies, nrsamples );
	    if ( !clipper.calculateRange() )
	    {
		//todo: copy old trace
		continue;
	    }
	}

	for ( int sampleidx=0; sampleidx<nrsamples; sampleidx++ )
	{
	    int nrenergies = 0;
	    float energysum = 0;
	    for ( int energyidx=sampleidx+samplewindow_.start;
		      energyidx<=sampleidx+samplewindow_.stop;
		      energyidx++ )
	    {
		if ( energyidx<0 || energyidx>=nrsamples )
		    continue;

		const float energy = energies[energyidx];
		if ( mIsUdf( energy ) )
		    continue;

		if ( doclip && !clipper.getRange().includes( energy ) )
		    continue;
		
		energysum += energy;
		nrenergies++;
	    }

	    float outputval = 0;
	    if ( nrenergies && !mIsZero(energysum,1e-6) )
	    {
		const float inpval = input_->data().get(offsetidx,sampleidx);
		outputval = inpval/sqrt(energysum/nrenergies);
	    }

	    output_->data().set( offsetidx, sampleidx, outputval );
	}
    }

    return true;
}
