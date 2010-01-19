#ifndef probdenfunc_h
#define probdenfunc_h

/*
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bert
 Date:		Jan 2010
 RCS:		$Id: probdenfunc.h,v 1.2 2010-01-19 12:06:40 cvsbert Exp $
________________________________________________________________________


*/

#include "bufstring.h"
#include "ranges.h"
template <class T> class TypeSet;


mClass ProbDenFunc
{
public:

    virtual		~ProbDenFunc()			{}

    virtual int		nrDims() const			= 0;
    virtual const char*	dimName(int dim) const		= 0;
    virtual float	value(const TypeSet<float>&) const = 0;

    virtual float	normFac() const			{ return 1; }
			//!< factor to get 'true' normalized probabilities
};


mClass ProbDenFunc1D : public ProbDenFunc
{
public:

    virtual int		nrDims() const		{ return 1; }
    virtual const char*	dimName(int) const	{ return varName(); }

    virtual float	value(float) const	= 0;
    virtual const char*	varName() const		{ return varnm_; }

    virtual float	value( const TypeSet<float>& v ) const
						{ return value(v[0]); }

    BufferString	varnm_;

protected:

    			ProbDenFunc1D( const char* vnm )
			    : varnm_(vnm)	{}
    ProbDenFunc1D&	operator =(const ProbDenFunc1D&);

};


mClass ProbDenFunc2D : public ProbDenFunc
{
public:

    virtual int		nrDims() const			{ return 2; }
    virtual const char*	dimName(int) const;

    virtual float	value(float,float) const	= 0;
    virtual float	value( const TypeSet<float>& v ) const
			{ return value(v[0],v[1]); }

    BufferString	dim0nm_;
    BufferString	dim1nm_;

protected:

    			ProbDenFunc2D( const char* vnm0, const char* vnm1 )
			    : dim0nm_(vnm0), dim1nm_(vnm1)	{}
    ProbDenFunc2D&	operator =(const ProbDenFunc2D&);

};


#endif
