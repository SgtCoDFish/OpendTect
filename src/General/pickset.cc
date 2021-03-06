/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : Bert
 * DATE     : Mar 2001 / Mar 2016
-*/


#include "picksetmgr.h"
#include "ioman.h"
#include "iopar.h"
#include "polygon.h"
#include "survinfo.h"
#include "tabledef.h"
#include "posimpexppars.h"
#include "unitofmeasure.h"
#include "od_iostream.h"
#include <ctype.h>

static const char* sKeyConnect = "Connect";


// Pick::SetMgr
Pick::SetMgr& Pick::SetMgr::getMgr( const char* nm )
{
    mDefineStaticLocalObject( PtrMan<ManagedObjectSet<SetMgr> >, mgrs, = 0 );
    SetMgr* newmgr = 0;
    if ( !mgrs )
    {
	mgrs = new ManagedObjectSet<SetMgr>;
	newmgr = new SetMgr( 0 );
	    // ensure the first mgr has the 'empty' name
    }
    else if ( (!nm || !*nm) )
	return *((*mgrs)[0]);
    else
    {
	for ( int idx=1; idx<mgrs->size(); idx++ )
	{
	    if ( (*mgrs)[idx]->name() == nm )
		return *((*mgrs)[idx]);
	}
    }

    if ( !newmgr )
	newmgr = new SetMgr( nm );

    *mgrs += newmgr;
    return *newmgr;
}


Pick::SetMgr::SetMgr( const char* nm )
    : NamedObject(nm)
    , locationChanged(this), setToBeRemoved(this)
    , setAdded(this), setChanged(this)
    , setDispChanged(this)
    , undo_( *new Undo() )
{
    mAttachCB( IOM().entryRemoved, SetMgr::objRm );
    mAttachCB( IOM().surveyToBeChanged, SetMgr::survChg );
    mAttachCB( IOM().applicationClosing, SetMgr::survChg );
}


Pick::SetMgr::~SetMgr()
{
    detachAllNotifiers();
    undo_.removeAll();
    delete &undo_;
}


void Pick::SetMgr::add( const MultiID& ky, Set* st )
{
    pss_ += st; ids_ += ky; changed_ += false;
    setAdded.trigger( st );
}


void Pick::SetMgr::set( const MultiID& ky, Set* newset )
{
    Set* oldset = find( ky );
    if ( !oldset )
    {
	if ( newset )
	    add( ky, newset );
    }
    else if ( newset != oldset )
    {
	const int idx = pss_.indexOf( oldset );
	//Must be removed from list before trigger, otherwise
	//other users may remove it in calls invoked by the cb.
	pss_.removeSingle( idx );
	setToBeRemoved.trigger( oldset );
	delete oldset;
	ids_.removeSingle( idx );
	changed_.removeSingle( idx );
	if ( newset )
	    add( ky, newset );
    }
}


const MultiID& Pick::SetMgr::id( int idx ) const
{ return ids_.validIdx(idx) ? ids_[idx] : MultiID::udf(); }


void Pick::SetMgr::setID( int idx, const MultiID& mid )
{
    ids_[idx] = mid;
}


int Pick::SetMgr::indexOf( const Set& st ) const
{
    return pss_.indexOf( &st );
}


int Pick::SetMgr::indexOf( const MultiID& ky ) const
{
    for ( int idx=0; idx<size(); idx++ )
    {
	if ( ky == ids_[idx] )
	    return idx;
    }
    return -1;
}


int Pick::SetMgr::indexOf( const char* nm ) const
{
    for ( int idx=0; idx<size(); idx++ )
    {
	if ( pss_[idx]->name() == nm )
	    return idx;
    }
    return -1;
}


Pick::Set* Pick::SetMgr::find( const MultiID& ky ) const
{
    const int idx = indexOf( ky );
    return idx < 0 ? 0 : const_cast<Set*>( pss_[idx] );
}


