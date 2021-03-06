#ifndef volprocregionfiller_h
#define volprocregionfiller_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Yuancheng Liu
 Date:		November 2007
 RCS:		$Id: volprocregionfiller.h 36409 2014-09-13 21:11:44Z kristofer.tingdahl@dgbes.com $
________________________________________________________________________


-*/

#include "volumeprocessingmod.h"
#include "volprocstep.h"
#include "multiid.h"

namespace EM { class Region3D; }

namespace VolProc
{

/*!\brief Region filler */

mExpClass(VolumeProcessing) RegionFiller : public Step
{ mODTextTranslationClass(RegionFiller);
public:

				mDefaultFactoryInstantiation( VolProc::Step,
				    RegionFiller, "RegionFiller",
				    tr("Region painter") )

				RegionFiller();
				~RegionFiller();
    virtual void		releaseData();

    void			setInsideValue(float);
    float			getInsideValue() const;
    void			setOutsideValue(float);
    float			getOutsideValue() const;

    float			getStartValue() const;
    const MultiID&		getStartValueHorizonID() const;
    int				getStartValueAuxDataIdx() const;
    float			getGradientValue() const;
    const MultiID&		getGradientHorizonID() const;
    int				getGradientAuxDataIdx() const;

    EM::Region3D&		region();
    const EM::Region3D&		region() const;

    virtual void		fillPar(IOPar&) const;
    virtual bool		usePar(const IOPar&);

    virtual bool		needsFullVolume() const		{ return false;}
    virtual bool		canInputAndOutputBeSame() const	{ return false;}
    virtual bool		areSamplesIndependent() const	{ return true;}
    virtual bool		needsInput() const		{ return false;}
    virtual bool		isInputPrevStep() const		{ return true; }
    virtual bool		prefersBinIDWise() const	{ return true; }

protected:

    virtual bool		prepareComp(int nrthreads);
    virtual bool		computeBinID(const BinID&,int);
    virtual od_int64		extraMemoryUsage(OutputSlotID,
						const TrcKeySampling&,
						const StepInterval<int>&) const;

    EM::Region3D&		region_;
    float			insideval_;
    float			outsideval_;

    float			startval_;
    float			gradval_;
    MultiID			startvalkey_;
    MultiID			gradvalkey_;
    int				startvalauxidx_;
    int				gradvalauxidx_;

};

} // namespace VolProc

#endif
