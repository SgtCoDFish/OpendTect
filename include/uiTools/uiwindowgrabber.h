#ifndef uiwindowgrabber_h
#define uiwindowgrabber_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        J.C. Glas
 Date:          July 2008
 RCS:           $Id$
________________________________________________________________________

-*/

#include "uitoolsmod.h"
#include "uidialog.h"

class uiLabeledComboBox;
class uiFileInput;
class uiSliderExtra;
class uiLabel;
class uiMainWin;
class Timer;


/*!Dialog to specify the grab window and the output image file */

mExpClass(uiTools) uiWindowGrabDlg : public uiDialog
{
public:
			uiWindowGrabDlg(uiParent*,bool desktop);

    uiMainWin*		getWindow() const;
    const char*		getFilename() const	{ return filename_.buf(); }
    int			getQuality() const;

protected:
    uiLabeledComboBox*	windowfld_;
    uiFileInput*	fileinputfld_;
    uiSliderExtra*	qualityfld_;
    uiLabel*		infofld_;

    ObjectSet<uiMainWin> windowlist_;

    void		updateFilter();
    void		fileSel(CallBacker*);
    void		addFileExtension(BufferString&);
    bool		filenameOK() const;

    bool		acceptOK(CallBacker*);
    void		surveyChanged(CallBacker*);

    const char*		getExtension() const;

    static BufferString dirname_;
    BufferString	filename_;
};


/*!Grabs the screen area covered by a window or the whole desktop */

mExpClass(uiTools) uiWindowGrabber: public CallBacker
{
public:
			uiWindowGrabber(uiParent*);
			~uiWindowGrabber();

    void		grabDesktop(bool yn)	{ desktop_ = yn; }
    bool		go();
    void		actCB(CallBacker*);

protected:
    uiParent*		parent_;
    bool		desktop_;
    uiMainWin*		grabwin_;
    BufferString	filename_;
    int			quality_;
    Timer*		tmr_;
};


#endif

