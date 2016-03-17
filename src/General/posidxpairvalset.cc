/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : Bert
 * DATE     : 21-6-1996 / Mar 2016
-*/

static const char* rcsID mUsedVar = "$Id$";

#include "posidxpairvalset.h"
#include "posidxpairvalue.h"
#include "iopar.h"
#include "separstr.h"
#include "posinfo.h"
#include "strmoper.h"
#include "statrand.h"
#include "survgeom.h"
#include "threadlock.h"
#include "od_iostream.h"

static const float cMaxDistFromGeom = 1000.f;
static Threads::Lock udf_lock_(true);
static TypeSet<float> udf_typeset_(64,mUdf(float));


static const float* getUdfArr( int nrvals )
{
    Threads::Locker lckr( udf_lock_ );
    if ( udf_typeset_.size() < nrvals )
	udf_typeset_.setSize( nrvals, mUdf(float) );
    return udf_typeset_.arr();
}


static inline void setToUdf( float* arr, int nvals )
{
    if ( nvals == 0 )
	return;
    else if ( !arr )
	{ pFreeFnErrMsg("input arr should not be null"); return; }

    OD::memCopy( arr, getUdfArr(nvals), nvals * sizeof(float) );
}


static void initVals( Pos::IdxPairDataSet& ds, int spos_i, int spos_j )
{
    const Pos::IdxPairDataSet::SPos spos( spos_i, spos_j );
    const int nrvals = (int)(ds.objSize() / sizeof(float));
    float* vals = (float*)ds.getObj( spos );
    if ( vals )
	setToUdf( vals, nrvals );
    else
	ds.set( spos, getUdfArr(nrvals) );
}


Pos::IdxPairValueSet::IdxPairValueSet( int nv, bool ad )
	: data_(nv*sizeof(float),ad,true)
	, nrvals_(nv)
{
}


Pos::IdxPairValueSet& Pos::IdxPairValueSet::operator =(
				    const IdxPairValueSet& oth )
{
    if ( this != &oth )
    {
	data_ = oth.data_;
	const_cast<int&>( nrvals_ ) = oth.nrvals_;
    }
    return *this;
}


void Pos::IdxPairValueSet::copyStructureFrom( const IdxPairValueSet& oth )
{
    data_.copyStructureFrom( oth.data_ );
    const_cast<int&>(nrvals_) = oth.nrvals_;
}


bool Pos::IdxPairValueSet::getFrom( od_istream& strm, GeomID gid )
{
    setEmpty();
    if ( !setNrVals(0) )
	return false;

    BufferString line; char valbuf[1024];
    const Survey::Geometry* survgeom = Survey::GM().getGeometry( gid );
    int coordindic = -1;

    while ( strm.getLine( line ) )
    {
	char* firstchar = line.getCStr();
	mSkipBlanks( firstchar );
	if ( *firstchar == '"' )
	{
	    char* ptr = firstOcc( firstchar+1, '"' );
	    if ( !ptr ) continue;
	    firstchar = ptr+1;
	}
	mSkipBlanks( firstchar );
	if ( !*firstchar || (*firstchar != '-' && !iswdigit(*firstchar) ) )
	    continue;

	const char* nextword = getNextWord( firstchar, valbuf );
	Coord coord;
	coord.x = toDouble( valbuf );
	mSkipBlanks( nextword ); if ( !*nextword ) continue;
	nextword = getNextWord( nextword, valbuf );
	coord.y = toInt( valbuf );

	if ( coordindic < 0 )
	{
	    float dist = mUdf(float);
	    if ( survgeom )
		(void)survgeom->nearestTrace( coord, &dist );

	    const char* firstval = nextword;
	    int nrvalsfound = 0;
	    while ( true )
	    {
		mSkipBlanks( nextword ); if ( !*nextword ) break;
		nrvalsfound++;
		nextword = getNextWord( nextword, valbuf );
	    }
	    setNrVals( nrvalsfound );
	    nextword = firstval;
	    coordindic = dist < cMaxDistFromGeom ? 1 : 0;
	}

	TrcKey tk;
	if ( coordindic == 1 )
	    tk = survgeom->nearestTrace( coord );
	else
	{
	    tk.setLineNr( (Pos::LineID)(coord.x + 0.5) );
	    tk.setTrcNr( (Pos::TraceID)(coord.y + 0.5) );
	}

	float* vals = getVals( add(tk.binID()) );
	if ( !vals )
	    continue;

	for ( int idx=0; idx<nrVals(); idx++ )
	{
	    mSkipBlanks( nextword ); if ( !*nextword ) break;
	    nextword = getNextWord( nextword, valbuf );
	    if ( !valbuf[0] ) break;
	    vals[idx] = toFloat(valbuf);
	}
    }

    return !isEmpty();
}


