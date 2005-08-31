/*
___________________________________________________________________

 * COPYRIGHT: (C) dGB Beheer B.V.
 * AUTHOR   : K. Tingdahl
 * DATE     : Nov 2003
___________________________________________________________________

-*/

static const char* rcsID = "$Id: horizonadjuster.cc,v 1.11 2005-08-31 03:08:20 cvsduntao Exp $";

#include "horizonadjuster.h"

#include "attribpsc.h"
#include "emhorizon.h"
#include "genericnumer.h"
#include "iopar.h"
#include "linear.h"
#include "survinfo.h"

#include <math.h>

namespace MPE {

const char* HorizonAdjuster::permzrgstr_ = "Permitted Z range";
const char* HorizonAdjuster::ampthresholdstr_ = "Value threshhold";
const char* HorizonAdjuster::similaritywinstr_ = "Similarity window";
const char* HorizonAdjuster::similaritythresholdstr_ = "Similarity threshhold";
const char* HorizonAdjuster::trackbyvalstr_ = "Track by value";
const char* HorizonAdjuster::trackeventstr_ = "Track event";

HorizonAdjuster::HorizonAdjuster( EM::Horizon& hor,
	const EM::SectionID& sid )
    : SectionAdjuster( sid )
    , horizon_( hor )
{
    computers_ += new AttribPositionScoreComputer();

    float dist = 8 * SI().zStep();
    permzrange_ = Interval<float>(-dist,dist);
    dist /= 2;
    similaritywin_ = Interval<float>(-dist, dist);
    
    ampthreshold_ = mUndefValue;
    similaritythreshold_ = 0.75;
    trackbyvalue_ = true;
    trackevent_ = VSEvent::Max;
}


int HorizonAdjuster::nextStep()
{
    initTrackParam();
    horizon_.geometry.checkSupport(false);
    int count = 0;
    for ( int idx=0; idx<pids_.size(); idx++ )
    {
	BinID bid;	bid.setSerialized( pids_[idx] );
	BinID refbid;
	if ( refpos_ )
	    refbid.setSerialized(*refpos_);
	else if ( pidsrc_.size() > idx )
	    refbid.setSerialized( pidsrc_[idx] );
	else
	    refbid = bid;
	
	float targetz;
 	if ( trackTrace(refbid, bid, targetz, 0) )
	    ++count, setHorizonPick(bid, targetz);
	else if ( !extrapolateonfail_ )
	    setHorizonPick(bid, mUndefValue);
    }
    horizon_.geometry.checkSupport(true);

#ifdef __debug__
    BufferString msg( "Tracked horizon picks: " ); msg += count;
    ErrMsg( msg );
#endif
    return 0;
}


bool HorizonAdjuster::trackTrace( const BinID& refbid,
				 const BinID& targetbid, float& targetz, 
				 float* refsamples )
{
    const Coord3 pos = horizon_.geometry.getPos(sectionid_,targetbid);
    if ( !pos.isDefined() )	return false;
    targetz = pos.z;

    const AttribPositionScoreComputer* acomp = getAttribComputer();
    const PositionScoreComputerAttribData* attrdata = acomp->getAttribData();
    const HorSampling hrg = attrdata->getCubeSampling(0).hrg;
    const StepInterval<float> zrg = attrdata->getCubeSampling(0).zrg;
    if ( !zrg.includes(targetz) )
	return false;

    float smplbuf[matchwinsamples_+2];
    if ( ! refsamples ) {
	BinID srcbid(refbid.inl, refbid.crl);
	if ( srcbid.inl == -1 && srcbid.crl == -1 )
	    srcbid = targetbid;
	if ( !hrg.includes(srcbid) || !hrg.includes(srcbid) )
	    return false;
	if ( ! getCompSamples(attrdata, srcbid,
	    	horizon_.geometry.getPos(sectionid_,srcbid).z, smplbuf ) )
	    return false;
	refsamples = smplbuf;
    }

    Interval<double> permrange( targetz+permzrange_.start,
	    			targetz+permzrange_.stop );

    if ( permrange.start<zrg.start )	permrange.start = zrg.start;
    if ( permrange.stop>zrg.stop )	permrange.stop = zrg.stop;
    if ( permrange.start>permrange.stop || !permrange.includes(targetz) )
	return false;
    
    const int nrcubesamples = zrg.nrSteps();
    float trcbuffer[zrg.nrSteps() + 2 ];
    const Interval<int> samplerg( mMAX(zrg.nearestIndex(permrange.start),0),
			mMIN(zrg.nearestIndex(permrange.stop),nrcubesamples-1));
    if ( ! attrdata->getValueBySample( 0, targetbid, trcbuffer,
				       samplerg.start, samplerg.stop) )
    	return false;
    const int nrsamples = samplerg.stop - samplerg.start + 1;
    int refpos = zrg.nearestIndex(targetz)-samplerg.start;
    
    if ( trackbyvalue_ )
    {
	float evpos = adjoiningEventPosByValue(trackevent_, trcbuffer,
		nrsamples, refpos, *refsamples);
	if ( Values::isUdf(evpos) )
	    return false;
	targetz = zrg.atIndex(samplerg.start);
	targetz += (evpos*zrg.step);
	return true;
    }
    else {
	int upmatchpos, downmatchpos;  upmatchpos = downmatchpos = -1;
	float upmatchv, downmatchv;    bool upeq, downeq;
    
	Interval<double> matchrg( targetz+similaritywin_.start,
				  targetz+similaritywin_.stop );
	if ( !( permrange.includes(matchrg.start)
	     && permrange.includes(matchrg.stop) ) )
	    return false; 

	int stsmpl = zrg.nearestIndex(matchrg.start)-samplerg.start;
 	upmatchpos = matchingSampleBySimilarity( trcbuffer, stsmpl,
			    0, refsamples, upmatchv, upeq );
	downmatchpos = matchingSampleBySimilarity( trcbuffer, stsmpl+1,
			    nrsamples - matchwinsamples_,
			    refsamples, downmatchv, downeq );

	int matchpos = -1;
	if ( upmatchpos!=-1 &&  downmatchpos!=-1 )
	{
	    if ( downmatchv == upmatchv && upeq && downeq )
		matchpos = ( upmatchpos + downmatchpos ) / 2;
	    else 
		matchpos = downmatchv>upmatchv ? downmatchpos : upmatchpos;
	}
	else if ( upmatchpos!=-1 )
	    matchpos = upmatchpos;
	else if ( downmatchpos!=-1 )
	    matchpos = downmatchpos;

	if ( matchpos == -1 )	    return false;
	// snap to event
	matchpos += simlaritymatchpt_;
	float evpos = adjoiningEventPosByValue(trackevent_, trcbuffer,
		nrsamples, matchpos, trcbuffer[matchpos]);
	if ( Values::isUdf(evpos) ) return false;
	targetz = zrg.atIndex(samplerg.start);
	targetz += (evpos*zrg.step);
	return true;
    }
}


float HorizonAdjuster::adjoiningEventPosByValue( VSEvent::Type ev, 
		const float* srctrc, int nrsamples, int refpos, float refval )
{
    if ( ev==VSEvent::ZC || ev==VSEvent::ZCNegPos
	 || ev==VSEvent::ZCPosNeg )
	return adjoiningZeroEventPos(ev, srctrc, nrsamples, refpos);
    else if ( ev == VSEvent::Max || ev == VSEvent::Min )
    {
	int upmatchpos, downmatchpos;  upmatchpos = downmatchpos = -1;
	float upmatchv, downmatchv;    bool upeq, downeq;

	upmatchpos = adjoiningExtremePos( ev, srctrc, refpos,
				0, refval, upmatchv, upeq );
	downmatchpos = adjoiningExtremePos( ev, srctrc, refpos+1,
				nrsamples-1, refval, downmatchv, downeq );
	
	int matchpos = -1;
	if ( upmatchpos!=-1 &&  downmatchpos!=-1 )
	{
	    if ( downmatchv == upmatchv && upeq && downeq )
		matchpos = ( upmatchpos + downmatchpos ) / 2;
	    else if ( ev == VSEvent::Max )
		matchpos = downmatchv>upmatchv ? downmatchpos : upmatchpos;
	    else
		matchpos = downmatchv<upmatchv ? downmatchpos : upmatchpos;
	}
	else if ( upmatchpos!=-1 )
	    matchpos = upmatchpos;
	else if ( downmatchpos!=-1 )
	    matchpos = downmatchpos;
    
	return matchpos == -1  ? mUndefValue
	       : exactExtremePos(ev, srctrc, nrsamples, matchpos);
    }
    else
    {
	pErrMsg("Event not handled");
	return mUndefValue;
    }
}


int HorizonAdjuster::adjoiningExtremePos( VSEvent::Type ev, const float* srctrc,
			int startsample, int endsample, float refval,
			float &matchval, bool& eqfromstart )
{
    const int tolerancesamples = 1;
    int matchpos = -1;
    eqfromstart = false;
    int step = startsample>endsample ? -1 : 1;
    
    int numsamples = endsample - startsample;
    if ( numsamples<0 )	numsamples = -numsamples;
    numsamples++;
    
    const float alloweddev = fabs( refval * 0.20 );
    float prevdev = alloweddev;
    float prevval = srctrc[startsample];
    
    int smpl = startsample;
    int eqsamples = 0;
    for ( int idx=0; idx<numsamples; idx++, smpl += step )
    {
	float sampleval = srctrc[smpl];
	if ( Values::isUdf(sampleval) )	    break;
	float dev =  fabs( sampleval - refval );
	bool matchfail = false;
	if ( ev == VSEvent::Min )
	{
	    if ( !Values::isUdf(ampthreshold_) && sampleval>ampthreshold_ )
		matchfail = true;;
	    if ( Values::isUdf(ampthreshold_) && sampleval>refval
		&& sampleval - refval>alloweddev )
		matchfail = true;;
	    if ( sampleval>prevval )
		matchfail = true;;
	}
	else
	{
	    if ( !Values::isUdf(ampthreshold_) && sampleval<ampthreshold_ )
		matchfail = true;;
	    if ( Values::isUdf(ampthreshold_) && sampleval<refval
		&& refval - sampleval>alloweddev )
		matchfail = true;;
	    if ( sampleval<prevval )
		matchfail = true;;
	}
	if (matchfail) 
	{
	    if ( matchpos == -1 && idx<tolerancesamples )
		continue;
	    else
		break;
	}
	
	if ( idx == 0 || prevval != sampleval )
	{
	    prevval = sampleval;
	    prevdev = dev;
	    matchpos = smpl;
	    matchval = sampleval;
	    eqsamples = 0;
	}
	else if ( idx && prevval == sampleval )
	    eqsamples++;
    }
    if ( matchpos != -1 && eqsamples )
    {
	eqfromstart = (eqsamples == (matchpos - startsample + 1));
	matchpos += ( eqsamples / 2 * step );
    }
    return matchpos;
}


int HorizonAdjuster::matchingSampleBySimilarity( const float* srctrc,
			int startsample, int endsample, const float* refval,
			float &matchratio, bool& eqfromstart )
{
    const int noiselevel = 1;	int noise = 0;
    int matchpos = -1;    eqfromstart = false;

    int eqsamples = 0;    float prevratio = 0;
    int step = startsample>endsample ? -1 : 1;
    for ( int smpl=startsample;  ; smpl += step )
    {
	if ( step==1 && smpl>endsample || step==-1 && smpl<endsample )
	    break;
	
	float curratio = similarity<const float*, const float*>(refval, srctrc,
				matchwinsamples_, false, 0, smpl );
	if ( curratio<prevratio )
	{
	    if ( noise < noiselevel ) { ++noise;    continue; }
	    else			break;
	}
	
	if ( smpl==startsample || prevratio != curratio )
	{
	    matchpos = smpl;
	    matchratio = prevratio = curratio;
	    eqsamples = 0;
	}
	else if ( smpl!=startsample && prevratio == curratio )
	    eqsamples++;
	noise = 0;
    }
    if ( matchpos != -1 && eqsamples )
    {
	eqfromstart = (eqsamples == (matchpos - startsample + 1));
	matchpos += ( eqsamples / 2 * step );
    }
    return matchratio>=similaritythreshold_ ? matchpos : -1;
}


float HorizonAdjuster::adjoiningZeroEventPos( VSEvent::Type ev,
		const float *srctrc, int nrsamples, int startpos )
{
    if ( !(ev==VSEvent::ZC || ev==VSEvent::ZCNegPos || ev==VSEvent::ZCPosNeg) )
	return mUndefValue;
    
    float dwpos = firstZeroEventPos( ev, srctrc, startpos, nrsamples - 1 );
    float uppos = firstZeroEventPos( ev, srctrc, startpos, 0 );

    float matchpos = mUndefValue;
    if ( !Values::isUdf(uppos) && !Values::isUdf(dwpos) )
	matchpos = (dwpos-startpos<startpos-uppos) ? dwpos : uppos;
    else if ( !Values::isUdf(uppos) && Values::isUdf(dwpos) )
	matchpos = uppos;
    else if ( Values::isUdf(uppos) && !Values::isUdf(dwpos) )
	matchpos = dwpos;
    
    return matchpos;
}


float HorizonAdjuster::firstZeroEventPos( VSEvent::Type ev, const float *srctrc,
					  int startsample, int endsample )
{
    int step = startsample>endsample ? -1 : 1;
    for ( int smpl=startsample+step;  ; smpl += step )
    {
	if ( step==1 && smpl>endsample || step==-1 && smpl<endsample )
	    break;
	
	float prev, cur;
	if ( step > 0 )
	    prev = srctrc[smpl-step],	cur  = srctrc[smpl];
	else
	    prev = srctrc[smpl],	cur  = srctrc[smpl-step];
	if ( prev==0 && cur==0 )	continue;
	
	bool zeroev = false;
	if ( prev<=0 && cur>0 || prev==0 && cur>0 )
	{
	    if ( ev==VSEvent::ZCNegPos || ev==VSEvent::ZC )
		zeroev =  true;
	    if ( ev == VSEvent::ZCPosNeg )
		break;
	}
	if ( prev>=0 && cur<0 || prev==0 && cur<0 )
	{
	    if ( ev == VSEvent::ZCPosNeg || ev==VSEvent::ZC )
		zeroev = true;
	    if ( ev == VSEvent::ZCNegPos )
		break;
	}
	
	if ( zeroev )
	{
	    float pos = step == 1 ? smpl-1 : smpl;
	    pos += ( -prev/(cur-prev) );
	    return pos;
	}

	if ( ev==VSEvent::ZCNegPos && prev > cur 
	     || ev==VSEvent::ZCPosNeg && prev < cur )
	    break;
    }
    return mUndefValue;
}


float HorizonAdjuster::exactExtremePos( VSEvent::Type ev, const float *smplbuf,
				        int nrsamples, int pickpos )
{
    if ( nrsamples < 4 || pickpos<1 || pickpos>=nrsamples-2 )
	return pickpos;
    ThirdOrderPoly poly;
    if ( ev == VSEvent::Min )
	poly.setFromSamples( -smplbuf[pickpos-1], -smplbuf[pickpos],
    		 -smplbuf[pickpos+1], -smplbuf[pickpos+2]);
    else
	poly.setFromSamples( smplbuf[pickpos-1], smplbuf[pickpos],
    		 smplbuf[pickpos+1], smplbuf[pickpos+2]);
    float extremepos, extremepos0, extremepos1;
    poly.getExtremePos( extremepos0, extremepos1 );
    PtrMan<SecondOrderPoly> firstder = poly.createDerivative();
    PtrMan<FloatMathFunction> secder = firstder->createDerivative();

    if ( secder->getValue(extremepos0)<0 )
	extremepos = extremepos0;
    else if ( secder->getValue(extremepos1)<0 )
	extremepos = extremepos1;

    if ( Values::isUdf(extremepos) || extremepos>=1 || extremepos<=-1 )
	return pickpos;
    
    return pickpos+extremepos;
}


void HorizonAdjuster::initTrackParam()
{
    const AttribPositionScoreComputer* acomp = getAttribComputer();
    
    if (trackbyvalue_)
    {
	matchwinsamples_ = 1;
	simlaritymatchpt_ = 0;
    }
    else {
	const PositionScoreComputerAttribData* attrdata = acomp->getAttribData();
	const StepInterval<float> zrg = attrdata->getCubeSampling(0).zrg;
	matchwinsamples_ = (int)( (similaritywin_.stop - similaritywin_.start)
    				  / zrg.step ) + 1;
	simlaritymatchpt_ = (int)(-similaritywin_.start / zrg.step);
    }
}


const AttribPositionScoreComputer* 
    HorizonAdjuster::getAttribComputer() const
{
    for ( int idx=0; idx<nrComputers(); idx++ )
    {
	mDynamicCastGet(const AttribPositionScoreComputer*,attr,
			getComputer(idx));
	if ( attr ) return attr;
    }

    return 0;
}


void HorizonAdjuster::setHorizonPick(const BinID&  bid, float val )
{
    horizon_.geometry.setPos( sectionid_, bid, Coord3(0,0,val), true );
}


bool HorizonAdjuster::getCompSamples(
	const PositionScoreComputerAttribData*	attrdata,
	const BinID& bid, float z, float* buf )
{
    float *smplbuf = buf;
    if ( trackbyvalue_ )
    {
	const StepInterval<float> zrg = attrdata->getCubeSampling(0).zrg;
	int stsmpl = zrg.nearestIndex(z);
	*smplbuf = attrdata->getValueBySample( 0, bid, stsmpl );
	return ! Values::isUdf(*smplbuf);
    }
    else
    {
	const StepInterval<float> zrg = attrdata->getCubeSampling(0).zrg;
	int stsmpl = zrg.nearestIndex(z+similaritywin_.start);
	return attrdata->getValueBySample(0, bid, smplbuf, stsmpl,
					  stsmpl + matchwinsamples_ - 1);
    }
}


void HorizonAdjuster::fillPar( IOPar& iopar ) const
{
    SectionAdjuster::fillPar( iopar );
    int event = trackevent_;
    iopar.set( trackeventstr_, event );
    iopar.set( permzrgstr_, permzrange_.start, permzrange_.stop );
    iopar.set( ampthresholdstr_, ampthreshold_ );
    iopar.set( similaritywinstr_, similaritywin_.start, similaritywin_.stop );
    iopar.set( similaritythresholdstr_, similaritythreshold_ );
    int byval = trackbyvalue_;
    iopar.set( trackbyvalstr_, byval );
}


bool HorizonAdjuster::usePar( const IOPar& iopar )
{
    SectionAdjuster::usePar( iopar );
    int event;
    iopar.get( trackeventstr_,  event);
    trackevent_ = (VSEvent::Type)event;
    iopar.get( permzrgstr_, permzrange_.start, permzrange_.stop );
    iopar.get( ampthresholdstr_, ampthreshold_ );
    iopar.get( similaritywinstr_, similaritywin_.start, similaritywin_.stop );
    iopar.get( similaritythresholdstr_, similaritythreshold_ );
    int byval;
    iopar.get( trackbyvalstr_,  byval);
    trackbyvalue_ = byval;
    return true;
}


}; // namespace MPE