MultiID* Pick::SetMgr::find( const Set& st ) const
{
    const int idx = indexOf( st );
    return idx < 0 ? 0 : const_cast<MultiID*>( &ids_[idx] );
}


Pick::Set* Pick::SetMgr::find( const char* nm ) const
{
    const int idx = indexOf( nm );
    return idx < 0 ? 0 : const_cast<Set*>( pss_[idx] );
}


void Pick::SetMgr::reportChange( CallBacker* sender, const ChangeData& cd )
{
    const int setidx = pss_.indexOf( cd.set_ );
    if ( setidx >= 0 )
    {
	changed_[setidx] = true;
	locationChanged.trigger( const_cast<ChangeData*>( &cd ), sender );
    }
}


void Pick::SetMgr::reportChange( CallBacker* sender, const Set& s )
{
    const int setidx = pss_.indexOf( &s );
    if ( setidx >= 0 )
    {
	changed_[setidx] = true;
	setChanged.trigger( const_cast<Set*>(&s), sender );
    }
}


void Pick::SetMgr::reportDispChange( CallBacker* sender, const Set& s )
{
    const int setidx = pss_.indexOf( &s );
    if ( setidx >= 0 )
    {
	changed_[setidx] = true;
	setDispChanged.trigger( const_cast<Set*>(&s), sender );
    }
}


void Pick::SetMgr::removeCBs( CallBacker* cb )
{
    locationChanged.removeWith( cb );
    setToBeRemoved.removeWith( cb );
    setAdded.removeWith( cb );
    setChanged.removeWith( cb );
    setDispChanged.removeWith( cb );
}


void Pick::SetMgr::removeAll()
{
    for ( int idx=pss_.size()-1; idx>=0; idx-- )
    {
	Set* pset = pss_.removeSingle( idx );
	setToBeRemoved.trigger( pset );
	delete pset;
    }
}


void Pick::SetMgr::survChg( CallBacker* )
{
    removeAll();
    locationChanged.cbs_.erase();
    setToBeRemoved.cbs_.erase();
    setAdded.cbs_.erase();
    setChanged.cbs_.erase();
    setDispChanged.cbs_.erase();
    ids_.erase();
    changed_.erase();
}


void Pick::SetMgr::objRm( CallBacker* cb )
{
    mCBCapsuleUnpack(MultiID,ky,cb);
    if ( indexOf(ky) >= 0 )
	set( ky, 0 );
}


namespace Pick
{

class SetKnotUndoEvent: public UndoEvent
{
public:

