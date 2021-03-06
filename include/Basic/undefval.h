#ifndef undefval_h
#define undefval_h
/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        A.H. Lammertink
 Date:          13/01/2005
________________________________________________________________________

-*/

#include "basicmod.h"
#include "commondefs.h"
#include "plftypes.h"

//! Undefined value. IEEE gives NaN but that's not exactly what we want
#define __mUndefDValue            1e30
#define __mUndefFValue            1e30f
//! Check on undefined. Also works when double converted to float and vv
#define __mIsUndefinedD(x)         (((x)>9.99999e29)&&((x)<1.00001e30))
#define __mIsUndefinedF(x)         (((x)>9.99999e29f)&&((x)<1.00001e30f))
//! Almost MAXINT so unlikely, but not MAXINT to avoid that
#define __mUndefIntVal            2109876543
//! Almost MAXINT64 therefore unlikely.
#define __mUndefIntVal64          9223344556677889900LL


/*!  \brief Templatized undefined and initialisation (i.e. null) values.  

    Since these are all templates, they can be used much more generic
    than previous solutions with macros.

Use like:

  T x = mUdf(T);
  if ( mIsUdf(x) )
      mSetUdf(y);

*/

namespace Values
{

/*!
\brief Templatized undefined values.
*/

template<class T>
mClass(Basic) Undef
{
public:
    static T		val();
    static bool		hasUdf();
    static bool		isUdf(T);
    void		setUdf(T&);
};


/*!
\brief Undefined od_int16.
*/

template<>
mClass(Basic) Undef<od_int16>
{
public:
    static od_int16	val()			{ return 32766; }
    static bool		hasUdf()		{ return false; }
    static bool		isUdf( od_int16 i )	{ return i == 32766; }
    static void		setUdf( od_int16& i )	{ i = 32766; }
};


/*!
\brief Undefined od_uint16.
*/

template<>
mClass(Basic) Undef<od_uint16>
{
public:
    static od_uint16	val()			{ return 65534; }
    static bool		hasUdf()		{ return false; }
    static bool		isUdf( od_uint32 i )	{ return i == 65534; }
    static void		setUdf( od_uint32& i )	{ i = 65534; }
};


/*!
\brief Undefined od_int32.
*/

template<>
mClass(Basic) Undef<od_int32>
{
public:
    static od_int32	val()			{ return __mUndefIntVal; }
    static bool		hasUdf()		{ return true; }
    static bool		isUdf( od_int32 i )	{ return i == __mUndefIntVal; }
    static void		setUdf( od_int32& i )	{ i = __mUndefIntVal; }
};


/*!
\brief Undefined od_uint32.
*/

template<>
mClass(Basic) Undef<od_uint32>
{
public:
    static od_uint32	val()			{ return __mUndefIntVal; }
    static bool		hasUdf()		{ return true; }
    static bool		isUdf( od_uint32 i )	{ return i == __mUndefIntVal; }
    static void		setUdf( od_uint32& i )	{ i = __mUndefIntVal; }
};


/*!
\brief Undefined od_int64.
*/

template<>
mClass(Basic) Undef<od_int64>
{
public:
    static od_int64	val()			{ return __mUndefIntVal64; }
    static bool		hasUdf()		{ return true; }
    static bool		isUdf( od_int64 i )	{ return i == __mUndefIntVal64;}
    static void		setUdf( od_int64& i )	{ i = __mUndefIntVal64; }
};


/*!
\brief Undefined od_uint64.
*/

template<>
mClass(Basic) Undef<od_uint64>
{
public:
    static od_uint64	val()			{ return __mUndefIntVal64; }
    static bool		hasUdf()		{ return true; }
    static bool		isUdf( od_uint64 i )	{ return i == __mUndefIntVal64;}
    static void		setUdf( od_uint64& i )	{ i = __mUndefIntVal64; }
};


/*!
\brief Undefined bool.
*/

template<>
mClass(Basic) Undef<bool>
{
public:
    static bool		val()			{ return false; }
    static bool		hasUdf()		{ return false; }
    static bool		isUdf( bool b )		{ return false; }
    static void		setUdf( bool& b )	{ b = false; }
};


/*!
\brief Undefined float.
*/

template<>
mClass(Basic) Undef<float>
{
public:
    static float	val()			{ return __mUndefFValue; }
    static bool		hasUdf()		{ return true; }
    static bool		isUdf( float f )	{ return __mIsUndefinedF(f); }
    static void		setUdf( float& f )	{ f = __mUndefFValue; }
};


/*!
\brief Undefined double.
*/

template<>
mClass(Basic) Undef<double>
{
public:
    static double	val()			{ return __mUndefDValue; }
    static bool		hasUdf()		{ return true; }
    static bool		isUdf( double d )	{ return __mIsUndefinedD(d); }
    static void		setUdf( double& d )	{ d = __mUndefDValue; }
};


/*!
\brief Undefined const char*.
*/

template<>
mClass(Basic) Undef<const char*>
{
public:
    static const char*	val()			{ return ""; }
    static bool		hasUdf()		{ return true; }
    static bool		isUdf( const char* s )	{ return !s || !*s; }
    static void		setUdf( const char*& )	{}
};


/*!
\brief Undefined char*.
*/

template<>
mClass(Basic) Undef<char*>
{
public:
    static const char*	val()			{ return ""; }
    static bool		hasUdf()		{ return true; }
    static bool		isUdf( const char* s )	{ return !s || !*s; }
    static void		setUdf( char*& s )	{ if ( s ) *s = '\0'; }
};


template <class T> inline
bool isUdf( const T& t )
{ 
    return Undef<T>::isUdf(t);  
}

template <class T> inline
const T& udfVal( const T& t )
{ 
    mDefineStaticLocalObject( T, u, = Undef<T>::val() );
    return u;
}

template <class T> inline
bool hasUdf()
{ 
    return Undef<T>::hasUdf();  
}

template <class T> inline
T& setUdf( T& u )
{
    Undef<T>::setUdf( u );
    return u; 
}

}


//! Use this macro to get the undefined for simple types
#define mUdf(type) Values::Undef<type>::val()
//! Use this macro to set simple types to undefined
#define mSetUdf(val) Values::setUdf(val)



template <class T>
inline bool isUdfImpl( T val )
    { return Values::isUdf( val ); }

mGlobal(Basic) bool isUdfImpl(float);
mGlobal(Basic) bool isUdfImpl(double);


//! Use mIsUdf to check for undefinedness of simple types
# define mIsUdf(val) isUdfImpl(val)


#endif
