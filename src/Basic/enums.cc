/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : K. Tingdahl
 * DATE     : Jan 2011
-*/


#include "enums.h"

#include "string2.h"

EnumDef::EnumDef()
{}

EnumDef::EnumDef( const char* nm, const char* s[] )
    : NamedObject(nm)
    , keys_(s)
{
    if ( !size() )
    {
	pErrMsg("Emtpy enum def!. Input array must be wrong");
    }
    for ( int idx=0; idx<size(); idx++ )
	enums_ += idx;
}


bool EnumDef::isValidKey( const char* s ) const
{
    return keys_.indexOf( s )>=0;
}


int EnumDef::indexOf( const char* s ) const
{
    const int idx = keys_.indexOf( s );
    return idx >= 0 ? idx : 0;
}


int EnumDef::indexOf( int theenum ) const
{
    const int idx = enums_.indexOf( theenum );
    return idx >= 0 ? idx : 0;
}


const char* EnumDef::getKeyForIndex( int i ) const
{ return keys_.get(i).buf(); }


uiString EnumDef::getUiStringForIndex( int i ) const
{
    if ( !uistrings_.validIdx(i) )
    {
	pErrMsg("Invalid enum");
	return uiString::emptyString();
    }

    return uistrings_[i];
}


const char* EnumDef::getIconFileForIndex(int i) const
{
    return iconfiles_.validIdx(i)
	    ? iconfiles_.get(i).str()
	    : 0;
}


void EnumDef::setIconFileForIndex( int i, const char* iconname )
{
    if ( i>=size() || !iconname || !*iconname )
	return;

    if ( !iconfiles_.size() )
    {
	for ( int idx=0; idx<size(); idx++ )
	    iconfiles_.add( sKey::EmptyString() );
    }
    iconfiles_.get(i) = iconname;
}


void EnumDef::setUiStringForIndex(int idx,const uiString& str)
{ uistrings_[idx]=str; }

int EnumDef::getEnumValForIndex(int idx) const
{ return enums_[idx]; }


int EnumDef::size() const
{ return keys_.size(); }


void EnumDef::remove( const char* key )
{
    const int idx = keys_.indexOf( key );
    if ( idx<0 )
    {
        pErrMsg("Removing missing enum");
        return;
    }

    uistrings_.removeSingle( idx, true );
    keys_.removeSingle(idx,true);
    enums_.removeSingle( idx, true );
    if ( iconfiles_.size() )
	iconfiles_.removeSingle( idx );
}


void EnumDef::add(const char* key, const uiString& string, int enumval,
                  const char* iconfile)
{
    uistrings_.add( string );
    enums_.add( enumval );
    keys_.add( key );
    setIconFileForIndex( keys_.size()-1, iconfile );
}


void EnumDef::fillUiStrings()
{
    for ( int idx=0; idx<keys_.size(); idx++ )
	uistrings_ += ::toUiString( keys_.get(idx) );

}


bool EnumDef::isValidName(const char* key) const
{ return isValidKey(key); }


const char* EnumDef::convert(int idx) const
{ return getKeyForIndex(idx); }


int EnumDef::convert(const char* txt) const
{ return indexOf(txt); }
