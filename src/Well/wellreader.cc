/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : A.H. Bril
 * DATE     : Aug 2003
-*/


#include "wellreader.h"
#include "wellodreader.h"
#include "wellioprov.h"
#include "welltransl.h"

#include "ascstream.h"
#include "bufstringset.h"
#include "file.h"
#include "filepath.h"
#include "genc.h"
#include "iopar.h"
#include "ioobj.h"
#include "ioman.h"
#include "keystrs.h"
#include "ptrman.h"
#include "separstr.h"
#include "survinfo.h"
#include "welldata.h"
#include "welltrack.h"
#include "welllog.h"
#include "welllogset.h"
#include "welld2tmodel.h"
#include "wellmarker.h"
#include "welldisp.h"
#include "od_istream.h"


const char* Well::odIO::sKeyWell()	{ return "Well"; }
const char* Well::odIO::sKeyTrack()	{ return "Track"; }
const char* Well::odIO::sKeyLog()	{ return "Well Log"; }
const char* Well::odIO::sKeyMarkers()	{ return "Well Markers"; }
const char* Well::odIO::sKeyD2T()	{ return "Depth2Time Model"; }
const char* Well::odIO::sKeyDispProps()	{ return "Display Properties"; }
const char* Well::odIO::sExtWell()	{ return ".well"; }
const char* Well::odIO::sExtLog()	{ return ".wll"; }
const char* Well::odIO::sExtMarkers()	{ return ".wlm"; }
const char* Well::odIO::sExtD2T()	{ return ".wlt"; }
const char* Well::odIO::sExtCSMdl()	{ return ".csmdl"; }
const char* Well::odIO::sExtDispProps()	{ return ".disp"; }
const char* Well::odIO::sExtWellTieSetup() { return ".tie"; }


static const char* sOperReadD2T = "read valid Time/Depth relation";


bool Well::ReadAccess::addToLogSet( Well::Log* newlog ) const
{
    if ( !newlog )
	return false;

    newlog->removeTopBottomUdfs();
    newlog->updateAfterValueChanges();
    if ( newlog->isEmpty() )
	return false;

    wd_.logs().add( newlog );
    return true;
}


bool Well::ReadAccess::updateDTModel( D2TModel* dtmodel, bool ischeckshot,
				      uiString& errmsg ) const

{
    uiString msg;
    if ( !dtmodel || !dtmodel->ensureValid(wd_,msg) )
    {
	delete dtmodel;
	errmsg = msg; // need translated string
	return false;
    }

    if ( ischeckshot )
	wd_.setCheckShotModel( dtmodel );
    else
	wd_.setD2TModel( dtmodel );

    return true;
}


bool Well::ReadAccess::updateDTModel( D2TModel* dtmodel, bool ischeckshot,
				      BufferString& errmsg ) const

{
    uiString msg;
    if ( !dtmodel || !dtmodel->ensureValid(wd_,msg) )
    {
	delete dtmodel;
	errmsg = mFromUiStringTodo(msg); // need translated string
	return false;
    }

    if ( ischeckshot )
	wd_.setCheckShotModel( dtmodel );
    else
	wd_.setD2TModel( dtmodel );

    return true;
}


Well::Reader::Reader( const IOObj& ioobj, Well::Data& wd )
    : ra_(0)
{
    init( ioobj, wd );
}


Well::Reader::Reader( const MultiID& ky, Well::Data& wd )
    : ra_(0)
{
    IOObj* ioobj = IOM().get( ky );
    if ( !ioobj )
	errmsg_.set( "Cannot find well ID " ).add( ky ).add( " in data store" );
    else
    {
	init( *ioobj, wd );
	delete ioobj;
    }
}


void Well::Reader::init( const IOObj& ioobj, Well::Data& wd )
{
    if ( ioobj.group() != mTranslGroupName(Well) )
	errmsg_.set( ioobj.name() ).add( " is not a Well, but a " )
		.add( ioobj.group() );
    else
    {
	ra_ = WDIOPF().getReadAccess( ioobj, wd, errmsg_ );
	if ( !ra_ )
	    errmsg_.set( "Cannot create reader of type " )
		   .add(ioobj.translator());
    }
}


Well::Reader::~Reader()
{
    delete ra_;
}


#define mImplWRFn(rettyp,fnnm,typ,arg,udf) \
rettyp Well::Reader::fnnm( typ arg ) const \
{ return ra_ ? ra_->fnnm(arg) : udf; }
#define mImplSimpleWRFn(fnnm) \
bool Well::Reader::fnnm() const { return ra_ ? ra_->fnnm() : false; }

