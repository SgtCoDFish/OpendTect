#ifndef uiprogressbar_H
#define uiprogressbar_H

/*+
________________________________________________________________________

 CopyRight:     (C) de Groot-Bril Earth Sciences B.V.
 Author:        A.H. Lammertink
 Date:          17/1/2001
 RCS:           $Id: uiprogressbar.h,v 1.5 2001-08-23 14:59:17 windev Exp $
________________________________________________________________________

-*/

#include <uiobj.h>

class uiProgressBarBody;

class uiProgressBar : public uiObject
{
public:

                        uiProgressBar( uiParent*,const char* nm="ProgressBar", 
				       int totalSteps=100, int progress=0);

    void		setProgress(int);
    int			Progress() const;
    void		setTotalSteps(int);
    int			totalSteps() const;

private:

    uiProgressBarBody*	body_;
    uiProgressBarBody&	mkbody(uiParent*, const char*);
};

#endif
