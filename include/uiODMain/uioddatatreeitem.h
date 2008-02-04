#ifndef uioddatatreeitem_h
#define uioddatatreeitem_h

/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Kristofer Tingdahl
 Date:		May 2006
 RCS:		$Id: uioddatatreeitem.h,v 1.11 2008-02-04 07:09:58 cvsraman Exp $
________________________________________________________________________


-*/

#include "uiodtreeitem.h"
#include "factory.h"

class uiMenuHandler;
namespace Attrib { class SelSpec; };


/*!Base class for a data treeitem. */

class uiODDataTreeItem : public uiTreeItem
{
public:
				uiODDataTreeItem(const char* parenttype);
				~uiODDataTreeItem();

    bool			select();
    int				displayID() const;
    int				attribNr() const;

    static const int		cPixmapWidth()		{ return 16; }
    static const int		cPixmapHeight()		{ return 10; }

				mDefineFactory2ParamInClass(uiODDataTreeItem,
					const Attrib::SelSpec&,const char*,
					factory );

    				/*!<Adds custom create function for create
				    function. */

protected:

    int				uiListViewItemType() const;
    bool			init();

    virtual void		checkCB(CallBacker*);
    bool			shouldSelect(int) const;

    virtual bool		hasTransparencyMenu() const { return true; }

    uiODApplMgr*		applMgr() const;
    uiSoViewer*			viewer() const;
    int				sceneID() const;
    bool			isSelectable() const	{ return true; }
    bool			isExpandable() const	{ return false; }
    const char*			parentType() const	{ return parenttype_; }
    bool			showSubMenu();

    virtual void		createMenuCB(CallBacker*);
    virtual void		handleMenuCB(CallBacker*);
    void			updateColumnText(int col);
    virtual BufferString	createDisplayName() const		= 0;

    void			displayMiniCtab( int ctabid );

    uiMenuHandler*		menu_;
    MenuItem			movemnuitem_;
    MenuItem			movetotopmnuitem_;
    MenuItem			movetobottommnuitem_;
    MenuItem			moveupmnuitem_;
    MenuItem			movedownmnuitem_;

    MenuItem			removemnuitem_;
    MenuItem			changetransparencyitem_;
    MenuItem                    statisticsitem_;
    MenuItem			amplspectrumitem_;
    MenuItem			addto2dvieweritem_;
    MenuItem			view2dwvaitem_;
    MenuItem			view2dvditem_;
    const char*			parenttype_;
};

#endif
