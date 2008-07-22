#ifndef uivelocityfunctionvolume_h
#define uivelocityfunctionvolume_h

/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	K. Tingdahl
 Date:		April 2005
 RCS:		$Id: uivelocityfunctionvolume.h,v 1.1 2008-07-22 17:39:21 cvskris Exp $
________________________________________________________________________


-*/

#include "uiselectvelocityfunction.h"

class CtxtIOObj;
class uiSeisSel;
class uiGenInput;

namespace Vel
{
class VolumeFunctionSource;

class uiVolumeFunction : public uiFunctionSettings
{
public:
    static void		initClass();

    			uiVolumeFunction(uiParent*,VolumeFunctionSource*);
    			~uiVolumeFunction();

    FunctionSource*	getSource();
    bool		acceptOK();

protected:
    static uiFunctionSettings*
				create(uiParent*,FunctionSource*);

    uiSeisSel*			volumesel_;
    CtxtIOObj*			ctxtioobj_;

    VolumeFunctionSource*	source_;
};


}; //namespace

#endif
