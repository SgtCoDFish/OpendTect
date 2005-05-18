/*+
 * COPYRIGHT: (C) dGB Beheer B.V.
 * AUTHOR   : A.H. Bril
 * DATE     : 28-2-1996
 * FUNCTION : Seismic trace functions
-*/

static const char* rcsID = "$Id: seistrcprop.cc,v 1.12 2005-05-18 09:20:45 cvsbert Exp $";

#include "seistrcprop.h"
#include "seistrc.h"
#include "susegy.h"
#include "simpnumer.h"
#include "timeser.h"
#include "ptrman.h"
#include <math.h>
#include <float.h>


Seis::Event SeisTrcPropCalc::find( Seis::Event::Type evtype,
				   Interval<float> tg, int occ ) const
{
    Seis::Event ev;
    if ( mIsZero(tg.width(),mDefEps) ) return ev;

    int sz = trc.size(); float sr = trc.info().sampling.step;
    SampleGate sg;
    tg.shift( -trc.startPos() );
    if ( tg.start < 0 ) tg.start = 0; if ( tg.stop < 0 ) tg.stop = 0;
    float endpos = trc.samplePos(sz-1);
    if ( tg.start > endpos ) tg.start = endpos;
    if ( tg.stop > endpos ) tg.stop = endpos;

    int inc;
    if ( tg.start < tg.stop )
    {
	inc = 1;
	sg.start = (int)floor(tg.start/sr);
	sg.stop = (int)ceil(tg.stop/sr);
    }
    else
    {
	inc = -1;
	sg.start = (int)ceil(tg.start/sr);
	sg.stop = (int)floor(tg.stop/sr);
	if ( evtype == Seis::Event::ZCNegPos )
	    evtype = Seis::Event::ZCPosNeg;
	else if ( evtype == Seis::Event::ZCPosNeg )
	    evtype = Seis::Event::ZCNegPos;
    }

    float prev = trc.get( sg.start, curcomp );
    int upward;
    if ( sg.start-inc >= 0 && sg.start-inc < sz )
	upward = prev > trc.get( sg.start-inc, curcomp );
    else
	// avoid extreme at start of gate
        upward = evtype == Seis::Event::Max
			? prev > trc.get( sg.start+inc, curcomp )
			: prev < trc.get( sg.start+inc, curcomp );

    if ( evtype == Seis::Event::GateMax || evtype == Seis::Event::GateMin )
	occ = 2;
    int iextr = sg.start;
    float extr = 0;

    for ( int idx=sg.start+inc; idx!=sg.stop+inc; idx+=inc )
    {
	float cur = trc.get( idx, curcomp );

	switch ( evtype )
	{
	case Seis::Event::Extr:		if ( ( prev <= cur && !upward )
					  || ( prev >= cur && upward  ) )
					    occ--;
	break;
	case Seis::Event::Min:		if ( prev <= cur && !upward ) occ--;
	break;
	case Seis::Event::Max:		if ( prev >= cur && upward ) occ--;
	break;
	case Seis::Event::GateMin: if ( prev <= cur && !upward && prev < extr 
					   && idx > sg.start+inc )
					    { extr = prev; iextr = idx-inc; }
	break;
	case Seis::Event::GateMax: if ( prev >= cur && upward && prev > extr 
					   && idx > sg.start+inc )
				    { extr = prev; iextr = idx-inc; }
	break;

	case Seis::Event::ZC:		if ( ( cur >= 0 && prev < 0 )
					  || ( cur <= 0 && prev > 0 ) )
					    occ--;
	break;
	case Seis::Event::ZCNegPos:	if ( cur >= 0 && prev < 0 ) occ--;
	break;
	case Seis::Event::ZCPosNeg:	if ( cur <= 0 && prev > 0 ) occ--;
	break;
	}

	if ( occ < 1 )
	{
	    switch ( evtype )
	    {
	    case Seis::Event::Extr:
	    case Seis::Event::Min: case Seis::Event::Max:
	    case Seis::Event::GateMax: case Seis::Event::GateMin:
		getPreciseExtreme( ev, idx-inc, inc, prev, cur );
	    break;
	    case Seis::Event::ZC:
	    case Seis::Event::ZCNegPos: case Seis::Event::ZCPosNeg:
	    {
		// Linear (who cares?)
		float reldist = cur / ( cur - prev );
		float pos = inc < 0 ? idx + reldist : idx - reldist;
		ev.pos = trc.startPos() + pos * sr;
		ev.val = 0;
	    }
	    break;
	    }
	    return ev;
	}

	upward = prev < cur;
	prev = cur;
    }

    if ( ( evtype == Seis::Event::GateMin || evtype == Seis::Event::GateMax )
      && ( iextr != sg.start && iextr != sg.stop ) )
	getPreciseExtreme( ev, iextr, inc,
			   trc.get(iextr,curcomp), trc.get(iextr+inc,curcomp) );

    return ev;
}


