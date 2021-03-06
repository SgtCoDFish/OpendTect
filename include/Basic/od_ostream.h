#ifndef od_ostream_h
#define od_ostream_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bert
 Date:		Sep 2013
________________________________________________________________________

-*/

#include "basicmod.h"
#include "od_stream.h"

class SeparString;
class CompoundKey;
class FixedString;
class uiString;


/*!\brief OD class for stream write */

mExpClass(Basic) od_ostream : public od_stream
{
public:

			od_ostream()				{}
			od_ostream( const char* fnm, bool useexist=false )
			    : od_stream(fnm,true,useexist)	{}
			od_ostream( const FilePath& fp, bool useexist=false )
			    : od_stream(fp,true,useexist)	{}
			od_ostream( std::ostream* s )
			    : od_stream(s)			{}
			od_ostream( std::ostream& s )
			    : od_stream(s)			{}
			od_ostream( const od_ostream& s )
			    : od_stream(s)			{}
    od_ostream&		operator =( const od_ostream& s )
			    { od_stream::operator =(s); return *this; }
    bool		open(const char*,bool useexist=false);

    od_ostream&		add(char);
    od_ostream&		add(unsigned char);
    od_ostream&		add(const char*);
    od_ostream&		add(od_int16);
    od_ostream&		add(od_uint16);
    od_ostream&		add(od_int32);
    od_ostream&		add(od_uint32);
    od_ostream&		add(od_int64);
    od_ostream&		add(od_uint64);
#ifdef __lux64__
    od_ostream&		add(long long);
    od_ostream&		add(unsigned long long);
#else
    od_ostream&		add(long);
    od_ostream&		add(unsigned long);
#endif
    od_ostream&		add(float);
    od_ostream&		add(double);

    od_ostream&		add(const OD::String&);
    od_ostream&		add(const uiString&);
    od_ostream&		add(const IOPar&);
    od_ostream&		add(const SeparString&);
    od_ostream&		add(const CompoundKey&);

    od_ostream&		add(const void*); //!< produces pErrMsg but works
    od_ostream&		addPtr(const void*);
    od_ostream&		add(od_istream&);
    od_ostream&		add( od_ostream& )	{ return *this; }

    bool		addBin(const void*,Count nrbytes);
    template <class T>
    od_ostream&		addBin(const T&);

    std::ostream&	stdStream();
    void		flush();

    static od_ostream&	nullStream();
    static od_ostream&	logStream(); //!< used by ErrMsg and UsrMsg

};


//!< common access to the user log file, or std::cout in batch progs
inline mGlobal(Basic) od_ostream& od_cout() { return od_ostream::logStream(); }

//!< Never redirected
mGlobal(Basic) od_ostream& od_cerr();



template <class T>
inline od_ostream& operator <<( od_ostream& s, const T& t )
{ return s.add( t ); }


inline od_ostream& od_endl( od_ostream& strm )
{
    strm.add( od_newline ).flush();
    return strm;
}


mGlobal(Basic) std::ostream& od_endl( std::ostream& strm );
//Just to give linker errors


typedef od_ostream& (*od_ostreamFunction)(od_ostream&);
inline od_ostream& operator <<( od_ostream& s, od_ostreamFunction fn )
{ return (*fn)( s ); }

template <class T>
inline od_ostream& od_ostream::addBin( const T& t )
{
    addBin( &t, sizeof(T) );
    return *this;
}


#endif