mImplSimpleWRFn(get)
mImplSimpleWRFn(getInfo)
mImplSimpleWRFn(getTrack)
mImplSimpleWRFn(getLogs)
mImplSimpleWRFn(getMarkers)
mImplSimpleWRFn(getD2T)
mImplSimpleWRFn(getCSMdl)
mImplSimpleWRFn(getDispProps)

mImplWRFn(bool,getLog,const char*,lognm,false)
void Well::Reader::getLogInfo( BufferStringSet& lognms ) const
{ if ( ra_ ) ra_->getLogInfo( lognms ); }
Well::Data* Well::Reader::data()
{ return ra_ ? &ra_->data() : 0; }


bool Well::Reader::getMapLocation( Coord& coord ) const
{
    if ( !data() || !getInfo() )
	return false;

    const Well::Data& wd = *data();
    coord = wd.info().surfacecoord;
    return true;
}



Well::odIO::odIO( const char* f, BufferString& e )
    : basenm_(f)
    , errmsg_(e)
{
    FilePath fp( basenm_ );
    fp.setExtension( 0, true );
    const_cast<BufferString&>(basenm_) = fp.fullPath();
}


const char* Well::odIO::getFileName( const char* ext, int nr ) const
{
    return mkFileName( basenm_, ext, nr );
}


const char* Well::odIO::mkFileName( const char* bnm, const char* ext, int nr )
{
    mDeclStaticString( fnm );
    fnm = bnm;
    if ( nr )
	{ fnm += "^"; fnm += nr; }
    fnm += ext;
    return fnm;
}


const char* Well::odIO::getMainFileName( const IOObj& ioobj )
{
    return ioobj.fullUserExpr( true );
}


const char* Well::odIO::getMainFileName( const MultiID& mid )
{
    PtrMan<IOObj> ioobj = IOM().get( mid );
    if ( !ioobj ) return 0;
    return getMainFileName( *ioobj );
}


#define mErrRetFnmOper(fnm,oper) \
{ errmsg_.set( "Cannot " ).add( oper ).add( " " ).add( fnm ); return false; }


bool Well::odIO::removeAll( const char* ext ) const
{
    for ( int idx=1; ; idx++ )
    {
	BufferString fnm( getFileName(ext,idx) );
	if ( !File::exists(fnm) )
	    break;
	else if ( !File::remove(fnm) )
	    mErrRetFnmOper( fnm, "remove" )
    }
    return true;
}


static const char* rdHdr( od_istream& strm, const char* fileky,
			  double& ver )
{
    ascistream astrm( strm, true );
    if ( !astrm.isOfFileType(fileky) )
    {
	BufferString msg( strm.fileName(), " has type '" );
	msg += astrm.fileType(); msg += "' whereas it should be '";
	msg += fileky; msg += "'";
	ErrMsg( msg );
	return 0;
    }

    ver = (double)astrm.majorVersion() +
	  ((double)astrm.minorVersion() / (double)10);
    mDeclStaticString( hdrln ); hdrln = astrm.headerStartLine();
    return hdrln.buf();
}


#define mGetInpStream(ext,nr,seterr,todo) \
    errmsg_.setEmpty(); \
    od_istream strm( getFileName(ext,nr) ); \
    if ( !strm.isOK() ) \
    { \
	if ( seterr ) \
	{ \
	    errmsg_.set( "Cannot open " ).add( strm.fileName() ); \
	    strm.addErrMsgTo( errmsg_ ); \
	} \
	todo; \
    }



Well::odReader::odReader( const char* f, Well::Data& w, BufferString& e )
    : Well::odIO(f,e)
    , Well::ReadAccess(w)
{
    FilePath fp( f );
    fp.setExtension( 0 );
    wd_.info().setName( fp.fileName() );
}


Well::odReader::odReader( const IOObj& ioobj, Well::Data& w, BufferString& e )
    : Well::odIO(ioobj.fullUserExpr(true),e)
    , Well::ReadAccess(w)
{
    wd_.info().setName( ioobj.name() );
    wd_.setMultiID( ioobj.key() );
}


bool Well::odReader::get() const
{
    wd_.setD2TModel( 0 );
    wd_.setCheckShotModel( 0 );
    if ( !getTrack() || !getInfo() ) //Keep first
	return false;

    if ( SI().zIsTime() && !getD2T() )
	return false;

    getCSMdl();
    getLogs();
    getMarkers();

    getDispProps();
    return true;
}


