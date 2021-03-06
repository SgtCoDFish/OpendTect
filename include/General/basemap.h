#ifndef basemap_h
#define basemap_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	K. Tingdahl
 Date:		Sep 2009
________________________________________________________________________

-*/

#include "generalmod.h"
#include "draw.h"
#include "namedobj.h"
#include "notify.h"

namespace OD
{
    class RGBImage;
    class LineStyle;
    class MarkerStyle2D;
}


class MouseEvent;
class TaskRunner;
template <class T> class ODPolygon;

/*!Object that can be painted in a basemap. */


mExpClass(General) BaseMapObject : public NamedObject
{
public:
				BaseMapObject(const char* nm);

    int				ID() const		{ return id_; }

    Threads::Lock		lock_;
    virtual void		updateGeometry()			{}

    virtual void		setType(const char* tp) { typenm_ = tp;}
    virtual const char*		getType() const		{ return typenm_; }

    virtual void		setDepth(int val)	{ depth_ = val; }
    virtual int			getDepth() const	{ return depth_; }
				/*!<Determines what should be painted ontop of
				    what */

    virtual int			nrShapes() const;
    virtual const char*		getShapeName(int shapeidx) const;
    virtual void		getPoints(int shapeidx,TypeSet<Coord>&) const;
				/*!<Returns a number of coordinates that
				    may form a be connected or filled. */
    virtual bool		getBoundingBox(
					Geom::PosRectangle<double>&) const;
				/*!<Returns the Coord range. */
    virtual OD::Alignment	getAlignment(int shapeidx) const;
    virtual float		getTextRotation() const { return 0; }
    virtual Color		getColor() const;

    virtual void		setMarkerStyle(int idx,
					const OD::MarkerStyle2D&) {}
    virtual const OD::MarkerStyle2D* getMarkerStyle(int shapeidx) const
					{ return 0;}
    virtual BufferString	getImageFileName(int idx) const	{ return ""; }
    virtual void		getXYScale(int idx,float& scx,float& scy) const
				{ scx = scy = 1.f; }
    virtual void		setXYScale(int idx,float scx,float scy)	{}

    virtual void		setLineStyle(int shapeidx,
					const OD::LineStyle&) {}
    virtual const OD::LineStyle* getLineStyle(int shapeidx) const { return 0; }

    virtual void		setFillColor(int idx,const Color&)	    {}
    virtual const Color		getFillColor(int idx) const
						    { return Color::NoColor();}

    virtual bool		fill(int shapeidx) const	{ return false;}
    virtual bool		close(int shapeidx) const	{ return false;}

    virtual void		setImage(int idx,OD::RGBImage*)     {}
    virtual const OD::RGBImage* getImage(int shapeidx) const	{ return 0;}

    virtual const OD::RGBImage* createImage(Coord& origin,Coord& p11) const;
				/*!<Returns image in xy plane. p11 is the
				    coordinate of the corner opposite of the
				    origin. */
    virtual const OD::RGBImage* createPreview(int approxdiagonal) const;
				/*!<Returns a preview image that has
				    approximately the size of the specified
				    diagonal. */
    virtual bool		allowHoverEvent(int) const	{ return true; }

    virtual bool		canRemoveWithPolygon() const   { return false; }
    virtual void		removeWithPolygon(const ODPolygon<double>&) {}

    virtual bool		fillPar(IOPar&) const;
    virtual bool		usePar(const IOPar&,TaskRunner* taskr=0);

    CNotifier<BaseMapObject,const MouseEvent&>	leftClicked;
    CNotifier<BaseMapObject,const MouseEvent&>	rightClicked;
    Notifier<BaseMapObject>			changed;
    Notifier<BaseMapObject>			stylechanged;
    CNotifier<BaseMapObject,const uiString&>	namechanged;

protected:
    int				depth_;
    int				id_;
    BufferString		typenm_;
};


/*!Base class for a Basemap. */
mExpClass(General) BaseMap
{
public:

    virtual void		addObject(BaseMapObject*)		= 0;
				/*!<Object maintained by caller. Adding an
				    existing will trigger update */

    virtual void		removeObject(const BaseMapObject*)	= 0;

};


#endif
