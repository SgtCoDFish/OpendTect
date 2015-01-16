#ifndef uibasemaprandomlineitem_h
#define uibasemaprandomlineitem_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Nanne Hemstra
 Date:		January 2014
 RCS:		$Id$
________________________________________________________________________

-*/

#include "uibasemapmod.h"
#include "uibasemapitem.h"

mExpClass(uiBasemap) uiBasemapRandomLineGroup : public uiBasemapIOObjGroup
{
public:
			uiBasemapRandomLineGroup(uiParent*,bool isadd);
			~uiBasemapRandomLineGroup();

    bool		acceptOK();
    bool		fillPar(IOPar&) const;
    bool		usePar(const IOPar&);

protected:

};



mExpClass(uiBasemap) uiBasemapRandomLineTreeItem : public uiBasemapTreeItem
{
public:
			uiBasemapRandomLineTreeItem(const char*);
			~uiBasemapRandomLineTreeItem();

    bool		usePar(const IOPar&);

protected:

    bool		showSubMenu();
    bool		handleSubMenu(int);
    const char*		parentType() const
			{ return typeid(uiBasemapTreeTop).name(); }
};



mExpClass(uiBasemap) uiBasemapRandomLineItem : public uiBasemapItem
{
public:
			mDefaultFactoryInstantiation(
				uiBasemapItem,
				uiBasemapRandomLineItem,
				"RandomLines",
				sFactoryKeyword())

    const char*		iconName() const;
    uiBasemapGroup*	createGroup(uiParent*,bool isadd);
    uiBasemapTreeItem*	createTreeItem(const char*);
};

#endif