    enum  UnDoType	    { Insert, PolygonClose, Remove, Move };

SetKnotUndoEvent( UnDoType type, const MultiID& mid, int sidx,
		      const Location& loc )
{
    init( type, mid, sidx, loc );
}

void init( UnDoType type, const MultiID& mid, int sidx, const Location& loc )
{
    type_ = type;
    newloc_ = Coord3::udf();
    loc_ = Coord3::udf();

    mid_ = mid;
    index_ = sidx;
    Pick::SetMgr& mgr = Pick::Mgr();
    Pick::Set& set = mgr.get(mid);

    if ( type == Insert || type == PolygonClose )
    {
	if ( &set && set.size()>index_ )
	    loc_ = set[index_];
    }
    else if ( type == Remove )
    {
	loc_ = loc;
    }
    else if ( type == Move )
    {
	if ( &set && set.size()>index_ )
	    loc_ = set[index_];
	newloc_ = loc;
    }
}


const char* getStandardDesc() const
{
    if ( type_ == Insert )
	return "Insert Knot";
    else if ( type_ == Remove )
	return "Remove";
    else if ( type_ == Move )
	return "move";

    return "";
}

bool unDo()
{
    SetMgr& mgr = Pick::Mgr();
    const int setidx = mgr.indexOf( mid_ );
    if ( setidx < 0 )
	return false;
    Set& set = mgr.get( setidx );

    if ( set.disp_.connect_ == Pick::Set::Disp::Close
      && index_ == set.size()-1 && type_ != Move )
	type_ = PolygonClose;

    SetMgr::ChangeData::Ev ev =
	    type_ == Move   ? SetMgr::ChangeData::Changed
	: ( type_ == Remove ? SetMgr::ChangeData::Added
			    : SetMgr::ChangeData::ToBeRemoved );

    SetMgr::ChangeData cd( ev, &set, index_ );

    if ( type_ == Move )
    {
       if ( &set && set.size()>index_  && loc_.pos().isDefined() )
	   set[index_] = loc_;
    }
    else if ( type_ == Remove )
    {
       if ( loc_.pos().isDefined() )
	 set.insert( index_, loc_ );
    }
    else if ( type_ == Insert  )
    {
	set.removeSingle( index_ );
    }
    else if ( type_ == PolygonClose )
    {
	set.disp_.connect_ = Pick::Set::Disp::Open;
	set.removeSingle(index_);
    }

    mgr.reportChange( 0, cd );

    return true;
}


bool reDo()
{
    SetMgr& mgr = Pick::Mgr();
    const int setidx = mgr.indexOf( mid_ );
    if ( setidx < 0 )
	return false;
    Set& set = mgr.get( setidx );

    SetMgr::ChangeData::Ev ev =
		type_ == Move	? SetMgr::ChangeData::Changed
	    : ( type_ == Remove ? SetMgr::ChangeData::ToBeRemoved
				: SetMgr::ChangeData::Added );

    Pick::SetMgr::ChangeData cd( ev, &set, index_ );

    if ( type_ == Move )
    {
	if ( &set && set.size()>index_ && newloc_.pos().isDefined() )
	    set[index_] = newloc_;
    }
    else if ( type_ == Remove )
    {
	set.removeSingle( index_ );
    }
    else if ( type_ == Insert )
    {
	if ( loc_.pos().isDefined() )
	    set.insert( index_,loc_ );
    }
    else if ( type_ == PolygonClose )
    {
	if ( loc_.pos().isDefined() )
	{
	    set.disp_.connect_=Pick::Set::Disp::Close;
	    set.insert( index_, loc_ );
	}
    }

    mgr.reportChange( 0, cd );

    return true;
}

protected:

    Location	loc_;
    Location	newloc_;
    MultiID	mid_;
    int		index_;
    UnDoType	type_;

};

} // namespace Pick


// Both Pick set types

template <class PicksType>
static typename PicksType::size_type findIdx( const PicksType& picks,
	                                      const TrcKey& tk )
{
    const typename PicksType::size_type sz = picks.size();
    for ( typename PicksType::size_type idx=0; idx<sz; idx++ )
	if ( picks.get(idx).trcKey() == tk )
	    return idx;
    return -1;
}
Pick::Set::size_type Pick::Set::find( const TrcKey& tk ) const
{ return findIdx( *this, tk ); }
Pick::List::size_type Pick::List::find( const TrcKey& tk ) const
{ return findIdx( *this, tk ); }



template <class PicksType>
static typename PicksType::size_type getNearestLocation( const PicksType& ps,
				    const Coord3& pos, bool ignorez )
{
    const typename PicksType::size_type sz = ps.size();
    if ( sz < 2 )
	return sz - 1;
    if ( pos.isUdf() )
	return 0;

    typename PicksType::size_type ret = 0;
    const Coord3& p0 = ps.get( ret ).pos();
    double minsqdist = p0.isUdf() ? mUdf(double)
		     : (ignorez ? pos.sqHorDistTo( p0 ) : pos.sqDistTo( p0 ));

    for ( typename PicksType::size_type idx=1; idx<sz; idx++ )
    {
	const Coord3& curpos = ps.get( idx ).pos();
	if ( pos.isUdf() )
	    continue;

	const double sqdist = ignorez ? pos.sqHorDistTo( curpos )
				      : pos.sqDistTo( curpos );
	if ( sqdist == 0 )
	    return idx;
	else if ( sqdist < minsqdist )
	    { minsqdist = sqdist; ret = idx; }
    }
    return ret;
}
Pick::Set::size_type Pick::Set::nearestLocation( const Coord& pos ) const
{ return getNearestLocation( *this, Coord3(pos.x,pos.y,0.f), true ); }
Pick::Set::size_type Pick::Set::nearestLocation( const Coord3& pos,
						 bool ignorez ) const
{ return getNearestLocation( *this, pos, ignorez ); }
Pick::List::size_type Pick::List::nearestLocation( const Coord& pos ) const
{ return getNearestLocation( *this, Coord3(pos.x,pos.y,0.f), true ); }
Pick::List::size_type Pick::List::nearestLocation( const Coord3& pos,
						 bool ignorez ) const
{ return getNearestLocation( *this, pos, ignorez ); }


