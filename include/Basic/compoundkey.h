#ifndef compoundkey_h
#define compoundkey_h

/*+
________________________________________________________________________

 CopyRight:	(C) de Groot-Bril Earth Sciences B.V.
 Author:	A.H. Bril
 Date:		15-1-2000
 RCS:		$Id: compoundkey.h,v 1.1 2001-02-13 17:46:47 bert Exp $
________________________________________________________________________

-*/


#ifndef general_H
#include <general.h>
#endif
#include <stdlib.h>

/*!\brief Concatenated short keys separated by dots.

 Usage is for Object identifiers in the Object Manager, or UnitIDs.

 A Compound Key Glob Expression is a string used for matching.
 It is similar to a UNIX-type glob expression.

*/


class CompoundKey
{
public:

    inline		CompoundKey(const char* s=0)	{ if ( s ) id = s; }
    inline CompoundKey&	operator=(const char* s)	{ id = s; return *this;}
    inline CompoundKey&	operator+=(const char*);
    inline bool		operator==(const char* s) const	{ return id == s; }
    inline bool		operator==(const CompoundKey& u) const
							{ return id == u.id; }
    inline bool		operator!=(const char* s) const	{ return id != s; }
    inline bool		operator!=(const CompoundKey& u) const
							{ return id != u.id; }
    inline		operator char*() const		{ return id; }

    int			nrKeys() const;
    BufferString	key(int) const;
    void		setKey(int,const char*);
    CompoundKey		upLevel() const;
    bool		isUpLevelOf(const CompoundKey&) const;

    virtual bool	matchGE(const char*) const;

protected:

    BufferString	id;
    char*		fromKey(int,bool cptobuf=false) const;

};


inline ostream& operator <<( ostream& strm, const CompoundKey& uid )
{ strm << (const char*)uid; return strm; }
inline istream& operator >>( istream& strm, CompoundKey& uid )
{ strm >> (char*)uid; return strm; }

inline CompoundKey& CompoundKey::operator +=( const char* s )
{
    if ( id[0] ) id += ".";
    id += s;
    return *this;
}


#endif
