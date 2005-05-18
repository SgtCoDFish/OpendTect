/*+
 * COPYRIGHT: (C) dGB Beheer B.V.
 * AUTHOR   : A.H. Bril
 * DATE     : 21-1-1998
-*/

static const char* rcsID = "$Id: seiscbvsps.cc,v 1.11 2005-05-18 09:20:45 cvsbert Exp $";

#include "seiscbvsps.h"
#include "seispsioprov.h"
#include "seiscbvs.h"
#include "cbvsreadmgr.h"
#include "seisbuf.h"
#include "filepath.h"
#include "filegen.h"
#include "survinfo.h"
#include "segposinfo.h"
#include "dirlist.h"
#include "iopar.h"
#include "errh.h"

class CBVSSeisPSIOProvider : public SeisPSIOProvider
{
public:
			CBVSSeisPSIOProvider() : SeisPSIOProvider("CBVS") {}
    SeisPSReader*	makeReader( const char* dirnm, int inl ) const
			{ return new SeisCBVSPSReader(dirnm,inl); }
    SeisPSWriter*	makeWriter( const char* dirnm ) const
			{ return new SeisCBVSPSWriter(dirnm); }
    static int		factid;
};

// This adds the CBVS type pre-stack seismics data storage to the factory
int CBVSSeisPSIOProvider::factid = SPSIOPF().add( new CBVSSeisPSIOProvider );


SeisCBVSPSIO::SeisCBVSPSIO( const char* dirnm )
    	: dirnm_(dirnm)
{
    BufferString& sm = const_cast<BufferString&>( selmask_ );
    sm = "*."; sm += CBVSSeisTrcTranslator::sKeyDefExtension;
}


SeisCBVSPSIO::~SeisCBVSPSIO()
{
}


SeisCBVSPSReader::SeisCBVSPSReader( const char* dirnm, int inl )
    	: SeisCBVSPSIO(dirnm)
    	, posdata_(*new PosInfo::CubeData)
    	, curtr_(0)
    	, curinl_(mUdf(int))
{
    if ( !File_isDirectory(dirnm_) )
    {
	errmsg_ = "Directory '"; errmsg_ += dirnm_;
	errmsg_ += "' does not exist";
	return;
    }

    if ( Values::isUdf(inl) )
    {
	DirList dl( dirnm_, DirList::FilesOnly, selmask_.buf() );
	for ( int idx=0; idx<dl.size(); idx++ )
	{
	    BufferString fnm( dl.get(idx) );
	    char* ptr = fnm.buf();
	    while ( *ptr && !isdigit(*ptr) ) ptr++;
	    while ( *ptr && isdigit(*ptr) ) ptr++;
	    *ptr = '\0';
	    if ( fnm == "" ) continue;

	    addInl( atoi(fnm.buf()) );
	}

	if ( posdata_.size() < 1 )
	{
	    errmsg_ = "Directory '"; errmsg_ += dirnm_;
	    errmsg_ += "' contains no usable pre-stack data files";
	    return;
	}
	posdata_.sort();
    }
    else if ( inl >= 0 )
	addInl( inl );
}


SeisCBVSPSReader::~SeisCBVSPSReader()
{
    delete &posdata_;
}


void SeisCBVSPSReader::addInl( int inl )
{
    if ( !mkTr(inl) ) return;

    PosInfo::InlData* newid = new PosInfo::InlData( inl );
    const CBVSInfo::SurvGeom& sg = curtr_->readMgr()->info().geom;

    if ( sg.fullyrectandreg )

	newid->segments += PosInfo::InlData::Segment( sg.start.inl,
					sg.stop.inl, sg.step.inl );
    else
    {
	const PosInfo::CubeData& cd = sg.cubedata;
	if ( cd.size() < 1 )
	    { pErrMsg("Huh? should get error earlier"); delete newid; return; }

	PosInfo::InlData::Segment seg( cd[0]->inl, cd[0]->inl, 1 );
	if ( cd.size() > 1 )
	{
	    seg.stop = cd[1]->inl;
	    seg.step = seg.stop - seg.start;
	    for ( int idx=2; idx<cd.size(); idx++ )
	    {
		const PosInfo::InlData& id = *cd[idx];
		if ( seg.stop == seg.start )
		    { seg.stop = id.inl; seg.step = seg.stop - seg.start; }
		else if ( id.inl != cd[idx-1]->inl + seg.step )
		{
		    newid->segments += seg;
		    seg.start = seg.stop = id.inl;
		}
	    }
	    newid->segments += seg;
	}
    }

    posdata_ += newid;
}


