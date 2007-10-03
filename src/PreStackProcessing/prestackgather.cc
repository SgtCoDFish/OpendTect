/*+
 * COPYRIGHT: (C) dGB Beheer B.V.
 * AUTHOR   : K. Tingdahl
 * DATE     : April 2005
-*/

static const char* rcsID = "$Id: prestackgather.cc,v 1.6 2007-10-03 14:01:33 cvskris Exp $";

#include "prestackgather.h"

#include "genericnumer.h"
#include "flatposdata.h"
#include "ioobj.h"
#include "iopar.h"
#include "ioman.h"
#include "property.h"
#include "ptrman.h"
#include "samplfunc.h"
#include "seisbuf.h"
#include "seistrc.h"
#include "seispsioprov.h"
#include "seispsread.h"
#include "seisread.h"
#include "seistrcsel.h"
#include "seistrctr.h"
#include "survinfo.h"
#include "unitofmeasure.h"

using namespace PreStack;

const char* Gather::sKeyIsAngleGather()	{ return "Angle Gather"; }
const char* Gather::sKeyIsNMO()		{ return "Is NMO Corrected"; }
const char* Gather::sKeyVelocityCubeID()	{ return "Velocity volume"; }
const char* Gather::sKeyZisTime()		{ return "Z Is Time"; }

Gather::Gather()
    : SeisTrcBufDataPack( 0, Seis::VolPS, SeisTrcInfo::Offset,
	    		     "Pre-Stack Gather", 0 )
    , offsetisangle_( false )
    , iscorr_( false )
    , binid_( -1, -1 )
{}



Gather::Gather( const Gather& gather )
    : SeisTrcBufDataPack( gather )
    , offsetisangle_( gather.offsetisangle_ )
    , iscorr_( gather.iscorr_ )
    , binid_( gather.binid_ )
{}


bool Gather::setSize( int nroff, int nrz )
{
    return true;
}


Gather::~Gather()
{}


bool Gather::readFrom( const MultiID& mid, const BinID& bid, 
			   BufferString* errmsg )
{
    PtrMan<IOObj> ioobj = IOM().get( mid );
    if ( !ioobj )
    {
	if ( errmsg ) (*errmsg) = "No valid gather selected.";
	delete arr2d_; arr2d_ = 0;
	return false;
    }

    PtrMan<SeisPSReader> rdr = SPSIOPF().getReader( *ioobj, bid.inl );
    if ( !rdr )
    {
	if ( errmsg )
	    (*errmsg) = "This Pre-Stack data store cannot be handeled.";
	delete arr2d_; arr2d_ = 0;
	return false;
    }

    SeisTrcBuf* tbuf = new SeisTrcBuf;
    if ( !rdr->getGather(bid,*tbuf) )
    {
	if ( errmsg ) (*errmsg) = rdr->errMsg();
	delete arr2d_; arr2d_ = 0;
	delete tbuf;
	return false;
    }

    tbuf->sort( true, SeisTrcInfo::Offset );

    for ( int idx=tbuf->size()-1; idx<-0; idx-- )
    {
	if ( mIsUdf( tbuf->get(idx)->info().offset ) )
	    delete tbuf->remove( idx );
    }

    if ( tbuf->isEmpty() )
    {
	delete arr2d_; arr2d_ = 0;
	delete tbuf;
	return false;
    }

    setBuffer( tbuf, Seis::VolPS, SeisTrcInfo::Offset, 0 );

    offsetisangle_ = false;
    ioobj->pars().getYN(sKeyIsAngleGather(), offsetisangle_ );

    iscorr_ = false;
    ioobj->pars().getYN(sKeyIsNMO(), iscorr_ );

    zit_ = SI().zIsTime();
    ioobj->pars().getYN(sKeyZisTime(),zit_);

    ioobj->pars().get(sKeyVelocityCubeID(), velocitymid_ );
    
    binid_ = bid;
    setName( ioobj->name() );

    return true;
}


float Gather::getOffset( int idx ) const
{ return posData().position( true, idx ); }


float Gather::getAzimuth( int idx ) const
{
    return trcBuf().get( idx )->info().azimuth;
}


bool Gather::getVelocityID(const MultiID& stor, MultiID& vid )
{
    PtrMan<IOObj> ioobj = IOM().get( stor );
    return ioobj ? ioobj->pars().get( sKeyVelocityCubeID(), vid ) : false;
}