bool Pos::IdxPairValueSet::putTo( od_ostream& strm ) const
{
    SPos spos;
    while ( next(spos) )
    {
	const IdxPair ip( getIdxPair(spos) );
	const float* vals = gtVals(spos);
	strm << ip.first << od_tab << ip.second;
	for ( int idx=0; idx<nrvals_; idx++ )
	    strm << od_tab << toString( vals[idx] );
	strm << od_newline;
    }
    strm.flush();
    return strm.isOK();
}


float* Pos::IdxPairValueSet::getVals( const SPos& spos )
{
    return !spos.isValid() ? 0 : gtVals( spos );
}


const float* Pos::IdxPairValueSet::getVals( const SPos& spos ) const
{
    return !spos.isValid() ? 0 : gtVals( spos );
}


float Pos::IdxPairValueSet::getVal( const SPos& spos, int valnr ) const
{
    if ( !spos.isValid() || valnr < 0 || valnr >= nrvals_ )
	return mUdf(float);

    return gtVals(spos)[valnr];
}


Interval<float> Pos::IdxPairValueSet::valRange( int valnr ) const
{
    Interval<float> ret( mUdf(float), mUdf(float) );
    if ( valnr >= nrvals_ || valnr < 0 || isEmpty() )
	return ret;

    SPos spos;
    while ( next(spos) )
    {
	const float val = gtVals(spos)[valnr];
	if ( !mIsUdf(val) )
	    { ret.start = ret.stop = val; break; }
    }
    while ( next(spos) )
    {
	const float val = gtVals(spos)[valnr];
	if ( !mIsUdf(val) )
	    ret.include( val, false );
    }

    return ret;
}


void Pos::IdxPairValueSet::get( const SPos& spos, float* vals,
				 int maxnrvals ) const
{
   if ( maxnrvals > nrvals_ )
	maxnrvals = nrvals_;
    if ( maxnrvals < 0 )
	maxnrvals = nrvals_ + maxnrvals + 1;
    if ( !vals || maxnrvals < 1 )
	return;

    if ( spos.isValid() )
	OD::memCopy( vals, gtVals(spos), maxnrvals * sizeof(float) );
    else
	setToUdf( vals, maxnrvals );
}


void Pos::IdxPairValueSet::get( const SPos& spos, IdxPair& ip, float* vals,
				 int maxnrvals ) const
{
    get( spos, vals, maxnrvals );
    ip = getIdxPair( spos );
}


Pos::IdxPairValueSet::SPos Pos::IdxPairValueSet::add( const Pos::IdxPair& ip,
						      const float* arr )
{
    if ( !arr )
	arr = getUdfArr( nrvals_ );
    return data_.add( ip, arr );
}


Pos::IdxPairValueSet::SPos Pos::IdxPairValueSet::add( const DataRow& dr )
{
    if ( dr.size() >= nrvals_ )
	return add( dr, dr.values() );

    DataRow locdr( 0, 0, nrvals_ );
    for ( int idx=0; idx<dr.size(); idx++ )
	locdr.value(idx) = dr.value(idx);
    for ( int idx=dr.size(); idx<nrvals_; idx++ )
	mSetUdf( locdr.value(idx) );
    return add( dr, locdr.values() );
}


void Pos::IdxPairValueSet::add( const PosInfo::CubeData& cubedata )
{
    data_.add( cubedata, initVals );
}


void Pos::IdxPairValueSet::set( SPos spos, const float* vals )
{
    if ( !spos.isValid() || nrvals_ < 1 )
	return;

    if ( !vals )
	vals = getUdfArr( nrvals_ );

    data_.set( spos, vals );
}


void Pos::IdxPairValueSet::removeVal( int validx )
{
    if ( validx < 0 || validx >= nrvals_ )
	return;
    else if ( nrvals_ == 1 )
	{ setNrVals( 0 ); return; }

    data_.decrObjSize( sizeof(float), validx*sizeof(float) );
    const_cast<int&>(nrvals_)--;
}


bool Pos::IdxPairValueSet::insertVal( int validx )
{
    if ( validx < 0 || validx >= nrvals_ )
	return false;

    const_cast<int&>(nrvals_)++;
    const float udf = mUdf(float);
    return data_.incrObjSize( sizeof(float), validx*sizeof(float), &udf );
}


bool Pos::IdxPairValueSet::setNrVals( int newnrvals )
{
    const int nrdiff = newnrvals - nrvals_;

    if ( nrdiff < 0 )
	data_.decrObjSize( (-nrdiff)*sizeof(float) );
    else if ( nrdiff > 0 && !data_.incrObjSize(nrdiff*sizeof(float)) )
	return false;

    const_cast<int&>(nrvals_) = newnrvals;
    return true;
}


