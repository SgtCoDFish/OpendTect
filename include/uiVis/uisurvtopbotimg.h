#ifndef uisurvtopbotimg_h
#define uisurvtopbotimg_h
/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        K. Tingdahl
 Date:          February 2006
 RCS:           $Id: uisurvtopbotimg.h,v 1.3 2009-06-23 05:25:47 cvsranojay Exp $
________________________________________________________________________

-*/

#include "uidialog.h"
#include "color.h"

class Coord;
class uiSurvTopBotImageGrp;
namespace visSurvey { class Scene; }


/*! sets the top and/or bottom images. */

mClass uiSurvTopBotImageDlg : public uiDialog
{
public:
			uiSurvTopBotImageDlg(uiParent*,visSurvey::Scene*);

protected:

    visSurvey::Scene*		scene_;

    uiSurvTopBotImageGrp*	topfld_;
    uiSurvTopBotImageGrp*	botfld_;

    void		setOn(bool,bool);
    void		setCoord(bool,const Coord&, const Coord&);
    void		setTransparency(bool,float);
    void		newFile(bool,const char*);

    friend class	uiSurvTopBotImageGrp;

};

#endif
