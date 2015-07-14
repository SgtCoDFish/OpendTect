#ifndef volprocbodyfiller_h
#define volprocbodyfiller_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Yuancheng Liu
 Date:		November 2007
 RCS:		$Id$
________________________________________________________________________


-*/

#include "volumeprocessingmod.h"
#include "volprocchain.h"
#include "arrayndimpl.h"
#include "trckeyzsampling.h"

namespace EM { class EMObject; class Body; class ImplicitBody; }

namespace VolProc
{

class Step;

/*!
\brief Body filler
*/

mExpClass(VolumeProcessing) BodyFiller : public Step
{
public:
	mDefaultFactoryCreatorImpl( VolProc::Step, BodyFiller );
	mDefaultFactoryInstanciationBase( "BodyFiller", "Body shape painter" );

				BodyFiller();
				~BodyFiller();

    bool			needsInput() const		{ return false;}
    bool			isInputPrevStep() const		{ return true; }
    bool			areSamplesIndependent() const	{ return true; }

    void			fillPar(IOPar&) const;
    bool			usePar(const IOPar&);

    void			releaseData();
    bool			canInputAndOutputBeSame() const { return true; }
    bool			needsFullVolume() const		{ return false;}

    enum ValueType		{ Constant, PrevStep, Undefined };

    void			setInsideValueType(ValueType);
    ValueType			getInsideValueType() const;
    void			setOutsideValueType(ValueType);
    ValueType			getOutsideValueType() const;

    void			setInsideValue(float);
    float			getInsideValue() const;
    void			setOutsideValue(float);
    float			getOutsideValue() const;

    bool			setSurface(const MultiID&);
    MultiID			getSurfaceID() { return mid_; }
    Task*			createTask();

    static const char*		sKeyOldType();

protected:

    bool			prefersBinIDWise() const	{ return true; }
    bool			prepareComp(int nrthreads)	{ return true; }
    bool			computeBinID(const BinID&,int);
    bool			getFlatPlgZRange(const BinID&,
						 Interval<double>& result);

    EM::Body*			body_;
    EM::EMObject*		emobj_;
    EM::ImplicitBody*		implicitbody_;
    MultiID			mid_;

    ValueType			insidevaltype_;
    ValueType			outsidevaltype_;
    float			insideval_;
    float			outsideval_;

				//For flat body_ only, no implicitbody_.
    TrcKeyZSampling		flatpolygon_;
    TypeSet<Coord3>		plgknots_;
    TypeSet<Coord3>		plgbids_;
    char			plgdir_;
				/* inline=0; crosline=1; z=2; other=3 */
    double			epsilon_;

    static const char*		sKeyOldMultiID();
    static const char*		sKeyOldInsideOutsideValue();

    static const char*		sKeyMultiID();
    static const char*		sKeyInsideType();
    static const char*		sKeyOutsideType();
    static const char*		sKeyInsideValue();
    static const char*		sKeyOutsideValue();

};

} // namespace VolProc

#endif