bool Well::odReader::getInfo() const
{
    mGetInpStream( sExtWell(), 0, true, return false );

    wd_.info().source_.set( getFileName(sExtWell()) );
    return getInfo( strm );
}


#define mErrStrmOper(oper,todo) \
{ errmsg_.set( "Cannot " ).add( oper ).add( " for " ).add( strm.fileName() ); \
    strm.addErrMsgTo( errmsg_ ); todo; }
#define mErrRetStrmOper(oper) mErrStrmOper(oper,return false)

static const char* sKeyOldreplvel()	{ return "Replacement velocity"; }
static const char* sKeyOldgroundelev()	{ return "Ground Level elevation"; }

bool Well::odReader::getInfo( od_istream& strm ) const
{
    double version = 0.0;
    const char* hdrln = rdHdr( strm, sKeyWell(), version );
    if ( !hdrln )
	mErrRetStrmOper( "read header" )
    bool badhdr = *hdrln != 'd';
    if ( !badhdr )
    {
	if ( *(hdrln+1) == 'G' )
	{
	    errmsg_.set( "Cannot read old time wells" );
	    strm.addErrMsgTo( errmsg_ );
	    return false;
	}
	else if ( *(hdrln+1) != 'T' )
	    badhdr = true;
    }
    if ( badhdr )
	mErrRetStrmOper("find proper file header in main well file")

    ascistream astrm( strm, false );
    while ( !atEndOfSection(astrm.next()) )
    {
	if ( astrm.hasKeyword(Well::Info::sKeyUwid()) )
	    wd_.info().uwid = astrm.value();
	else if ( astrm.hasKeyword(Well::Info::sKeyOper()) )
	    wd_.info().oper = astrm.value();
	else if ( astrm.hasKeyword(Well::Info::sKeyState()) )
	    wd_.info().state = astrm.value();
	else if ( astrm.hasKeyword(Well::Info::sKeyCounty()) )
	    wd_.info().county = astrm.value();
	else if ( astrm.hasKeyword(Well::Info::sKeyCoord()) )
	    wd_.info().surfacecoord.fromString( astrm.value() );
	else if ( astrm.hasKeyword(sKeyOldreplvel()) ||
		  astrm.hasKeyword(Well::Info::sKeyReplVel()) )
	    wd_.info().replvel = astrm.getFValue();
	else if ( astrm.hasKeyword(sKeyOldgroundelev()) ||
		  astrm.hasKeyword(Well::Info::sKeyGroundElev()) )
	    wd_.info().groundelev = astrm.getFValue();
    }

    Coord surfcoord = wd_.info().surfacecoord;
    if ( (mIsZero(surfcoord.x,0.001) && mIsZero(surfcoord.x,0.001))
	    || (mIsUdf(surfcoord.x) && mIsUdf(surfcoord.x)) )
    {
	if ( !getTrack(strm) )
	    return false;

	wd_.info().surfacecoord = wd_.track().pos( 0 );
    }

    return true;
}


bool Well::odReader::getTrack( od_istream& strm ) const
{
    Coord3 c, c0; float dah;
    wd_.track().setEmpty();
    while ( strm.isOK() )
    {
	strm >> c.x >> c.y >> c.z >> dah;
	if ( !strm.isOK() || c.distTo(c0) < 1 ) break;
	wd_.track().addPoint( c, (float) c.z, dah );
    }
    if ( wd_.track().isEmpty() )
	mErrRetStrmOper( "find track data" )

    return true;
}


bool Well::odReader::getTrack() const
{
    od_istream strm( getFileName(sExtWell(),0) );
    if ( !strm.isOK() )
	mErrRetStrmOper( "find valid main well file" )

    ascistream astrm( strm );
    const double version = (double)astrm.majorVersion() +
			   ((double)astrm.minorVersion()/(double)10);
    IOPar dum; dum.getFrom( astrm );

    const bool isok = getTrack( strm );
    if ( SI().zInFeet() && version < 4.195 )
    {
	Well::Track& welltrack = wd_.track();
	for ( int idx=0; idx<welltrack.size(); idx++ )
	{
	    Coord3 pos = welltrack.pos( idx );
	    pos.z *= mToFeetFactorF;
	    welltrack.setPoint( idx, pos, (float) pos.z );
	}
    }

    return isok;
}


