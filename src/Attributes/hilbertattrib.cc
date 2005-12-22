/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        N. Hemstra
 Date:          May 2005
 RCS:           $Id: hilbertattrib.cc,v 1.14 2005-12-22 14:55:56 cvsnanne Exp $
________________________________________________________________________

-*/

#include "hilbertattrib.h"

#include "attribdataholder.h"
#include "attribdesc.h"
#include "attribfactory.h"
#include "attribparam.h"
#include "genericnumer.h"

namespace Attrib
{

void Hilbert::initClass()
{
    Desc* desc = new Desc( attribName() );
    desc->ref();

    IntParam* halflen = new IntParam( halflenStr() );
    halflen->setDefaultValue( 30 );
    halflen->setValue( 30 );
    halflen->setRequired( false );
    desc->addParam( halflen );

    desc->addInput( InputSpec("Input data",true) );
    desc->addOutputDataType( Seis::UnknowData );
    desc->init();

    PF().addDesc( desc, createInstance );
    desc->unRef();
}


Provider* Hilbert::createInstance( Desc& desc )
{
    Hilbert* res = new Hilbert( desc );
    res->ref();

    if ( !res->isOK() )
    {
	res->unRef();
	return 0;
    }

    res->unRefNoDelete();
    return res;
}


Hilbert::Hilbert( Desc& ds )
    : Provider( ds )
{
    if ( !isOK() ) return;

    mGetInt( halflen_, halflenStr() );
    hilbfilterlen_ = halflen_ * 2 + 1;
    hilbfilter_ = makeHilbFilt( halflen_ );
    gate_ = Interval<float>( -halflen_-3, halflen_+3 );
}


bool Hilbert::getInputOutput( int input, TypeSet<int>& res ) const
{
    return Provider::getInputOutput( input, res );
}


bool Hilbert::getInputData( const BinID& relpos, int intv )
{
    inputdata_ = inputs[0]->getData( relpos, intv );
    dataidx_ = getDataIndex( 0 );
    return inputdata_;
}

/*
const Interval<float>* Hilbert::desZMargin( int inp, int ) const
{
    const_cast<Interval<float>*>(&timegate)->start = gate.start * refstep;
    const_cast<Interval<float>*>(&timegate)->stop =  gate.stop * refstep;
    return &timegate; 
}
*/

float* Hilbert::makeHilbFilt( int hlen )
{
    float* h = new float[hlen*2+1];
    h[hlen] = 0;
    for ( int i=1; i<=hlen; i++ )
    {
	const float taper = 0.54 + 0.46 * cos( M_PI*(float)i / (float)(hlen) );
	h[hlen+i] = taper * ( -(float)(i%2)*2.0 / (M_PI*(float)(i)) );
	h[hlen-i] = -h[hlen+i];
    }

    return h;
}


class Masker
{
public:
Masker( const DataHolder* dh, int shift, float avg, int dataidx )
    : data_(dh )
    , avg_(avg)
    , shift_(shift)
    , dataidx_(dataidx) {}

float operator[]( int idx ) const
{
    const int pos = shift_ + idx;
    if ( pos < 0 )
	return data_->series(dataidx_)->value(0) - avg_;
    if ( pos >= data_->nrsamples_ )
	return data_->series(dataidx_)->value(data_->nrsamples_-1) - avg_;
    return data_->series(dataidx_)->value( pos ) - avg_;
}

    const DataHolder*	data_;
    const int		shift_;
    float		avg_;
    int			dataidx_;
};


bool Hilbert::computeData( const DataHolder& output, const BinID& relpos, 
			   int z0, int nrsamples ) const
{
    if ( !inputdata_ ) return false;

    const int shift = z0 - inputdata_->z0_;
    Masker masker( inputdata_, shift, 0, dataidx_ );
    float avg = 0;
    int nrsampleused = nrsamples;
    for ( int idx=0; idx<nrsamples; idx++ )
    {
	float val = masker[idx];
	if ( mIsUdf(val) )
	{
	    avg += 0;
	    nrsampleused--;
	}
	else
	    avg += val;
    }

    masker.avg_ = avg / nrsampleused;
    float* outp = output.series(0)->arr();
    GenericConvolve( hilbfilterlen_, -halflen_, hilbfilter_, nrsamples,
		     0, masker, nrsamples, 0, outp );

    return true;
}

}; // namespace Attrib