bool SeisCBVSPSReader::mkTr( int inl ) const
{
    if ( curtr_ && curinl_ == inl )
	return true;
    else if ( Values::isUdf(inl) )
	return false;

    delete curtr_; curtr_ = 0;
    curinl_ = inl;

    FilePath fp( dirnm_ );
    BufferString fnm = inl; fnm += ext();
    fp.add( fnm );

    BufferString filep = fp.fullPath();

    if( !File_exists( (const char*)filep ) )
    {
	errmsg_ = "No Pre Stack data available for that inline.\n";
	errmsg_ += "Please choose another inline.";
	return false;
    }

    errmsg_ = "";
    curtr_ = CBVSSeisTrcTranslator::make( fp.fullPath(), false, false,
					  &errmsg_ );
    return curtr_;
}


bool SeisCBVSPSReader::getGather( int crl, SeisTrcBuf& gath ) const
{
    gath.deepErase();
    if ( !curtr_->goTo( BinID(crl,1) ) )
	{ errmsg_ = "Crossline not present"; return false; }

    const BinID bid( curinl_, crl );
    const Coord coord = SI().transform( bid );
    while ( true )
    {
	SeisTrc* trc = new SeisTrc;
	if ( !curtr_->read(*trc) )
	{
	    delete trc;
	    errmsg_ = curtr_->errMsg();
	    return errmsg_ == "";
	}
	else if ( trc->info().binid.inl != bid.crl )
	    { delete trc; return true; }

	trc->info().nr = trc->info().binid.crl;
	trc->info().binid = bid; trc->info().coord = coord;
	gath.add( trc );
    }

    // Not reached
    return true;
}


bool SeisCBVSPSReader::getGather( const BinID& bid, SeisTrcBuf& gath ) const
{
    return mkTr( bid.inl ) && getGather( bid.crl, gath );
}


SeisCBVSPSWriter::SeisCBVSPSWriter( const char* dirnm )
    	: SeisCBVSPSIO(dirnm)
    	, reqdtype_(DataCharacteristics::Auto)
    	, tr_(0)
    	, prevbid_(*new BinID(mUdf(int),mUdf(int)))
	, nringather_(1)
{
    if ( !File_isDirectory(dirnm_) )
    {
	if ( File_exists(dirnm_) )
	{
	    errmsg_ = "Existing file '";
	    errmsg_ += dirnm_; errmsg_ += "'. Remove or rename.";
	    return;
	}
	File_createDir(dirnm_,0);
    }
}


SeisCBVSPSWriter::~SeisCBVSPSWriter()
{
    close();
    delete &prevbid_;
}


void SeisCBVSPSWriter::close()
{
    delete tr_; tr_ = 0;
    prevbid_ = BinID( mUdf(int), mUdf(int) );
    nringather_ = 1;
}


void SeisCBVSPSWriter::usePar( const IOPar& iopar )
{
    const char* res = iopar.find( CBVSSeisTrcTranslator::sKeyDataStorage );
    if ( res && *res )
	reqdtype_ = (DataCharacteristics::UserType)(*res-'0');
}


bool SeisCBVSPSWriter::newInl( const SeisTrc& trc )
{
    if ( Values::isUdf(prevbid_.inl) )
    {
	if ( reqdtype_ == DataCharacteristics::Auto )
	    dc_ = trc.data().getInterpreter()->dataChar();
	else
	    dc_ = DataCharacteristics( reqdtype_ );
    }

    const BinID& trcbid = trc.info().binid;
    BufferString fnm = trcbid.inl; fnm += ext();
    FilePath fp( dirnm_ ); fp.add( fnm );
    fnm = fp.fullPath();

    if ( tr_ ) delete tr_;
    tr_ = CBVSSeisTrcTranslator::getInstance();
    if ( !tr_->initWrite(new StreamConn(fnm,Conn::Write),trc) )
    {
	errmsg_ = "Trying to start writing to '";
	errmsg_ += fnm; errmsg_ += "':\n";
	errmsg_ += tr_->errMsg();
	return false;
    }
    ObjectSet<SeisTrcTranslator::TargetComponentData>& ci= tr_->componentInfo();
    for ( int idx=0; idx<ci.size(); idx++ )
	ci[idx]->datachar = dc_;

    return true;
}


bool SeisCBVSPSWriter::put( const SeisTrc& trc )
{
    SeisTrcInfo& ti = const_cast<SeisTrcInfo&>( trc.info() );
    const BinID trcbid = ti.binid;
    if ( trcbid.inl != prevbid_.inl )
    {
	if ( !newInl(trc) )
	    return false;
    }
    if ( trcbid.crl != prevbid_.crl )
	nringather_ = 1;
    prevbid_ = trcbid;

    ti.binid = BinID( trcbid.crl, nringather_ );
    bool res = tr_->write( trc );
    ti.binid = trcbid;
    if ( !res )
	errmsg_ = tr_->errMsg();
    else
	nringather_++;

    return res;
}