void Well::odReader::getLogInfo( BufferStringSet& nms ) const
{
    TypeSet<int> idxs;
    getLogInfo( nms, idxs );
}


void Well::odReader::getLogInfo( BufferStringSet& nms,
				 TypeSet<int>& idxs ) const
{
    for ( int idx=1;  ; idx++ )
    {
	mGetInpStream( sExtLog(), idx, false, break );

	double version = 0.0;
	if ( rdHdr(strm,sKeyLog(),version) )
	{
	    int bintyp = 0;
	    PtrMan<Well::Log> log = rdLogHdr( strm, bintyp, idx-1 );
	    if ( nms.isPresent(log->name()) )
	    {
		BufferString msg( log->name() );
		msg += " already present in the list, won't be read";
		ErrMsg( msg );
	    }
	    else
	    {
		nms.add( log->name() );
		idxs += idx;
	    }
	}
    }
}


bool Well::odReader::getLog( const char* lognm ) const
{
    BufferStringSet nms;
    TypeSet<int> idxs;
    getLogInfo( nms, idxs );
    const int lognmidx = nms.indexOf( lognm );
    if ( lognmidx<0 ) return false;

    const int logfileidx = idxs[lognmidx];
    mGetInpStream( sExtLog(), logfileidx, true, return false );
    return addLog( strm );
}


bool Well::odReader::getLogs() const
{
    bool rv = true;
    wd_.logs().setEmpty();
    for ( int idx=1;  ; idx++ )
    {
	mGetInpStream( sExtLog(), idx, false, break );

	if ( !addLog(strm) )
	    mErrStrmOper("read data",
		    ErrMsg(errmsg_); errmsg_.setEmpty(); rv = false; continue)
    }

    return rv;
}


Well::Log* Well::odReader::rdLogHdr( od_istream& strm, int& bintype, int idx )
{
    Well::Log* newlog = new Well::Log;
    ascistream astrm( strm, false );
    bool havehdrinfo = false;
    bintype = 0;
    while ( !atEndOfSection(astrm.next()) )
    {
	if ( astrm.hasKeyword(sKey::Name()) )
	    newlog->setName( astrm.value() );
	if ( astrm.hasKeyword(Well::Log::sKeyUnitLbl()) )
	    newlog->setUnitMeasLabel( astrm.value() );
	if ( astrm.hasKeyword(Well::Log::sKeyHdrInfo()) )
	    havehdrinfo = astrm.getYN();
	if ( astrm.hasKeyword(Well::Log::sKeyStorage()) )
	    bintype = *astrm.value() == 'B' ? 1
		    : (*astrm.value() == 'S' ? -1 : 0);
    }
    if ( newlog->name().isEmpty() )
    {
	BufferString nm( "[" ); nm += idx+1; nm += "]";
	newlog->setName( nm );
    }

    if ( havehdrinfo )
	newlog->pars().getFrom( astrm );

    return newlog;
}


bool Well::odReader::addLog( od_istream& strm ) const
{
    double version = 0.0;
    if ( !rdHdr(strm,sKeyLog(),version) )
	return false;

    int bintype = 0;
    Well::Log* newlog = rdLogHdr( strm, bintype, wd_.logs().size() );
    if ( !newlog )
	mErrRetStrmOper( "read log" )

    readLogData( *newlog, strm, bintype );

    if ( SI().zInFeet() && version < 4.195 )
    {
	for ( int idx=0; idx<newlog->size(); idx++ )
	    newlog->dahArr()[idx] = newlog->dah(idx) * mToFeetFactorF;
    }

    if ( wd_.track().isEmpty() )
	getTrack();

    const Interval<float> trackdahrg = wd_.track().dahRange();
    const bool havetrack = !wd_.track().isEmpty();
    for ( int idx=newlog->size()-1; idx>=0; idx-- )
    {
	const float curdah = newlog->dahArr()[idx];
	if ( !trackdahrg.includes(curdah,false) && havetrack )
	    newlog->valArr()[idx] = mUdf(float);
    }

    return addToLogSet( newlog );
}


void Well::odReader::readLogData( Well::Log& wl, od_istream& strm,
				int bintype ) const
{

    float v[2];
    while ( strm.isOK() )
    {
	if ( !bintype )
	    strm >> v[0] >> v[1];
	else
	{
	    strm.getBin( v, 2 * sizeof(float) );
	    if ( (bintype > 0) != __islittle__ )
	    {
		SwapBytes( v, sizeof(float) );
		SwapBytes( v+1, sizeof(float) );
	    }
	}
	if ( !strm.isOK() ) break;

	wl.addValue( v[0], v[1] );
    }
}