void SeisTrcPropCalc::getPreciseExtreme( Seis::Event& ev, int idx, int inc,
				 float y2, float y3 ) const
{
    float y1 = idx-inc < 0 || idx-inc >= trc.size()
	     ?  y2 : trc.get( idx-inc, curcomp );
    ev.pos = trc.startPos() + idx * trc.info().sampling.step;
    ev.val = y2;

    float a = ( y3 + y1 - 2 * y2 ) / 2;
    float b = ( y3 - y1 ) / 2;
    if ( !mIsZero(a,mDefEps) )
    {
	float pos = - b / ( 2 * a );
	ev.pos += inc * pos * trc.info().sampling.step;
	ev.val += ( a * pos + b ) * pos;
    }
}


#define mStartCompLoop \
    for ( int icomp = (curcomp<0 ? 0 : curcomp); \
	  icomp < (curcomp<0 ? trc.data().nrComponents() : curcomp+1); \
	  icomp++ ) \
    {
#define mEndCompLoop }


void SeisTrcPropChg::stack( const SeisTrc& trc2, bool alongpick )
{
    float pick = trc2.info().pick;
    if ( alongpick && (Values::isUdf(pick) || Values::isUdf(trc.info().pick)) )
	alongpick = NO;
    float diff;
    if ( alongpick )
    {
	diff = trc.info().pick - pick;
	if ( mIsZero(diff,mDefEps) ) alongpick = NO;
    }

    const float wght = trc.info().stack_count;
    mtrc().info().stack_count++;

    mStartCompLoop
    const int sz = trc.size();
    for ( int idx=0; idx<sz; idx++ )
    {
	float val = trc.get( idx, icomp ) * wght;
	val += alongpick
	     ? trc2.getValue( trc.samplePos(idx)-diff, icomp )
	     : trc2.get( idx, icomp );
	val /= wght + 1.;
	mtrc().set( idx, val, icomp );
    }
    mEndCompLoop
}


void SeisTrcPropChg::removeDC()
{
    mStartCompLoop
    int sz = trc.size();
    if ( !sz ) continue;

    float avg = 0;
    for ( int idx=0; idx<sz; idx++ )
	avg += trc.get(idx,icomp);
    avg /= sz;
    for ( int idx=0; idx<sz; idx++ )
	mtrc().set( idx, trc.get(idx,icomp) - avg, icomp );
    mEndCompLoop
}


void SeisTrcPropCalc::gettr( SUsegy& sutrc ) const
{
    trc.info().gettr( sutrc );
    sutrc.ns = trc.size();

    if ( trc.data().getInterpreter(curcomp)->isSUCompat() )
	memcpy( sutrc.data, trc.data().getComponent(curcomp)->data(),
		sutrc.ns*sizeof(float) );
    else
    {
	for ( int idx=0; idx<sutrc.ns; idx++ )
	    sutrc.data[idx] = trc.get( idx, curcomp );
    }
}


void SeisTrcPropChg::puttr( const SUsegy& sutrc )
{
    const int icomp = curcomp < 0 ? 0 : curcomp;

    mtrc().info().puttr( sutrc );
    mtrc().reSize( sutrc.ns, false );

    if ( trc.data().getInterpreter(icomp)->isSUCompat() )
	memcpy( mtrc().data().getComponent(icomp)->data(), sutrc.data,
		sutrc.ns*sizeof(float) );
    else
    {
	for ( int idx=0; idx<sutrc.ns; idx++ )
	    mtrc().set( idx, sutrc.data[idx], icomp );
    }
}


void SeisTrcPropChg::scale( float fac, float shft )
{
    mStartCompLoop
    const int sz = trc.size();
    for ( int idx=0; idx<sz; idx++ )
	mtrc().set( idx, shft + fac * trc.get( idx, icomp ), icomp );
    mEndCompLoop
}


void SeisTrcPropChg::normalize( bool aroundzero )
{
    mStartCompLoop
    const int sz = trc.size();
    if ( !sz ) continue;

    float val = trc.get( 0, icomp );
    Interval<float> rg( val, val );
    if ( aroundzero )
	rg.start = rg.stop = 0;

    for ( int idx=1; idx<sz; idx++ )
    {
        val = trc.get( idx, icomp );
	if ( aroundzero )
	{
	    if ( val > 0 )
		{ if ( val > rg.stop ) rg.stop = val; }
	    else
		{ if ( val < rg.start ) rg.start = val; }
	}
	else
	{
	    if ( val < rg.start ) rg.start = val;
	    if ( val > rg.stop ) rg.stop = val;
	}
    }
    float diff = rg.stop - rg.start;
    if ( mIsZero(diff,mDefEps) )
    {
	for ( int idx=0; idx<sz; idx++ )
	    mtrc().set( idx, 0, icomp );
    }
    else
    {
	if ( aroundzero )
	{
	    float maxval = -rg.start;
	    if ( rg.stop > maxval ) maxval = rg.stop;
	    const float sc = 1. / maxval;
	    for ( int idx=0; idx<sz; idx++ )
		mtrc().set( idx, trc.get(idx,icomp) * sc, icomp );
	}
	else
	{
	    float a = 2 / diff;
	    float b = 1 - rg.stop * a;
	    for ( int idx=0; idx<sz; idx++ )
		mtrc().set( idx, trc.get( idx, icomp ) * a + b, icomp );
	}
    }
    mEndCompLoop
}


void SeisTrcPropChg::corrNormalize()
{
    mStartCompLoop
    int sz = trc.size();
    if ( !sz ) continue;

    double val = trc.get( 0, icomp );
    double autocorr = val * val;
    for ( int idx=1; idx<sz; idx++ )
    {
        val = trc.get( idx, icomp );
	autocorr += val*val;
    }
    double sqrtacorr = sqrt( autocorr );

    if ( sqrtacorr < 1e-30 )
    {
	for ( int idx=0; idx<sz; idx++ )
	    mtrc().set( idx, 0, icomp );
    }
    else
    {
	for ( int idx=0; idx<sz; idx++ )
	    mtrc().set( idx, trc.get( idx, icomp ) / sqrtacorr, icomp );
    }
    mEndCompLoop
}


float SeisTrcPropCalc::getFreq( int isamp ) const
{
    float mypos = trc.samplePos( isamp );
    float endpos = trc.samplePos( trc.size() - 1 );
    if ( mypos < trc.startPos() || mypos > endpos )
	return mUdf(float);

    // Find nearest crests
    Interval<float> tg( mypos-2*trc.info().sampling.step, trc.startPos() );
    Seis::Event ev1 = SeisTrcPropCalc::find( Seis::Event::Extr, tg, 1 );
    tg.start = mypos+2*trc.info().sampling.step; tg.stop = endpos;
    Seis::Event ev2 = SeisTrcPropCalc::find( Seis::Event::Extr, tg, 1 );
    if ( Values::isUdf(ev1.pos) || Values::isUdf(ev2.pos) )
	return mUdf(float);

    // If my sample is an extreme, the events are exactly at a wavelength
    float myval = trc.get( isamp, curcomp );
    float val0 = trc.get( isamp-1, curcomp );
    float val1 = trc.get( isamp+1, curcomp );
    if ( (myval > val0 && myval > val1) || (myval < val0 && myval < val1) )
    {
	float wvpos = ev2.pos - ev1.pos;
	return wvpos > 1e-6 ? 1 / wvpos : mUdf(float);
    }

    // Now find next events ...
    tg.start = mypos-2*trc.info().sampling.step; tg.stop = trc.startPos();
    Seis::Event ev0 = SeisTrcPropCalc::find( Seis::Event::Extr, tg, 2 );
    tg.start = mypos+2*trc.info().sampling.step; tg.stop = endpos;
    Seis::Event ev3 = SeisTrcPropCalc::find( Seis::Event::Extr, tg, 2 );

    // Guess where they would have been when not found
    float dpos = ev2.pos - ev1.pos;
    if ( Values::isUdf(ev0.pos) )
	ev0.pos = ev1.pos - dpos;
    if ( Values::isUdf(ev3.pos) )
	ev3.pos = ev2.pos + dpos;

    // ... and create a weigthed wavelength
    float d1 = (mypos - ev1.pos) / dpos;
    float d2 = (ev2.pos - mypos) / dpos;
    float wvpos = dpos + d1*(ev3.pos-ev2.pos) + d2*(ev1.pos-ev0.pos);
    return wvpos > 1e-6 ? 1 / wvpos : mUdf(float);
}


float SeisTrcPropCalc::getPhase( int isamp ) const
{
    int quadsz = 1 + 2 * mHalfHilbertLength;
    ArrPtrMan<float> trcdata = new float[ quadsz ];
    int start = isamp - mHalfHilbertLength;
    int stop = isamp + mHalfHilbertLength;
    const int sz = trc.size();
    for ( int idx=start; idx<=stop; idx++ )
    {
        int trcidx = idx < 0 ? 0 : (idx>=sz ? sz-1 : idx);
        trcdata[idx-start] = trc.get( trcidx, curcomp );
    }
    ArrPtrMan<float> quadtrc = new float[ quadsz ];
    Hilbert( quadsz, trcdata, quadtrc );

    float q = quadtrc[mHalfHilbertLength];
    float d = trcdata[mHalfHilbertLength];
    return d*d + q*q ? atan2( q, d ) : 0;
}


void SeisTrcPropChg::topMute( float mpos, float taperlen )
{
    mStartCompLoop
    if ( mpos < trc.startPos() ) return;
    int endidx = trc.size() - 1;
    if ( mpos > trc.samplePos(endidx) )
	mpos = trc.samplePos(endidx);
    if ( taperlen < 0 || Values::isUdf(taperlen) )
	taperlen = 0;
//  if ( !Values::isUdf(trc.info().mute_pos) && trc.info().mute_pos >= mpos )
//	return;

    mtrc().info().mute_pos = mpos;
    mtrc().info().taper_length = taperlen;

    float pos = trc.startPos();
    while ( pos < mpos + mDefEps )
    {
	int idx = trc.nearestSample( pos );
	mtrc().set( idx, 0, icomp );
	pos += trc.info().sampling.step;
    }

    if ( mIsZero(taperlen,mDefEps) ) return;

    float pp = mpos + taperlen;
    if ( pp > trc.samplePos(endidx) )
	pp = trc.samplePos(endidx);
    while ( pos < pp + mDefEps )
    {
	int idx = trc.nearestSample( pos );
	float x = ((pos - mpos) / taperlen) * M_PI;
	float taper = 0.5 * ( 1 - cos(x) );
	mtrc().set( idx, trc.get( idx, icomp ) * taper, icomp );
	pos += trc.info().sampling.step;
    }
    mEndCompLoop
}


void SeisTrcPropChg::tailMute( float mpos, float taperlen )
{
    mStartCompLoop
    int endidx = trc.size() - 1;
    if ( mpos > trc.samplePos(endidx) ) return;
    if ( mpos < trc.startPos() )
	mpos = trc.startPos();

    if ( taperlen < 0 || Values::isUdf(taperlen) )
	taperlen = 0;
//  if ( !Values::isUdf(trc.info().mute_pos) && trc.info().mute_pos >= mpos )
//	return;

    mtrc().info().mute_pos = mpos;
    mtrc().info().taper_length = taperlen;

    float pos = mpos;
    while ( pos <= trc.samplePos(endidx)  )
    {
	int idx = trc.nearestSample( pos );
	mtrc().set( idx, 0, icomp );
	pos += trc.info().sampling.step;
    }

    if ( mIsZero(taperlen,mDefEps) ) return;

    float pp = mpos - taperlen;
    if ( pp < trc.startPos() )
	pp = trc.startPos();
    pos = pp;
    while ( pos <= mpos )
    {
	int idx = trc.nearestSample( pos );
	float x = ((mpos - pos) / taperlen) * M_PI;
	float taper = 0.5 * ( 1 - cos(x) );
	mtrc().set( idx, trc.get( idx, icomp ) * taper, icomp );
	pos += trc.info().sampling.step;
    }
    mEndCompLoop
}


double SeisTrcPropCalc::corr( const SeisTrc& t2, const SampleGate& sgin,
				bool alpick ) const
{
    double acorr1 = 0, acorr2 = 0, ccorr = 0;
    float val1, val2;
    SampleGate sg( sgin ); sg.sort();
    if ( !alpick
      && (sg.start<0 || sg.stop>=trc.size()
		     || sg.stop>=t2.size()) )
	return mUdf(double);

    float p1 = trc.info().pick;
    float p2 = t2.info().pick;
    if ( alpick && (Values::isUdf(p1) || Values::isUdf(p2)) )
	return mUdf(double);

    for ( int idx=sg.start; idx<=sg.stop; idx++ )
    {
	val1 = alpick ? trc.getValue(p1+idx*trc.info().sampling.step,curcomp)
		      : trc.get( idx, curcomp );
	val2 = alpick ? t2.getValue(p2+idx*t2.info().sampling.step,curcomp)
			: t2.get( idx, curcomp );
	acorr1 += val1 * val1;
	acorr2 += val2 * val2;
	ccorr += val1 * val2;
    }
 
    return ccorr / sqrt( acorr1 * acorr2 );
}


double SeisTrcPropCalc::dist( const SeisTrc& t2, const SampleGate& sgin,
				bool alpick ) const
{
    float val1, val2;
    double sqdist = 0, sq1 = 0, sq2 = 0;
    SampleGate sg( sgin ); sg.sort();
    if ( !alpick
      && (sg.start<0 || sg.stop>=trc.size()
		     || sg.stop>=t2.size()) )
	return mUdf(double);

    float p1 = trc.info().pick;
    float p2 = t2.info().pick;
    if ( alpick && (Values::isUdf(p1) || Values::isUdf(p2)) )
	return mUdf(double);

    for ( int idx=sg.start; idx<=sg.stop; idx++ )
    {
	val1 = alpick ? trc.getValue(p1+idx*trc.info().sampling.step,curcomp)
		      : trc.get( idx, curcomp );
	val2 = alpick ? t2.getValue(p2+idx*t2.info().sampling.step,curcomp)
		      : t2.get( idx, curcomp );
	sq1 += val1 * val1;
	sq2 += val2 * val2;
	sqdist += (val1-val2) * (val1-val2);
    }
    if ( sq1 + sq2 < 1e-10 ) return 0;
    return 1 - (sqrt(sqdist) / (sqrt(sq1) + sqrt(sq2)));
}