template <class PicksType>
inline static Pos::GeomID getFirstgeomID( const PicksType& picks )
{
    return picks.isEmpty() ? false : picks.get(0).trcKey().geomID();
}
Pos::GeomID Pick::Set::firstgeomID() const { return getFirstgeomID(*this); }
Pos::GeomID Pick::List::firstgeomID() const { return getFirstgeomID(*this); }


template <class PicksType>
inline static bool getHas2D( const PicksType& picks )
{
    const typename PicksType::size_type sz = picks.size();
    if ( sz < 1 )
	return false;
    for ( typename PicksType::size_type idx=0; idx<sz; idx++ )
	if ( picks.get(idx).is2D() )
	    return true;
    return false;
}
template <class PicksType>
inline static bool getHasOnly2D( const PicksType& picks )
{
    const typename PicksType::size_type sz = picks.size();
    if ( sz < 1 )
	return false;
    for ( typename PicksType::size_type idx=0; idx<sz; idx++ )
	if ( picks.get(idx).is2D() )
	    return false;
    return true;
}
template <class PicksType>
inline static bool getHas3D( const PicksType& picks )
{
    const typename PicksType::size_type sz = picks.size();
    if ( sz < 1 )
	return true;
    for ( typename PicksType::size_type idx=0; idx<sz; idx++ )
	if ( !picks.get(idx).is2D() )
	    return true;
    return false;
}
template <class PicksType>
inline static bool getHasOnly3D( const PicksType& picks )
{
    const typename PicksType::size_type sz = picks.size();
    if ( sz < 1 )
	return true;
    for ( typename PicksType::size_type idx=0; idx<sz; idx++ )
	if ( picks.get(idx).is2D() )
	    return false;
    return true;
}
template <class PicksType>
inline static bool getIsMultiGeom( const PicksType& picks )
{
    const typename PicksType::size_type sz = picks.size();
    if ( sz < 2 )
	return false;
    const Pos::GeomID geomid0 = picks.get(0).geomID();
    for ( typename PicksType::size_type idx=1; idx<sz; idx++ )
	if ( picks.get(idx).geomID() != geomid0 )
	    return true;
    return false;
}

bool Pick::Set::isMultiGeom() const	{ return getIsMultiGeom(*this); }
bool Pick::List::isMultiGeom() const	{ return getIsMultiGeom(*this); }
bool Pick::Set::has2D() const		{ return getHas2D(*this); }
bool Pick::List::has2D() const		{ return getHas2D(*this); }
bool Pick::Set::has3D() const		{ return getHas3D(*this); }
bool Pick::List::has3D() const		{ return getHas3D(*this); }
bool Pick::Set::hasOnly2D() const	{ return getHasOnly2D(*this); }
bool Pick::List::hasOnly2D() const	{ return getHasOnly2D(*this); }
bool Pick::Set::hasOnly3D() const	{ return getHasOnly3D(*this); }
bool Pick::List::hasOnly3D() const	{ return getHasOnly3D(*this); }


