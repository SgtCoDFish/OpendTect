#ifndef seisposkey_h
#define seisposkey_h

/*
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bert
 Date:		Nov 2008
________________________________________________________________________

*/

#include "seiscommon.h"
#include "binid.h"


namespace Seis
{

mExpClass(Seis) PosKey
{
public:

    inline		PosKey(Seis::GeomType gt=Seis::Vol);
    inline		PosKey( const BinID& bid, float offs=mUdf(float) )
			    : binid_(bid), offset_(offs)		{}
    inline		PosKey( int trcnr, float offs=mUdf(float) )
			    : binid_(mUdf(int),trcnr), offset_(offs)	{}
    inline bool		operator ==(const PosKey&) const;
    inline bool		operator !=( const PosKey& oth ) const
						{ return !(*this == oth); }

    inline bool		is2D() const		{ return mIsUdf(binid_.inl()); }
    inline bool		isPS() const		{ return !mIsUdf(offset_); }
    inline Seis::GeomType geomType() const	{ return geomTypeOf(
							 is2D(),isPS()); }
    inline bool		isUndef() const		{ return mIsUdf(binid_.crl()); }
    inline void		setUndef()		{ mSetUdf(binid_.crl()); }

    inline const BinID&	binID() const		{ return binid_; }
    inline int		trcNr() const		{ return binid_.crl(); }
    inline int		inLine() const		{ return binid_.inl(); }
    inline int		xLine() const		{ return binid_.crl(); }
    inline float	offset() const		{ return offset_; }
    inline bool		hasOffset(float) const;

    inline void		setTrcNr( int trcnr )	{ binid_.crl() = trcnr; }
    inline void		setBinID( const BinID& bid ) { binid_ = bid; }
    inline void		setInline( int inl )	{ binid_.inl() = inl; }
    inline void		setXine( int crl )	{ binid_.crl() = crl; }
    inline void		setOffset( float offs )	{ offset_ = offs; }
    inline void		set( const BinID& bid, float offs )
			{ binid_ = bid; offset_ = offs; }
    inline void		set( int trcnr, float offs )
			{ binid_.inl() = mUdf(int); binid_.crl() = trcnr;
			    offset_ = offs; }
    inline void		set(int trcnr,const BinID&,float);
			//!< set what's relevant for GeomType

    inline int&		trcNr()			{ return binid_.crl(); }
    inline BinID&	binID()			{ return binid_; }
    inline float&	offset()		{ return offset_; }
    inline int&		inLine()		{ return binid_.inl(); }
    inline int&		xLine()			{ return binid_.crl(); }

    static PosKey	undef()			{ return PosKey( mUdf(int) ); }

protected:

    BinID		binid_;
    float		offset_;

};


inline PosKey::PosKey( Seis::GeomType gt )
    : binid_(0,0)
    , offset_(0)
{
    if ( Seis::is2D(gt) ) mSetUdf(binid_.inl());
    if ( !Seis::isPS(gt) ) mSetUdf(offset_);
}

inline bool PosKey::hasOffset( float offs ) const
{
    if ( mIsUdf(offs) )
	return mIsUdf(offset_);
    return equalOffset( offs, offset_ );
}

inline void PosKey::set( int nr, const BinID& bid, float offs )
{
    const Seis::GeomType gt( geomType() );
    if ( Seis::is2D(gt) )
	setTrcNr( nr );
    else
	setBinID( bid );
    if ( Seis::isPS(gt) )
	offset_ = offs;
    else
	mSetUdf(offset_);

}

inline bool PosKey::operator ==( const PosKey& oth ) const
{
    if ( binid_ != oth.binid_ )
	return false;

    if ( mIsUdf(oth.offset_) )
	return mIsUdf( offset_ );

    return mIsEqual(offset_,oth.offset_,1e-4);
}


} // namespace Seis


#endif
