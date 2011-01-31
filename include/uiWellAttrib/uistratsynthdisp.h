#ifndef uistratsynthdisp_h
#define uistratsynthdisp_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bert
 Date:		Nov 2010
 RCS:		$Id: uistratsynthdisp.h,v 1.12 2011-01-31 12:20:26 cvsbert Exp $
________________________________________________________________________

-*/

#include "uigroup.h"
#include "datapack.h"
class AIModel;
class Wavelet;
class uiGroup;
class SeisTrcBuf;
class uiFlatViewer;
class uiToolButton;
class uiSeisWaveletSel;
class uiToolButtonSetup;
namespace Strat { class LayerModel; }


mClass uiStratSynthDisp : public uiGroup
{
public:

    			uiStratSynthDisp(uiParent*,const Strat::LayerModel&);
    			~uiStratSynthDisp();

    void		setDispMrkrs(const TypeSet<float>&,Color);
    void		modelChanged();

    const uiWorldRect&	curView(bool indepth) const;
    const SeisTrcBuf&	curTraces() const;
    DataPack::FullID	packID() const;
    const ObjectSet<AIModel>& aiModels() const		{ return aimdls_; }

    Notifier<uiStratSynthDisp>	wvltChanged;
    Notifier<uiStratSynthDisp>	zoomChanged;

    static Notifier<uiStratSynthDisp>&	fieldsCreated();
    void		addTool(const uiToolButtonSetup&);

protected:

    const Strat::LayerModel& lm_;
    ObjectSet<AIModel>	aimdls_;
    Wavelet*		wvlt_;
    int			longestaimdl_;

    uiGroup*		topgrp_;
    uiSeisWaveletSel*	wvltfld_;
    uiFlatViewer*	vwr_;
    uiToolButton*	lasttool_;

    void		wvltChg(CallBacker*);
    void		zoomChg(CallBacker*);
    int			getVelIdx(bool&) const;
    int			getDenIdx(bool&) const;

};


#endif
