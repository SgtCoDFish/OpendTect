#ifndef uibody_h
#define uibody_h

/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        A.H. Lammertink
 Date:          16/05/2001
 RCS:           $Id: uibody.h,v 1.10 2006-08-21 17:14:45 cvsbert Exp $
________________________________________________________________________

-*/

#include <uihandle.h>
#include <uiparent.h>

class QWidget;

class uiBody
{
public:
				uiBody()				{}
    virtual			~uiBody()				{}

    virtual void		finalise()				{}
    virtual void		clear()					{}
    virtual void		fontchanged()				{}


				//! can return 0
    inline const QWidget*       qwidget() const		{ return qwidget_();}
				//! can return 0
    inline QWidget*             qwidget()
                                   {return const_cast<QWidget*>(qwidget_());}

protected:

    virtual const QWidget*	qwidget_() const		=0;

};


/*! \brief Simple delegating implementation of uiBody.

Useful when a Qt object is already made, such as a QStatusBar, QMenuBar, etc.

*/
template <class C, class T>
class uiBodyImpl : public uiBody
{
public:
                        uiBodyImpl( C& handle, 
					   uiParent* parnt, 
					   T& qthing_ ) 
			    : uiBody()
			    , qthing__( &qthing_ )
			    , handle_( handle )
			    {}



    T*			qthing()			{ return qthing__; }
    const T*		qthing() const			{ return qthing__; }

    inline const C&	handle()			{ return handle_; }

protected:

    virtual const QWidget* qwidget_() const		
			    { return dynamic_cast<QWidget*>( qthing__ ); }

    T*			qthing__;

private:

    C&			handle_;

};

#if 0
/*! \brief Simple delegating implementation of uiBody.

Useful when a Qt object is already made, such as a QStatusBar, QMenuBar, etc.

*/
template <class C, class T>
class uiBodyImpl : public uiBodyImpl<C,T>
{
public:
                        uiBodyImpl( C& handle, uiParent* parnt,
					      T& qthing_ ) 
			    : uiBodyImpl<C,T>( handle,parnt,qthing_ ) {}

protected:

    virtual const QWidget* qwidget_() const		{ return 0; }

};


#endif

#endif
