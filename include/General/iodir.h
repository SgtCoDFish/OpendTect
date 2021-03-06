#ifndef iodir_H
#define iodir_H

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	A.H. Bril
 Date:		31-7-1995
________________________________________________________________________

-*/


#include "generalmod.h"

#include "multiid.h"
#include "objectset.h"
#include "namedobj.h"
#include "od_iosfwd.h"
#include "uistring.h"
class IOObj;


/*\brief 'Directory' of IOObj objects.

The IODir class is responsible for finding all IOObj's in the system. An IODir
instance will actually load all IOObj's, provides access to keys and allows
searching. It has a key of its own.

Few operation are done through the IODir directly: usually, IOMan will be
the service access point.

*/


mExpClass(General) IODir : public NamedObject
{ mODTextTranslationClass(IODir);
public:
			IODir(const char*);
			IODir(const MultiID&);
			~IODir();
    void		reRead();
    bool		isBad() const		{ return !isok_; }
    const MultiID&	key() const		{ return key_; }

    const IOObj*	main() const;
    const char*		dirName() const		{ return dirname_; }

    int			size() const		{ return objs_.size(); }
    int			isEmpty() const		{ return objs_.isEmpty(); }
    const IOObj*	get( int idx ) const	{ return objs_[idx]; }
    const ObjectSet<IOObj>& getObjs() const	{ return objs_; }

    bool		isPresent(const MultiID&) const;
    int			indexOf(const MultiID&) const;
    const IOObj*	get(const MultiID&) const;
    const IOObj*	get(const char* nm,const char* trgrpnm=0) const;
			// Without trgrpnm, just returns first

    bool		addObj(IOObj*,bool immediate_store=true);
			    //!< after call, IOObj is mine
    bool		commitChanges(const IOObj*);
			    //!< after call, assume pointer will be invalid
    bool		permRemove(const MultiID&);
    bool		ensureUniqueName(IOObj&);

    static IOObj*	getObj(const MultiID&,uiString& errmsg);
    static IOObj*	getMain(const char*,uiString& errmsg);

			// Use this if you know there's no contingency
			// Therefore, only in special-purpose programs
    bool		doWrite() const;

    uiString		errMsg() const		{ return errmsg_; }

private:

    ObjectSet<IOObj>	objs_;
    BufferString	dirname_;
    MultiID		key_;
    bool		isok_;
    mutable int		curid_;
    mutable uiString	errmsg_;

			IODir();
    static bool		create(const char* dirnm,const MultiID&,IOObj* mainobj);
    static IOObj*	doRead(const char*,IODir*,uiString& errmsg,int id=-1);
    static void		setDirName(IOObj&,const char*);
    static IOObj*	readOmf(od_istream&,const char*,IODir*,int);

    bool		build();
    bool		wrOmf(od_ostream&) const;
    IOObj*		get( int idx )		{ return objs_[idx]; }
    IOObj*		get(const MultiID&);

    MultiID		newKey() const;

    friend class	IOMan;
    friend class	IOObj;

};


#endif
