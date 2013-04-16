#ifndef prestackprop_h
#define prestackprop_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	A.H. Bril
 Date:		Jan 2008
 RCS:		$Id$
________________________________________________________________________

-*/

#include "prestackprocessingmod.h"
#include "stattype.h"
#include "enums.h"
#include "ranges.h"
#include "datapack.h"

class BinID;
class SeisTrcBuf;
class SeisPSReader;

namespace PreStack
{

class Gather;

/*!
\brief Calculates 'post-stack' properties of a PreStack data store.
*/

mExpClass(PreStackProcessing) PropCalc
{
public:

    enum CalcType	{ Stats, LLSQ };
    			DeclareEnumUtils(CalcType)
    enum AxisType	{ Norm, Log, Exp, Sqr, Sqrt, Abs, Sinsq };
    			DeclareEnumUtils(AxisType)
    enum LSQType	{ A0, Coeff, AngleA0, AngleCoeff, StdDevA0, StdDevCoeff, 
		          CorrCoeff };
    			DeclareEnumUtils(LSQType)

    mExpClass(PreStackProcessing) Setup
    {
    public:
			Setup()
			    : calctype_(Stats)
			    , stattype_(Stats::Average)
			    , lsqtype_(A0)
			    , offsaxis_(Norm)
			    , valaxis_(Norm)
			    , offsrg_(0,mUdf(float))
			    , useazim_(false)
			    , aperture_(0)  {}

	mDefSetupMemb(CalcType,calctype)
	mDefSetupMemb(Stats::Type,stattype)
	mDefSetupMemb(LSQType,lsqtype)
	mDefSetupMemb(AxisType,offsaxis)
	mDefSetupMemb(AxisType,valaxis)
	mDefSetupMemb(Interval<float>,offsrg)
	mDefSetupMemb(bool,useazim)
	mDefSetupMemb(int,aperture)
    };
			PropCalc(const Setup&);
    virtual		~PropCalc();

    Setup&		setup()				{ return setup_; }
    const Setup&	setup() const			{ return setup_; }
    const Gather*	getGather() const               { return gather_; }

    void		setGather(DataPack::ID);
    void		setAngleData(DataPack::ID);
			/*!< Only used if AngleA0 or AngleCoeff. If not set,
			     offset values from traces will be assumed to 
			     contain angles. */
    float		getVal(int sampnr) const;
    float		getVal(float z) const;

    static float	getVal( const PropCalc::Setup& su,
			        TypeSet<float>& vals, TypeSet<float>& offs );

protected:


    void		removeGather();

    Gather*		gather_;
    int*		innermutes_;
    int*		outermutes_;
    Gather*		angledata_;

    Setup		setup_;
};

}; //Namespace


#endif

