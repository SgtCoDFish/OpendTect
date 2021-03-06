/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bert
 Date:          Aug 2008
________________________________________________________________________

-*/


#include "poly2horvol.h"

#include "emmanager.h"
#include "emhorizon3d.h"
#include "emsurfacetr.h"
#include "executor.h"
#include "pickset.h"
#include "survinfo.h"
#include "polygon.h"
#include "gridder2d.h"

#include <math.h>


Poly2HorVol::~Poly2HorVol()
{
    if ( hor_ )
	hor_->unRef();
}


void Poly2HorVol::setHorizon( EM::Horizon3D* hor )
{
    if ( hor_ )
	{ hor_->unRef(); hor_ = 0; }
    hor_ = hor;
    if ( hor_ )
	hor_->ref();
}


bool Poly2HorVol::setHorizon( const MultiID& mid, TaskRunner* tr )
{
    if ( hor_ )
	{ hor_->unRef(); hor_ = 0; }

    EM::EMObject* emobj = EM::EMM().loadIfNotFullyLoaded( mid, tr );
    mDynamicCastGet(EM::Horizon3D*,hor,emobj)

    setHorizon( hor );
    return hor_;
}


#define mPolyLoc(b) \
	Geom::Point2D<float>( (float)b.inl(), (float)(b.crl()) )

float Poly2HorVol::getM3( float vel, bool upw, bool useneg )
{
    if ( !hor_ || ! ps_ )
	return mUdf(float);

    ODPolygon<float> poly;
    TrcKeySampling hs;
    TypeSet<Coord> pts; TypeSet<float> zvals;
    for ( int idx=0; idx<ps_->size(); idx++ )
    {
	const Pick::Location& pl = (*ps_)[idx];
	if ( !pl.hasPos() )
	    continue;

	pts += pl.pos(); zvals += (float) pl.z();
	const BinID& bid = pl.binID();
	poly.add( mPolyLoc(bid) );
	if ( idx )
	    hs.include( bid );
	else
	    hs.start_ = hs.stop_ = bid;
    }

    TriangulatedGridder2D grdr;
    grdr.setPoints( pts );
    grdr.setValues( zvals );
    float avgz = 0.f;
    for ( int idx=0; idx<zvals.size(); idx++ )
	avgz += zvals[idx];

    if ( !zvals.isEmpty() )
	avgz /= mCast(float,zvals.size());

    const int nrsect = hor_->nrSections();
    TrcKeySamplingIterator iter( hs );
    BinID bid; float totth = 0;
    while ( iter.next(bid) )
    {
	if ( !poly.isInside(mPolyLoc(bid),true,1e-6) )
	    continue;

	const EM::SubID subid = bid.toInt64();
	const Coord pos( hs.toCoord(bid) );

	for ( int isect=0; isect<nrsect; isect++ )
	{
	    const EM::SectionID sid = hor_->sectionID( isect );
	    float horz = (float) hor_->getPos( sid, subid ).z;
	    if ( mIsUdf(horz) && bid.inl()!=hs.stop_.inl() &&
		 bid.crl()!=hs.stop_.crl() )
	    { //The very last edges should exclude.
		horz = (float) hor_->geometry().sectionGeometry(sid)
		    ->computePosition( pos ).z;
	    }

	    if ( mIsUdf(horz) )
		continue;

	    float polyz = grdr.getValue( pos );
	    if ( mIsUdf(polyz) )
		polyz = avgz;

	    const float th = upw ? polyz - horz : horz - polyz;
	    if ( useneg || th > 0 )
		{ totth += th; break; }
	}
    }

    const float cellarea = SI().inlDistance() * hs.step_.inl()
			 * SI().crlDistance() * hs.step_.crl();
    const float v = SI().zIsTime() ? vel * .5f : 1; // TWT
    return cellarea * v * totth;
}


const char* Poly2HorVol::dispText( float m3, bool zinft )
{
    const float bblconv = 6.2898108;
    const float ft3conv = 35.314667;

    if ( mIsUdf(m3) )
	return "";

    float dispval = m3;
    if ( zinft ) dispval *= ft3conv;
    bool mega = false;
    if ( fabs(dispval) > 1e6 )
	{ mega = true; dispval /= 1e6; }

    mDeclStaticString( txt );
    txt = dispval; txt += mega ? "M " : " ";
    txt += zinft ? "ft^3" : "m^3";
    txt += " (";
    dispval *= bblconv;
    if ( zinft ) dispval /= ft3conv;
    if ( dispval > 1e6 )
	{ mega = true; dispval /= 1e6; }
    txt += dispval; if ( mega ) txt += "M";
    txt += " bbl)";

    return txt.buf();
}