// Pick::Set
mDefineEnumUtils( Pick::Set::Disp, Connection, "Connection" )
{ "None", "Open", "Close", 0 };

Pick::Set::Set( const char* nm )
    : NamedObject(nm)
    , pars_(*new IOPar)
{
}


Pick::Set::Set( const Set& s )
    : pars_(*new IOPar)
{
    *this = s;
}

Pick::Set::~Set()
{
    delete &pars_;
}


Pick::Set& Pick::Set::operator=( const Set& s )
{
    if ( &s == this ) return *this;
    copy( s ); setName( s.name() );
    disp_ = s.disp_; pars_ = s.pars_;
    return *this;
}


bool Pick::Set::isPolygon() const
{
    const FixedString typ = pars_.find( sKey::Type() );
    return typ.isEmpty() ? disp_.connect_!=Set::Disp::None
			 : typ == sKey::Polygon();
}


void Pick::Set::getPolygon( ODPolygon<double>& poly ) const
{
    const size_type sz = size();
    for ( size_type idx=0; idx<sz; idx++ )
    {
	const Coord c( (*this)[idx].pos() );
	poly.add( Geom::Point2D<double>( c.x, c.y ) );
    }
}


void Pick::Set::getLocations( ObjectSet<Location>& locs )
{
    for ( size_type idx=0; idx<size(); idx++ )
	locs += &((*this)[idx]);
}


void Pick::Set::getLocations( ObjectSet<const Location>& locs ) const
{
    for ( size_type idx=0; idx<size(); idx++ )
	locs += &((*this)[idx]);
}


float Pick::Set::getXYArea() const
{
    const size_type sz = size();
    if ( sz < 3 || disp_.connect_ == Disp::None )
	return mUdf(float);

    TypeSet<Geom::Point2D<float> > posxy;
    for ( size_type idx=sz-1; idx>=0; idx-- )
    {
	const Coord localpos = (*this)[idx].pos();
	posxy += Geom::Point2D<float>( (float)localpos.x, (float)localpos.y );
    }

    ODPolygon<float> polygon( posxy );
    if ( polygon.isSelfIntersecting() )
	return mUdf(float);

    float area = (float) polygon.area();
    if ( SI().xyInFeet() )
	area *= (mFromFeetFactorF*mFromFeetFactorF);

    return area;
}


void Pick::Set::fillPar( IOPar& par ) const
{
    BufferString parstr;
    disp_.mkstyle_.toString( parstr );
    par.set( sKey::MarkerStyle(), parstr );
    par.set( sKeyConnect, Disp::toString(disp_.connect_) );
    par.merge( pars_ );
}


bool Pick::Set::usePar( const IOPar& par )
{
    const bool v6_or_earlier = ( par.majorVersion()+par.minorVersion()*0.1 )>0
	&& ( par.majorVersion()+par.minorVersion()*0.1 )<=6;

    BufferString mkststr;
    if ( !par.get(sKey::MarkerStyle(),mkststr) && v6_or_earlier )
    {
	BufferString colstr;
	if ( par.get(sKey::Color(),colstr) )
	    disp_.mkstyle_.color_.use( colstr.buf() );
	par.get( sKey::Size(),disp_.mkstyle_.size_ );
	int type = 0;
	par.get( sKeyMarkerType(),type );
	type++;
	disp_.mkstyle_.type_ = (OD::MarkerStyle3D::Type) type;
    }
    else
    {
	disp_.mkstyle_.fromString( mkststr );
    }

    bool doconnect;
    par.getYN( sKeyConnect, doconnect );	// For Backward Compatibility
    if ( doconnect ) disp_.connect_ = Disp::Close;
    else
    {
	if ( !Disp::ConnectionDef().parse(par.find(sKeyConnect),
	     disp_.connect_) )
	    disp_.connect_ = Disp::None;
    }

    pars_ = par;
    pars_.removeWithKey( sKey::Color() );
    pars_.removeWithKey( sKey::Size() );
    pars_.removeWithKey( sKeyMarkerType() );
    pars_.removeWithKey( sKeyConnect );

    return true;
}


