/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Helene Payraudeau
 Date:		February 2005
________________________________________________________________________

-*/
static const char* rcsID = "$Id: eventattrib.cc,v 1.30 2009-05-18 10:33:38 cvshelene Exp $";

#include "eventattrib.h"
#include "survinfo.h"
#include "attribdataholder.h"
#include "attribdesc.h"
#include "attribfactory.h"
#include "attribparam.h"
#include "genericnumer.h"
#include "valseriesinterpol.h"

#define SGWIDTH 1000

namespace Attrib
{

mAttrDefCreateInstance(Event)
    
void Event::initClass()
{
    mAttrStartInitClassWithUpdate

    IntParam* evtype = new IntParam( eventTypeStr() );
    						//0 = Extr, 1 = Max, 2 = Min,
						 //3 = ZC, 4 = npZC, 5 = pnZC,
						 //6 = GateMax, 7 = GateMin
    evtype->setDefaultValue(0);
    desc->addParam( evtype );
        
    desc->addParam( new BoolParam(issingleeventStr()) );
    desc->addParam( new BoolParam(tonextStr()) );
    
    ZGateParam* gate = new ZGateParam(gateStr());
    gate->setLimits( Interval<float>(-1000,1000) );
    desc->addParam( gate );

    desc->addOutputDataType( Seis::UnknowData );

    InputSpec inputspec( "Input Data", true );
    desc->addInput( inputspec );

    mAttrEndInitClass
}


void Event::updateDesc( Desc& desc )
{
    const bool issingle = desc.getValParam(issingleeventStr())->getBoolValue();
    desc.setParamEnabled( eventTypeStr(), !issingle );

    VSEvent::Type evtype = VSEvent::None;
    if ( !issingle )
    {
	const int type = desc.getValParam(eventTypeStr())->getIntValue();
	evtype = Event::getEventType( type );
    }
    else
	desc.setNrOutputs( Seis::UnknowData, 3 );

    const bool enabgate =  !issingle && (evtype==VSEvent::GateMax ||
	    				 evtype==VSEvent::GateMin);
    desc.setParamEnabled( gateStr(), enabgate );
}


Event::Event( Desc& desc_ )
    : Provider( desc_ )
{
    if ( !isOK() ) return;

    mGetBool( issingleevent, issingleeventStr() );
    mGetBool( tonext, tonextStr() );

    gate.start = -SGWIDTH * SI().zStep();
    gate.stop = SGWIDTH * SI().zStep();

    eventtype = VSEvent::None;
    if ( !issingleevent )
    {
	const int type = desc_.getValParam(eventTypeStr())->getIntValue(0);
	eventtype = getEventType( type );

	if ( eventtype == VSEvent::GateMax || eventtype == VSEvent::GateMin )
	{
	    mGetFloatInterval( gate, gateStr() );
	    gate.scale( 1/zFactor() );
	}
    }
}


bool Event::getInputOutput( int input, TypeSet<int>& res ) const
{
    return Provider::getInputOutput( input, res );
}


bool Event::getInputData( const BinID& relpos, int zintv )
{
    inputdata = inputs[0]->getData( relpos, zintv );
    dataidx_ = getDataIndex( 0 );
    return inputdata;
}


const Interval<float>* Event::reqZMargin( int input, int output ) const
{
    return eventtype == VSEvent::GateMax || eventtype == VSEvent::GateMin 
	? &gate : 0;
}


const Interval<float>* Event::desZMargin( int input, int output ) const
{
    return eventtype != VSEvent::GateMax && eventtype != VSEvent::GateMin
	? &gate : 0;
}


VSEvent::Type Event::getEventType( int type )
{
    VSEvent::Type evtype;
    switch ( type )
    {
	case 0: evtype = VSEvent::Extr; break;
	case 1: evtype = VSEvent::Max; break;
	case 2: evtype = VSEvent::Min; break;
	case 3: evtype = VSEvent::ZC; break;
	case 4: evtype = VSEvent::ZCNegPos; break;
	case 5: evtype = VSEvent::ZCPosNeg; break;
	case 6: evtype = VSEvent::GateMax; break;
	case 7: evtype = VSEvent::GateMin; break;
	default: evtype = VSEvent::None; break;
    }

    return evtype;
}


ValueSeriesEvent<float,float> Event::findNextEvent( 
					ValueSeriesEvent<float,float> nextev, 
					int dir, VSEvent::Type evtype,
					int nrsamples, int z0 ) const
{
    ValueSeriesEvent<float,float> ev = nextev;
    SamplingData<float> sd;
    ValueSeriesEvFinder<float,float> vsevfinder( *(inputdata->series(dataidx_)),
	    					 z0+nrsamples, sd );
    Interval<float> sg;
    sg.start = ev.pos + dir;
    sg.stop = sg.start + dir*SGWIDTH;
    nextev = vsevfinder.find( evtype, sg, 1 );
    int nrloops = 0;
    while ( (int)(nextev.pos)==(int)(ev.pos) && ( !mIsUdf(nextev.pos) ) )
    {
	sg.start = ev.pos + dir * (2+nrloops);
	sg.stop = sg.start + dir * SGWIDTH;
	nextev = vsevfinder.find( evtype, sg, 1 );
	nrloops++;
    }
    return nextev;
}


void Event::singleEvent( const DataHolder& output, int nrsamples, int z0 ) const
{
    const int firstsample = z0 - inputdata->z0_;
    SamplingData<float> sd;
    ValueSeriesEvFinder<float,float> vsevfinder( *(inputdata->series(dataidx_)),
						 inputdata->nrsamples_, sd );
    VSEvent::Type zc = VSEvent::ZC;
    float extrasamp = output.extrazfromsamppos_/refstep;
    Interval<float> sg( firstsample+extrasamp, firstsample+extrasamp-SGWIDTH );
    ValueSeriesEvent<float,float> ev = vsevfinder.find( zc, sg, 1 );
    if ( mIsUdf(ev.pos) )
    {
	sg.stop = firstsample+output.extrazfromsamppos_ + SGWIDTH;
	ev = vsevfinder.find( zc, sg, 1 );
    }

    VSEvent::Type extr = VSEvent::Extr;
    ValueSeriesEvent<float,float> extrev;
    sg.start = ev.pos + 1;
    sg.stop = sg.start + SGWIDTH;
    if ( outputinterest[0] || outputinterest[2] )
	extrev = vsevfinder.find( extr, sg, 1 );

    ValueSeriesEvent<float,float> nextev = vsevfinder.find( zc, sg, 1 );
    for ( int idx=0; idx<nrsamples; idx++ )
    {
	const float cursample = firstsample + idx;
	if ( cursample < ev.pos )
	{
	    if ( outputinterest[0] ) setOutputValue( output, 0, idx, z0, 0 );
	    if ( outputinterest[1] ) setOutputValue( output, 1, idx, z0, 0 );
	    if ( outputinterest[2] ) setOutputValue( output, 2, idx, z0, 0 );
	}
	else if ( cursample > nextev.pos )
	{
	    ev = nextev;
	    nextev = findNextEvent( nextev, 1, zc, inputdata->nrsamples_,
		    		    firstsample );
	    if ( isOutputEnabled(0) || isOutputEnabled(2) )
	    {
		sg.start = ev.pos + 1;
		sg.stop = sg.start + SGWIDTH;
		extrev = vsevfinder.find( extr, sg, 1 );
	    }
	}

	if ( cursample >= ev.pos && cursample <= nextev.pos )
	{
	    if ( isOutputEnabled(0) )
	    {
		float val = mIsUdf( nextev.pos ) || mIsUdf( ev.pos ) ?
			      mUdf(float) : extrev.val/(nextev.pos-ev.pos);
		setOutputValue( output, 0, idx, z0, val );
	    }
	    if ( isOutputEnabled(1) )
	    {
		if ( mIsUdf( ev.pos ) ) 
		    setOutputValue( output, 1, idx, z0, mUdf(float) );
		else
		{
		    ValueSeriesInterpolator<float>
		    interp(inputdata->nrsamples_-1);
		    float lastsampval =  inputdata->getValue(dataidx_,
			    (inputdata->z0_+ev.pos-1)*refstep, refstep );
		    float nextsampval =  inputdata->getValue(dataidx_,
			    (inputdata->z0_+ev.pos+1)*refstep, refstep );
		    const float val = fabs( (nextsampval - lastsampval) / 2 );
		    setOutputValue( output, 1, idx, z0, val );
		}
	    }
	    if ( isOutputEnabled(2) )
	    {
		if ( mIsUdf( ev.pos ) ||  mIsUdf( extrev.pos ) 
		     || mIsUdf( nextev.pos ) ) 
		    setOutputValue( output, 2, idx, z0, mUdf(float) );
		else
		{
		    const float leftdist = extrev.pos - ev.pos;
		    const float rightdist = nextev.pos - extrev.pos;
		    const float val = (rightdist-leftdist)/(rightdist+leftdist);
		    setOutputValue( output, 2, idx, z0, val );
		}
	    }
	}
    }
}


void Event::multipleEvents( const DataHolder& output,
			    int nrsamples, int z0 ) const
{
    SamplingData<float> sd;
    ValueSeriesEvFinder<float,float> vsevfinder( *(inputdata->series(dataidx_)),
	    				         inputdata->nrsamples_, sd );
    const int firstsample = z0 - inputdata->z0_;
    const float extrasamp = output.extrazfromsamppos_/refstep;
    if ( eventtype == VSEvent::GateMax || eventtype == VSEvent::GateMin )
    {
	Interval<int> samplegate( mNINT(gate.start/refstep),
		                          mNINT(gate.stop/refstep) );
	int samplegatewidth = samplegate.width();
	int csample = firstsample + samplegate.start;
	Interval<float> sg(0,0);
	for ( int idx=0; idx<nrsamples; idx++ )
	{
	    const int cursample = csample + idx;
	    sg.start = cursample + extrasamp;
	    sg.stop = sg.start + samplegatewidth;
	    ValueSeriesEvent<float,float> ev = 
					vsevfinder.find( eventtype, sg, 1 );
	    if ( mIsUdf(ev.pos) )
		setOutputValue( output, 0, idx, z0, ev.pos );
	    else
	    {
		const float val = fabs( (firstsample + idx) - ev.pos) * refstep;
		setOutputValue( output, 0, idx, z0, val );
	    }
	}
	return;
    }

    Interval<float> sg(0,0);
    if ( nrsamples == 1 )
    {
	sg.start = firstsample + extrasamp;
	sg.stop = sg.start + SGWIDTH;
	ValueSeriesEvent<float,float> nextev = 
	    				vsevfinder.find( eventtype, sg, 1 );
	sg.stop = sg.start - SGWIDTH;
	ValueSeriesEvent<float,float> prevev = 
	    				vsevfinder.find( eventtype, sg, 1 );
	bool prevudf = mIsUdf(prevev.pos);
	bool nextudf = mIsUdf(nextev.pos);
	if ( prevudf && nextudf)
	    setOutputValue( output, 0, 0, z0, nextev.pos );
	else if ( prevudf || nextudf )
	{
	    const float val = fabs(firstsample - prevudf ? nextev.pos
		    					 : prevev.pos) *refstep;
	    setOutputValue( output, 0, 0, z0, val );
	}
	else
	{
	    if ( (nextev.pos - prevev.pos) < 1 )
	    {
		if ( tonext )
		{
		    sg.start -= 1;
		    sg.stop = sg.start - SGWIDTH;
		}
		else
		{
		    sg.start += 1;
		    sg.stop = sg.start + SGWIDTH;
		}
		ValueSeriesEvent<float,float> secondchance =
					vsevfinder.find( eventtype, sg, 1 );
		float val;
		if ( mIsUdf( secondchance.pos ) )
		    val = ( nextev.pos - prevev.pos ) * refstep;
		else if ( tonext )
		    val = ( nextev.pos - secondchance.pos ) * refstep;
		else
		    val = ( secondchance.pos - prevev.pos ) * refstep;

		setOutputValue( output, 0, 0, z0, val );	
	    }
	    else
		setOutputValue( output, 0, 0, z0, 
				( nextev.pos - prevev.pos ) * refstep );
	}
    }
    else
    {
	sg.start = tonext ? firstsample + extrasamp
	    		  : firstsample + extrasamp + nrsamples;
	int direction = tonext ? 1 : -1;
	sg.stop = sg.start -direction * SGWIDTH;
	ValueSeriesEvent<float,float> ev = vsevfinder.find( eventtype, sg, 1 );
	if ( mIsUdf(ev.pos) )
	{
	    sg.stop = firstsample + extrasamp + direction * SGWIDTH;
	    ev = vsevfinder.find( eventtype, sg, 1 );
	}
	ValueSeriesEvent<float,float> nextev = 
	    		findNextEvent( ev, direction, eventtype,
			       	       inputdata->nrsamples_, firstsample );

	if ( tonext )
	{
	    for ( int idx = 0; idx<nrsamples; idx++ )
	    {
		const int cursample = firstsample + idx;
		if ( cursample+extrasamp < ev.pos )
		    setOutputValue( output, 0, idx, z0, 0 );
		else if ( cursample+extrasamp > nextev.pos )
		{
		    ev = nextev;
		    nextev = findNextEvent( nextev, 1, eventtype,
			    		    inputdata->nrsamples_,
					    firstsample+extrasamp);
		}
		if ( cursample+extrasamp > ev.pos
			&& cursample+extrasamp < nextev.pos)
		{
		    if ( mIsUdf(nextev.pos) )
			setOutputValue( output, 0, idx, z0, nextev.pos );
		    else
		    {
			const float val = (nextev.pos - ev.pos) * refstep;	
			setOutputValue( output, 0, idx, z0, val );
		    }
		}
	    }
	}
	else
	{
	    for ( int idx=nrsamples-1; idx>=0; idx-- )
	    {
		const int cursample = firstsample + idx;
		if ( cursample > ev.pos )
		    setOutputValue( output, 0, idx, z0, 0 );
		else if ( cursample < nextev.pos )
		{
		    ev = nextev;
		    nextev = findNextEvent( nextev, -1, eventtype,
			    		    inputdata->nrsamples_,
					    firstsample+extrasamp);
		}
		if ( cursample+extrasamp < ev.pos
			&& cursample+extrasamp > nextev.pos)
		{
		    if ( mIsUdf(nextev.pos) )
			setOutputValue( output, 0, idx, z0, nextev.pos );
		    else
		    {
			const float val = (ev.pos - nextev.pos) * refstep;
			setOutputValue( output, 0, idx, z0, val );
		    }
		}
	    }
	}
    }
}

    
bool Event::computeData( const DataHolder& output, const BinID& relpos,
			 int z0, int nrsamples, int threadid ) const
{
    if ( !inputdata ) return false;

    if ( issingleevent )
        singleEvent( output, nrsamples, z0 );
    else
        multipleEvents( output, nrsamples, z0 );

    return true;
}

}; // namespace Attrib
