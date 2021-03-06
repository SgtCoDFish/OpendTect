#ifndef uisurvinfoed_h
#define uisurvinfoed_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Nanne Hemstra
 Date:          June 2001
________________________________________________________________________

-*/

#include "uiiomod.h"
#include "uidialog.h"
#include "uisip.h"
#include "bufstringset.h"
#include "ranges.h"

class SurveyInfo;
class uiCheckBox;
class uiComboBox;
class uiGenInput;
class uiGroup;
class uiSurvInfoProvider;

/*!
\brief The survey info editor.
*/

mExpClass(uiIo) uiSurveyInfoEditor : public uiDialog
{ mODTextTranslationClass(uiSurveyInfoEditor);

public:

			uiSurveyInfoEditor(uiParent*,SurveyInfo&,
					   bool isnew=false);
    bool		isOK() const		{ return topgrp_; }
			//!<Must be checked before 'go'
			~uiSurveyInfoEditor();

    bool		dirnmChanged() const	{ return dirnamechanged; }
    const char*		dirName() const;

    static ObjectSet<uiSurvInfoProvider>& survInfoProvs();
    static int		addInfoProvider(uiSurvInfoProvider*);
    static bool		copySurv(const char* frompath,const char* fromdirnm,
				 const char* topath,const char* todirnm);
    static bool		renameSurv(const char* path,const char* fromdirnm,
				   const char* todirnm);

    Notifier<uiSurveyInfoEditor> survParChanged;

protected:

    SurveyInfo&		si_;
    BufferString	orgdirname_;
    BufferString	orgstorepath_;
    const BufferString	rootdir_;
    bool		isnew_;
    IOPar*		impiop_;
    ObjectSet<uiSurvInfoProvider> sips_;
    uiSurvInfoProvider*	lastsip_;

    uiGenInput*		survnmfld_;
    uiGenInput*		pathfld_;
    uiGenInput*		inlfld_;
    uiGenInput*		crlfld_;
    uiGenInput*		zfld_;
    uiComboBox*		zunitfld_;
    uiComboBox*		pol2dfld_;

    uiGenInput*		x0fld_;
    uiGenInput*		xinlfld_;
    uiGenInput*		xcrlfld_;
    uiGenInput*		y0fld_;
    uiGenInput*		yinlfld_;
    uiGenInput*		ycrlfld_;
    uiGenInput*		ic0fld_;
    uiGenInput*		ic1fld_;
    uiGenInput*		ic2fld_;
    uiGenInput*		xy0fld_;
    uiGenInput*		xy1fld_;
    uiGenInput*		xy2fld_;
    uiGenInput*		coordset;
    uiGroup*		topgrp_;
    uiGroup*		crdgrp_;
    uiGroup*		trgrp_;
    uiGroup*		rangegrp_;
    uiComboBox*		sipfld_;
    uiCheckBox*		overrulefld_;
    uiCheckBox*		xyinftfld_;
    uiGenInput*		depthdispfld_;
    uiGenInput*		refdatumfld_;

    bool		dirnamechanged;
    void		mkSIPFld(uiObject*);
    void		mkRangeGrp();
    void		mkCoordGrp();
    void		mkTransfGrp();
    void		setValues();
    void                updStatusBar(const char*);
    bool		setRanges();
    bool		setSurvName();
    bool		setCoords();
    bool		setRelation();
    bool		doApply();

    bool		acceptOK(CallBacker*);
    bool		rejectOK(CallBacker*);
    void		sipCB(CallBacker*);
    void		doFinalise(CallBacker*);
    void		setInl1Fld(CallBacker*);
    void		rangeChg(CallBacker*);
    void		depthDisplayUnitSel(CallBacker*);
    void		updZUnit(CallBacker*);
    void		chgSetMode(CallBacker*);
    void		pathbutPush(CallBacker*);
    void		appButPushed(CallBacker*);

    static uiString	getSRDString(bool infeet);

    friend class	uiSurvey;

};


mExpClass(uiIo) uiCopySurveySIP : public uiSurvInfoProvider
{
public:
			uiCopySurveySIP() {};

    virtual const char*	usrText() const	{ return "Copy from other survey"; }
    virtual uiDialog*	dialog(uiParent*);
    virtual bool	getInfo(uiDialog*,TrcKeyZSampling&,Coord crd[3]);
    virtual const char*	iconName() const	{ return "copyobj"; }

    virtual TDInfo	tdInfo() const { return tdinf_; }
    virtual bool	xyInFeet() const { return inft_; }


protected:

    TDInfo		tdinf_;
    bool		inft_;
    BufferStringSet	survlist_;

};

#endif
