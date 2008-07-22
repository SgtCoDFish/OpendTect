/*+
 * COPYRIGHT: (C) dGB Beheer B.V.
 * AUTHOR   : K. Tingdahl
 * DATE     : April 2005
-*/

static const char* rcsID = "$Id: velocityfunction.cc,v 1.1 2008-07-22 17:39:21 cvskris Exp $";

#include "velocityfunction.h"

#include "attribdataholder.h"
#include "binidvalset.h"
#include "cubesampling.h"
#include "interpol1d.h"
#include "ioman.h"
#include "ioobj.h"
#include "survinfo.h"

namespace Vel
{

Function::Function( FunctionSource& vfs )
    : source_( vfs )
    , cache_( 0 )
    , desiredrg_( SI().zRange(true).start, SI().zRange(true).stop,
	          SI().zRange(true).step )
    , bid_( mUdf(int),mUdf(int) )
{
    source_.ref();
}


Function::~Function()
{
    removeCache();
    source_.removeFunction( this );
    source_.unRef();
}


const VelocityDesc& Function::getDesc() const
{
    return source_.getDesc();
}


const StepInterval<float>& Function::getDesiredZ() const
{ return desiredrg_; }


void Function::setDesiredZRange( const StepInterval<float>& n )
{ desiredrg_ = n; }


float Function::getVelocity( float z )const
{
    Threads::MutexLocker lock( cachelock_ );
    if ( !cache_ )
    {
	const StepInterval<float> sampling = getDesiredZ();
	cachesd_ = sampling;
	const int zstart = mNINT(sampling.start/sampling.step);
	const int zstop = mNINT(sampling.stop/sampling.step);
	mTryAlloc( cache_, TypeSet<float>( zstop-zstart+1, mUdf(float) ) );
	if ( !cache_ ) return mUdf(float);

	if ( !computeVelocity( cachesd_.start, cachesd_.step,
		    	       cache_->size(), cache_->arr() ) )
	{
	    delete cache_;
	    cache_ = 0;
	    return mUdf( float );
	}
    }

    lock.unLock();

    const float fsample = cachesd_.getIndex( z );
    const int isample = (int) fsample;
    if ( isample<0 || isample>=cache_->size() )
	return mUdf(float);

    return Interpolate::linearReg1DWithUdf(
	    (*cache_)[isample],
	    isample<(cache_->size()-1)
		? (*cache_)[isample+1]
		: mUdf(float),
	    fsample-isample );
}


const BinID& Function::getBinID() const
{ return bid_; }


bool Function::moveTo( const BinID& bid )
{
    bid_ = bid;
    removeCache();

    return true;
}


void Function::removeCache()
{
    Threads::MutexLocker lock( cachelock_ );
    delete cache_; cache_ = 0;
}


mImplFactory1Param( FunctionSource, const MultiID&, FunctionSource::factory );


BufferString FunctionSource::userName() const
{
    PtrMan<IOObj> ioobj = IOM().get( mid_ );
    if ( ioobj )
	return ioobj->name();
    
    return BufferString( type() );
}


const char* FunctionSource::errMsg() const
{ return errmsg_.isEmpty() ? 0 : errmsg_.buf(); }


void FunctionSource::getSurroundingPositions( const BinID& bid, 
						      BinIDValueSet& bids) const
{
    BinIDValueSet mybids( 0, false );
    getAvailablePositions( mybids );
    if ( !mybids.isEmpty() )
    {
	bids.append( mybids );
	return;
	//PositionNeighborFinder<BinID,int> neighborfinder( 2 );
	//neighborfinder.setCenter( bid );
	//for ( int idx=mybids.size()-1; idx>=0; idx-- )
	    //neighborfinder.addPosition( mybids[idx] );
//
	//mybids.erase();
	//neighborfinder.getPositions( mybids );
//
    }

    HorSampling hs;
    getAvailablePositions( hs );

    if ( hs.includes( bid ) )
	bids.add( bid );
    else
    {
	const StepInterval<int> inlrg = hs.inlRange();
	const StepInterval<int> crlrg = hs.crlRange();
	if ( inlrg.includes(bid.inl) && crlrg.includes(bid.crl) )
	{
	    const float finlidx = inlrg.getfIndex(bid.inl);
	    const float fcrlidx = crlrg.getfIndex(bid.crl);
	    const int previnl = inlrg.atIndex( (int) finlidx );
	    const int prevcrl = crlrg.atIndex( (int) fcrlidx );

	    for ( int inlidx=-1; inlidx<=2; inlidx++ )
	    {
		const int inl = previnl+inlidx * inlrg.step;
		if ( !inlrg.includes(inl) )
		    continue;

		for ( int crlidx=-1; crlidx<=2; crlidx++ )
		{
		    const int crl = prevcrl+crlidx * crlrg.step;
		    if ( !crlrg.includes(crl) )
			continue;

		    bids.add( BinID(inl,crl) );
		}
	    }
	}
	else if ( inlrg.includes(bid.inl) )
	{
	    const int crl = bid.crl<crlrg.start ? crlrg.start : crlrg.stop;
	    for ( int inl=inlrg.start; inl<=inlrg.stop; inl+=inlrg.step )
		bids.add( BinID(inl,crl) );
	}
	else if ( crlrg.includes(bid.crl) )
	{
	    const int inl = bid.inl<inlrg.start ? inlrg.start : inlrg.stop;
	    for ( int crl=crlrg.start; crl<=crlrg.stop; crl+=crlrg.step )
		bids.add( BinID(inl,crl) );
	}
    }
}


int FunctionSource::findFunction( const BinID& bid ) const
{
    for ( int idx=functions_.size()-1; idx>=0; idx-- )
    {
	if ( functions_[idx]->getBinID()==bid )
	    return idx;
    }

    return -1;
}


const Function* FunctionSource::getFunction( const BinID& bid )
{
    if ( mIsUdf(bid.inl) || mIsUdf(bid.crl) )
	return 0;

    const int idx = findFunction( bid );
    if ( idx!=-1 )
	return functions_[idx];

    Function* func = createFunction(bid);
    if ( !func )
	return 0;

    functions_ += func;
    return func;
}


void FunctionSource::removeFunction(Function* v)
{ functions_ -= v; }


}; //namespace




//#include "velocityfunctionsource.h"