void Pos::IdxPairValueSet::extend( const Pos::IdxPairDelta& so,
				   const Pos::IdxPairStep& sos )
{
    data_.extend( so, sos, initVals );
}


void Pos::IdxPairValueSet::getColumn( int valnr, TypeSet<float>& vals,
				bool incudf ) const
{
    if ( valnr < 0 || valnr >= nrVals() ) return;
    SPos spos;
    while ( next(spos) )
    {
	const float* v = gtVals( spos );
	if ( incudf || !mIsUdf(v[valnr]) )
	    vals += v[ valnr ];
    }
}


void Pos::IdxPairValueSet::removeRange( int valnr, const Interval<float>& rg,
				 bool inside )
{
    if ( valnr < 0 || valnr >= nrVals() ) return;
    TypeSet<SPos> sposs; SPos spos;
    while ( next(spos) )
    {
	const float* v = gtVals( spos );
	if ( inside == rg.includes(v[valnr],true) )
	    sposs += spos;
    }
    remove( sposs );
}


Pos::IdxPairValueSet::SPos Pos::IdxPairValueSet::add( const PairVal& pv )
{
    return nrvals_ < 2 ? add(pv,pv.val()) : add(DataRow(pv));
}


Pos::IdxPairValueSet::SPos Pos::IdxPairValueSet::add( const Pos::IdxPair& ip,
								double v )
{
    return add( ip, mCast(float,v) );
}


Pos::IdxPairValueSet::SPos Pos::IdxPairValueSet::add( const Pos::IdxPair& ip,
							float v )
{
    if ( nrvals_ < 2 )
	return add( ip, nrvals_ == 1 ? &v : 0 );

    DataRow dr( ip, 1 );
    dr.value(0) = v;
    return add( dr );
}


Pos::IdxPairValueSet::SPos Pos::IdxPairValueSet::add( const Pos::IdxPair& ip,
							float v1, float v2 )
{
    if ( nrvals_ == 0 )
	return add( ip );
    else if ( nrvals_ < 3 )
    {
	float v[2]; v[0] = v1; v[1] = v2;
	return add( ip, v );
    }

    DataRow dr( ip, 2 );
    dr.value(0) = v1; dr.value(1) = v2;
    return add( dr );
}


Pos::IdxPairValueSet::SPos Pos::IdxPairValueSet::add( const Pos::IdxPair& ip,
				       const TypeSet<float>& vals )
{
    if ( vals.isEmpty() )
	return add( ip );
    else if ( vals.size() >= nrvals_ )
	return add( ip, vals.arr() );

    DataRow dr( ip, vals.size() );
    dr.setVals( vals.arr() );
    return add( dr );
}


void Pos::IdxPairValueSet::get( const SPos& spos, DataRow& dr ) const
{
    dr.setSize( nrvals_ );
    get( spos, dr, dr.values() );
}


void Pos::IdxPairValueSet::get( const SPos& spos, PairVal& pv ) const
{
    if ( nrvals_ < 2 )
	get( spos, pv, &pv.val() );
    else
    {
	DataRow dr; get( spos, dr );
	pv.set( dr ); pv.set( dr.value(0) );
    }
}


void Pos::IdxPairValueSet::get( const SPos& spos, Pos::IdxPair& ip,
					float& v ) const
{
    if ( nrvals_ < 2 )
	get( spos, ip, &v );
    else
    {
	DataRow dr; get( spos, dr );
	ip = dr; v = dr.value(0);
    }
}


void Pos::IdxPairValueSet::get( const SPos& spos, Pos::IdxPair& ip,
				float& v1, float& v2 ) const
{
    if ( nrvals_ < 3 )
    {
	float v[2]; get( spos, ip, v );
	v1 = v[0]; v2 = v[1];
    }
    else
    {
	DataRow dr; get( spos, dr );
	ip = dr; v1 = dr.value(0); v2 = dr.value(1);
    }
}


void Pos::IdxPairValueSet::get( const SPos& spos,
				TypeSet<float>& vals, int maxnrvals ) const
{
   if ( maxnrvals > nrvals_ )
	maxnrvals = nrvals_;
    if ( maxnrvals < 0 )
	maxnrvals = nrvals_ + maxnrvals + 1;
    if ( maxnrvals < 1 )
	{ vals.setEmpty(); return; }

    if ( vals.size() != maxnrvals )
    {
	vals.setEmpty();
	for ( int idx=0; idx<maxnrvals; idx++ )
	    vals += mUdf(float);
    }
    get( spos, vals.arr(), maxnrvals );
}


