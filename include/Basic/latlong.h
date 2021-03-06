#ifndef latlong_h
#define latlong_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bert
 Date:		2008
 Contents:	Geographics lat/long <-> Coord transform (an estimate)
________________________________________________________________________

-*/
 
#include "basicmod.h"
#include "coord.h"


/*!
\brief Geographical coordinates, decimal but with conv to deg, min, sec.
*/

mExpClass(Basic) LatLong
{
public:
    			LatLong( double la=0, double lo=0 )
			    : lat_(la), lng_(lo)  {}
    			LatLong( const Coord& c ) { *this = transform(c);}
			operator Coord() const	  { return transform(*this); }

    static Coord	transform(const LatLong&); //!< Uses SI()
    static LatLong	transform(const Coord&);   //!< Uses SI()

    const char*		toString() const;
    bool		fromString(const char*);

    void		getDMS(bool lat,int&,int&,float&) const;
    void		setDMS(bool lat,int,int,float);

    double		lat_;
    double		lng_;

};


/*!
\brief Estimates to/from LatLong coordinates.

  Needs both survey coordinates and lat/long for an anchor point in the survey.
*/

mExpClass(Basic) LatLong2Coord
{
public:

			LatLong2Coord();
			LatLong2Coord(const Coord&,const LatLong&);
    bool		isOK() const	{ return !mIsUdf(lngdist_); }

    void		set(const LatLong&,const Coord&);

    LatLong		transform(const Coord&) const;
    Coord		transform(const LatLong&) const;

    const char*		toString() const;
    bool		fromString(const char*);

    Coord		refCoord() const	{ return refcoord_; }
    LatLong		refLatLong() const	{ return reflatlng_; }

protected:

    Coord		refcoord_;
    LatLong		reflatlng_;

    double		latdist_;
    double		lngdist_;
    double		scalefac_;
};


#endif