bool Well::odReader::getMarkers() const
{
    mGetInpStream( sExtMarkers(), 0, true, return false );
    return getMarkers( strm );
}


bool Well::odReader::getMarkers( od_istream& strm ) const
{
    double version = 0.0;
    if ( !rdHdr(strm,sKeyMarkers(),version) )
	mErrRetStrmOper( "read header" )

    ascistream astrm( strm, false );

    IOPar iopar( astrm );
    if ( iopar.isEmpty() )
	mErrRetStrmOper( "find anything in file" )

    if ( wd_.track().isEmpty() )
    {
	if ( !getTrack() )
	    return false;
    }

    const Interval<float> trackdahrg = wd_.track().dahRange();
    const bool havetrack = !wd_.track().isEmpty();
    wd_.markers().erase();
    BufferString bs;
    for ( int idx=1;  ; idx++ )
    {
	BufferString basekey; basekey += idx;
	BufferString key = IOPar::compKey( basekey, sKey::Name() );
	if ( !iopar.get(key,bs) ) break;

	Well::Marker* wm = new Well::Marker( bs );

	key = IOPar::compKey( basekey, Well::Marker::sKeyDah() );
	if ( !iopar.get(key,bs) )
	    { delete wm; continue; }
	const float val = bs.toFloat();
	wm->setDah( (SI().zInFeet() && version<4.195) ? (val*mToFeetFactorF)
						      : val );
	key = IOPar::compKey( basekey, sKey::StratRef() );
	int lvlid = -1; iopar.get( key, lvlid );
	wm->setLevelID( lvlid );

	key = IOPar::compKey( basekey, sKey::Color() );
	if ( iopar.get(key,bs) )
	{
	    Color col( wm->color() );
	    col.use( bs.buf() );
	    wm->setColor( col );
	}

	if ( !trackdahrg.includes(wm->dah(),false) && havetrack )
	    delete wm;
	else
	    wd_.markers().insertNew( wm );
    }

    return true;
}


bool Well::odReader::getD2T() const	{ return doGetD2T( false ); }
bool Well::odReader::getCSMdl() const	{ return doGetD2T( true ); }
bool Well::odReader::doGetD2T( bool csmdl ) const
{
    mGetInpStream( csmdl ? sExtCSMdl() : sExtD2T(), 0, true, return false );
    return doGetD2T( strm, csmdl );
}


bool Well::odReader::getD2T( od_istream& strm ) const
{ return doGetD2T(strm,false); }
bool Well::odReader::getCSMdl( od_istream& strm ) const
{ return doGetD2T(strm,true); }
bool Well::odReader::doGetD2T( od_istream& strm, bool csmdl ) const
{
    double version = 0.0;
    if ( !rdHdr(strm,sKeyD2T(),version) )
	mErrRetStrmOper( "read D/T file header" )

    ascistream astrm( strm, false );
    Well::D2TModel* d2t = new Well::D2TModel;
    while ( !atEndOfSection(astrm.next()) )
    {
	if ( astrm.hasKeyword(sKey::Name()) )
	    d2t->setName( astrm.value() );
	else if ( astrm.hasKeyword(sKey::Desc()) )
	    d2t->desc = astrm.value();
	else if ( astrm.hasKeyword(Well::D2TModel::sKeyDataSrc()) )
	    d2t->datasource = astrm.value();
    }

    float dah, val;
    while ( strm.isOK() )
    {
	strm >> dah >> val;
	if ( !strm.isOK() )
	    break;
	d2t->add( dah, val );
    }

    if ( wd_.track().isEmpty() )
    {
	if ( !getTrack() || !getInfo() )
	    return false;
    }
    if ( !updateDTModel(d2t,csmdl,errmsg_) )
	mErrRetStrmOper( sOperReadD2T )

    return true;
}


bool Well::odReader::getDispProps() const
{
    mGetInpStream( sExtDispProps(), 0, true, return false );
    return getDispProps( strm );
}


bool Well::odReader::getDispProps( od_istream& strm ) const
{
    double version = 0.0;
    if ( !rdHdr(strm,sKeyDispProps(),version) )
	mErrRetStrmOper( "read well properties header" )

    ascistream astrm( strm, false );
    IOPar iop; iop.getFrom( astrm );
    wd_.displayProperties(true).usePar( iop );
    wd_.displayProperties(false).usePar( iop );
    return true;
}
