#ifndef uichangesurfacedlg_h
#define uichangesurfacedlg_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        N. Hemstra
 Date:          June 2006
________________________________________________________________________

-*/


#include "uiearthmodelmod.h"
#include "uidialog.h"

namespace EM { class Horizon; }

class uiHorSaveFieldGrp;
class Executor;
class uiGenInput;
class uiIOObjSel;
template <class T> class Array2D;

/*!\brief Base class for surface changers. At the moment only does horizons. */

mExpClass(uiEarthModel) uiChangeHorizonDlg : public uiDialog
{ mODTextTranslationClass(uiChangeHorizonDlg);
public:
				uiChangeHorizonDlg(uiParent*,EM::Horizon*,
						   bool is2d,const uiString&);
				~uiChangeHorizonDlg();
    uiHorSaveFieldGrp*		saveFldGrp() const { return savefldgrp_; }

protected:

    uiHorSaveFieldGrp*		savefldgrp_;				
    uiIOObjSel*			inputfld_;
    uiGroup*			parsgrp_;

    EM::Horizon*		horizon_;
    bool			is2d_;

    bool			acceptOK(CallBacker*);
    bool			readHorizon();
    bool			doProcessing();
    bool			doProcessing2D();
    bool			doProcessing3D();

    void			attachPars();	//!< To be called by subclass
    virtual const char*		infoMsg(const Executor*) const	{ return 0; }
    virtual Executor*		getWorker(Array2D<float>&,
					  const StepInterval<int>&,
					  const StepInterval<int>&) = 0;
    virtual bool		fillUdfsOnly() const	{ return false;}
    virtual bool		needsFullSurveyArray() const { return false;}
    virtual const char*		undoText() const		{ return 0; }
};




class uiStepOutSel;

mExpClass(uiEarthModel) uiFilterHorizonDlg : public uiChangeHorizonDlg
{ mODTextTranslationClass(uiFilterHorizonDlg)
public:
				uiFilterHorizonDlg(uiParent*,EM::Horizon*);

protected:

    uiGenInput*			medianfld_;
    uiStepOutSel*		stepoutfld_;

    Executor*			getWorker(Array2D<float>&,
	    				  const StepInterval<int>&,
					  const StepInterval<int>&);
    virtual const char*		undoText() const { return "filtering"; }

};


#endif
