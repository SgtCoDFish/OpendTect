#ifndef iodrawtool_H
#define iodrawtool_H

/*+
________________________________________________________________________

 CopyRight:     (C) de Groot-Bril Earth Sciences B.V.
 Author:        A.H. Lammertink
 Date:          04/07/2001
 RCS:           $Id: iodrawtool.h,v 1.1 2001-08-23 14:59:17 windev Exp $
________________________________________________________________________

-*/

#include <iodraw.h>
#include <uigeom.h>
#include <color.h>
#include <draw.h>

class QPaintDevice; 
class QPaintDeviceMetrics; 
class QPainter;
class QPen;

class ioPixmap;
class uiFont;

//! Tool to draw on ioDrawArea's. Each ioDrawArea can give you a drawtool.
class ioDrawTool
{   

    friend class	ioDrawAreaImpl;
    friend class	uiScrollViewBody;
//    mTFriend		(T,i_drwblQObj);

mProtected:
			ioDrawTool( QPaintDevice* handle, int x_0=0, int y_0=0);
public:

    virtual		~ioDrawTool(); 

    Color		backgroundColor() const;
    void		setBackgroundColor(const Color&);
    void		clear(const uiRect* r=0,const Color* c=0);

    void		drawLine( int x1, int y1, int x2, int y2 );
    inline void		drawLine( uiPoint p1, uiPoint p2 )
                        { drawLine ( p1.x(), p1.y(), p2.x(), p2.y() ); }

    void		drawText( int x, int y, const char *, Alignment, 
				  bool erase=false, int len = -1);
    inline void		drawText( uiPoint p, const char * txt, Alignment al, 
				  bool erase=false, int len =-1 )
                        { drawText( p.x(), p.y(), txt, al, erase, len ); }

    void 		drawRect( int x, int y, int w, int h ); 
    inline void		drawRect( uiPoint topLeft, uiSize sz )
                        { drawRect( topLeft.x(), topLeft.y(), 
                                    sz.width(), sz.height()); }
    inline void		drawRect( uiRect r )
                        { drawRect( r.left(), r.top(), 
                                    r.width(), r.height()); }

    void 		drawEllipse( int x, int y, int w, int h ); 
    inline void		drawEllipse( uiPoint topLeft, uiSize sz )
                        { drawEllipse( topLeft.x(), topLeft.y(), 
                                       sz.width(), sz.height()); }
    inline void 	drawEllipse( uiRect r )
                        { drawEllipse( r.left(), r.top(), 
                                       r.width(), r.height()); }

    void 		drawPixmap( uiPoint destTopLeft,
				     ioPixmap*, 
				     uiRect srcAreaInPixmap );
    inline void		drawPixmap( int left, int top, 
				     ioPixmap* pm , 
                                     int sLeft, int sTop,
                                     int sRight, int sBottom )
                        { drawPixmap( uiPoint( left, top ), pm, 
                                      uiRect( sLeft, sTop, sRight, sBottom )); } 

    void		drawMarker(uiPoint,const MarkerStyle&,const char* txt=0,
				   bool below=true);

    int 		getDevHeight() const;
    int 		getDevWidth() const;

    void		setLineStyle( LineStyle );
    void		setPenColor( Color );
    void		setFillColor( Color );
    void		setPenWidth( unsigned int );

    void		setFont( const uiFont& f);
    const uiFont*	font()				{ return font_; }

    inline void		setOrigin( uiPoint tl ) { setOrigin( tl.x(), tl.y() ); }
    void		setOrigin( int x_0, int y_0 ) { x0 = x_0; y0 = y_0; }

    bool 		active() const { return active_; }
    bool	        beginDraw(); 
    bool		endDraw();

protected:

    bool		setActivePainter( QPainter* );

private:
    QPainter*		mQPainter;
    QPen&		mQPen;
    bool		freeMQPainter;
    QPaintDevice*	mQPaintDev;
    QPaintDeviceMetrics* mQPaintDevMetrics;
    bool		active_;
    int			x0;
    int			y0;
    const uiFont*	font_;
};

#endif
