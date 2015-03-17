#ifndef horizonadjuster_h
#define horizonadjuster_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        K. Tingdahl
 Date:          January 2005
 RCS:           $Id$
________________________________________________________________________

-*/

#include "mpeenginemod.h"
#include "datapack.h"
#include "sectionadjuster.h"
#include "ranges.h"
#include "valseriesevent.h"

class EventTracker;
namespace EM { class Horizon; }

namespace MPE
{

class SectionExtender;

/*!
\brief SectionAdjuster to adjust EM::Horizon.
*/

mExpClass(MPEEngine) HorizonAdjuster : public SectionAdjuster
{
public:
			HorizonAdjuster(EM::Horizon&,EM::SectionID);
			~HorizonAdjuster();

    void		reset();
    int			nextStep();

    void		getNeededAttribs(
				ObjectSet<const Attrib::SelSpec>&) const;
    TrcKeyZSampling	getAttribCube(const Attrib::SelSpec&) const;

    void		setPermittedZRange(const Interval<float>& rg);
    Interval<float>	permittedZRange() const;
    void		setTrackByValue(bool yn);
    bool		trackByValue() const;
    void		setTrackEvent(VSEvent::Type ev);
    VSEvent::Type	trackEvent() const;

    void		setAmplitudeThreshold(float th);
    float		amplitudeThreshold() const;
    void		setAmplitudeThresholds(const TypeSet<float>& ats);
    TypeSet<float>&	getAmplitudeThresholds();
    void 		setAllowedVariance(float v);
    void		setAllowedVariances(const TypeSet<float>& avs);
    TypeSet<float>&	getAllowedVariances();
    float 		allowedVariance() const;
    void		setUseAbsThreshold(bool abs);
    bool		useAbsThreshold() const;

    void		setSimilarityWindow(const Interval<float>& rg);
    Interval<float>	similarityWindow() const;
    void		setSimilarityThreshold(float th);
    float		similarityThreshold() const;

    int			getNrAttributes() const;
    const Attrib::SelSpec* getAttributeSel(int idx) const;
    void		setAttributeSel(int idx,const Attrib::SelSpec&);

    bool		hasInitializedSetup() const;

    void		fillPar(IOPar&) const;
    bool		usePar(const IOPar&);

protected:

    Attrib::SelSpec*		attribsel_;
    EM::Horizon&		horizon_;
    EventTracker*		tracker_;

private:

    DataPackMgr&	dpm_;
    DataPack::ID	datapackid_;

    bool		track(const BinID&,const BinID&,float&) const;
    const TrcKey	getTrcKey(const BinID&) const;
    void		setHorizonPick(const BinID&,float val);

    static const char*	sKeyTracker()		{ return "Tracker"; }
    static const char*	sKeyAttribID()		{ return "Attribute"; }
};

} // namespace MPE

#endif