void Pick::Set::addUndoEvent( EventType type, size_type idx,
				const Location& loc )
{
    SetMgr& mgr = Mgr();
    if ( mgr.indexOf(*this) == -1 )
	return;

    const MultiID mid = mgr.get(*this);
    if ( !mid.isEmpty() )
    {
	SetKnotUndoEvent::UnDoType undotype = (SetKnotUndoEvent::UnDoType)type;
	const Location touse = type == Insert ? Location(Coord3::udf()) : loc;
	SetKnotUndoEvent* undo = new SetKnotUndoEvent( undotype, mid, idx,
						       touse );
	Pick::Mgr().undo().addEvent( undo, 0 );
    }
}


void Pick::Set::insertWithUndo( size_type idx, const Location& loc )
{
    insert( idx, loc );
    addUndoEvent( Insert, idx, loc );
 }


void Pick::Set::appendWithUndo( const Location& loc )
{
    *this += loc;
    addUndoEvent( Insert, size()-1, loc );
 }


void Pick::Set::removeSingleWithUndo( size_type idx )
{
    const Location loc = (*this)[idx];
    addUndoEvent( Remove, idx, loc );
    removeSingle( idx );
}


void Pick::Set::moveWithUndo( size_type idx, const Location& undoloc,
				const Location& loc )
{
    if ( size()<idx ) return;
    (*this)[idx] = undoloc;
    addUndoEvent( Move, idx, loc );
    (*this)[idx] = loc;
}


// Pick::List

Pick::List& Pick::List::add( const Location& loc, bool mkcopy )
{
    if ( mkcopy )
	*this += new Location( loc );
    else
	*this += const_cast<Location*>( &loc );
    return *this;
}


// PickSetAscIO

Table::FormatDesc* PickSetAscIO::getDesc( bool iszreq )
{
    Table::FormatDesc* fd = new Table::FormatDesc( "PickSet" );
    createDescBody( fd, iszreq );
    return fd;
}


void PickSetAscIO::createDescBody( Table::FormatDesc* fd, bool iszreq )
{
    fd->bodyinfos_ += Table::TargetInfo::mkHorPosition( true );
    if ( iszreq )
	fd->bodyinfos_ += Table::TargetInfo::mkZPosition( true );
}


void PickSetAscIO::updateDesc( Table::FormatDesc& fd, bool iszreq )
{
    fd.bodyinfos_.erase();
    createDescBody( &fd, iszreq );
}


bool PickSetAscIO::isXY() const
{
    return formOf( false, 0 ) == 0;
}


#define mErrRet(s) { if ( !s.isEmpty() ) errmsg_ = s; return 0; }

bool PickSetAscIO::get( od_istream& strm, Pick::Set& ps,
			bool iszreq, float constz ) const
{
    while ( true )
    {
	int ret = getNextBodyVals( strm );
	if ( ret < 0 ) mErrRet(errmsg_)
	if ( ret == 0 ) break;

	const double xread = getDValue( 0 );
	const double yread = getDValue( 1 );
	if ( mIsUdf(xread) || mIsUdf(yread) ) continue;

	Coord pos( xread, yread );
	mPIEPAdj(Coord,pos,true);
	if ( !isXY() || !SI().isReasonable(pos) )
	{
	    BinID bid( mNINT32(xread), mNINT32(yread) );
	    mPIEPAdj(BinID,bid,true);
	    SI().snap( bid );
	    pos = SI().transform( bid );
	}

	if ( !SI().isReasonable(pos) ) continue;

	float zread = constz;
	if ( iszreq )
	{
	    zread = getFValue( 2 );
	    if ( mIsUdf(zread) )
		continue;

	    mPIEPAdj(Z,zread,true);
	}

	ps += Pick::Location( pos, zread );
    }

    return true;
}
