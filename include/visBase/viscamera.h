#ifndef viscamera_h
#define viscamera_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Kristofer Tingdahl
 Date:		4-11-2002
________________________________________________________________________


-*/

#include "visbasemod.h"
#include "visdata.h"
#include "position.h"
namespace osg { class Camera; class RenderInfo; }

namespace visBase
{

class DrawCallback;

/*!\brief 
    keep osg camera status and render info
*/

mExpClass(visBase) Camera : public DataObject
{
public:

    static Camera*	create()
			mCreateDataObj( Camera );

    osg::Camera*	osgCamera() const;
    Coord3		getTranslation() const;
    Coord3		getScale() const;
    void		getRotation(Coord3& vec,double& angle)const;
    void		getLookAtMatrix(Coord3&,Coord3&,Coord3&)const;


    Notifier<Camera>		preDraw;
    Notifier<Camera>		postDraw;

    const osg::RenderInfo*	getRenderInfo() const { return renderinfo_; }
    				//!<Only available during pre/post draw cb

private:
    friend			class DrawCallback;

    void			triggerDrawCallBack(const DrawCallback*,
                                	            const osg::RenderInfo&);

    virtual			~Camera();

    osg::Camera*		camera_;
    const osg::RenderInfo*	renderinfo_;
    DrawCallback*		predraw_;
    DrawCallback*		postdraw_;

};


};


#endif