void Pos::IdxPairValueSet::get( const SPos& spos, Pos::IdxPair& ip,
				TypeSet<float>& vals, int maxnrvals ) const
{
    ip = getIdxPair( spos );
    get( spos, vals, maxnrvals );
}


void Pos::IdxPairValueSet::set( const SPos& spos, float v )
{
    if ( nrvals_ < 1 ) return;

    if ( nrvals_ == 1 )
	set( spos, &v );
    else
    {
	TypeSet<float> vals( nrvals_, mUdf(float) );
	vals[0] = v; set( spos, vals );
    }
}


void Pos::IdxPairValueSet::set( const SPos& spos, float v1, float v2 )
{
    if ( nrvals_ < 1 )
	return;
    else if ( nrvals_ < 3 )
    {
	float v[2]; v[0] = v1; v[1] = v2;
	set( spos, v );
    }
    else
    {
	TypeSet<float> vals( nrvals_, mUdf(float) );
	vals[0] = v1;
	if ( nrvals_ > 1 )
	    vals[1] = v2;
	set( spos, vals );
    }
}


void Pos::IdxPairValueSet::set( const SPos& spos, const TypeSet<float>& v )
{
    if ( nrvals_ < 1 ) return;

    if ( nrvals_ <= v.size() )
	set( spos, v.arr() );
    else
    {
	TypeSet<float> vals( nrvals_, mUdf(float) );
	for ( int idx=0; idx<v.size(); idx++ )
	    vals[idx] = v[idx];

	set( spos, vals.arr() );
    }
}


void Pos::IdxPairValueSet::fillPar( IOPar& iop, const char* ky ) const
{
    FileMultiString fms;
    fms += nrvals_; fms += allowsDuplicateIdxPairs() ? "D" : "N";
    BufferString key; if ( ky && *ky ) { key = ky; key += ".Setup"; }
    iop.set( key, fms );

    SPos spos;
    IdxType prevfirst = mUdf( IdxType );
    while( next(spos) )
    {
	const IdxPair ip = getIdxPair( spos );

	if ( ip.first != prevfirst )
	{
	    if ( !mIsUdf(prevfirst) )
	    {
		if ( ky && *ky )
		    { key = ky; key += "."; }
		else
		    key.setEmpty();
		key += prevfirst;
		iop.set( key, fms );
	    }
	    fms.setEmpty(); fms += ip.first; prevfirst = ip.first;
	}

	fms += ip.second;
	if ( nrvals_ > 0 )
	{
	    const float* v = gtVals( spos );
	    for ( int idx=0; idx<nrvals_; idx++ )
		fms += v[idx];
	}
    }
}


void Pos::IdxPairValueSet::usePar( const IOPar& iop, const char* iopky )
{
    FileMultiString fms;
    BufferString key;
    const bool haveiopky = iopky && *iopky;
    if ( haveiopky )
	key.set( iopky ).add( ".Setup" );
    const char* res = iop.find( key );
    if ( res && *res )
    {
	setEmpty();
	fms = res;
	setNrVals( fms.getIValue(0) );
	allowDuplicateIdxPairs( *fms[1] == 'D' );
    }

    DataRow dr( 0, 0, nrvals_ );
    for ( int ifrst=0; ; ifrst++ )
    {
	if ( haveiopky )
	    key.set( iopky ).add( "." );
	else
	    key.setEmpty();
	key += ifrst;
	res = iop.find( key );
	if ( !res )
	    return;
	if ( !*res )
	    continue;

	fms = res;
	dr.first = fms.getIValue( 0 );
	int nrpos = (fms.size() - 1) / (nrvals_ + 1);
	for ( int iscnd=0; iscnd<nrpos; iscnd++ )
	{
	    int fmsidx = 1 + iscnd * (nrvals_ + 1);
	    dr.second = fms.getIValue( fmsidx );
	    fmsidx++;
	    for ( int ival=0; ival<nrvals_; ival++ )
		dr.value(ival) = fms.getFValue( fmsidx+ival );
	    add( dr );
	}
    }
}


bool Pos::IdxPairValueSet::haveDataRow( const DataRow& dr ) const
{
    SPos spos = find( dr );
    bool found = false;
    IdxPair tmpip;
    while ( !found && spos.isValid() )
    {
	if ( getIdxPair(spos) != dr )
	    break;

	TypeSet<float> setvals;
	get( spos, setvals );
	if ( setvals.size() == dr.size() )
	{
	    bool diff = false;
	    for ( int idx=0; idx<setvals.size(); idx++ )
	    {
		if ( dr.value(idx) != setvals[idx] )
		    diff = true;
	    }
	    found = !diff;
	}
	next( spos );
    }

    return found;
}
