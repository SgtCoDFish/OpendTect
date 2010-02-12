/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : A.H. Bril
 * DATE     : 21-3-1996
 * FUNCTION : Seis trace translator
-*/

static const char* rcsID = "$Id: segytr.cc,v 1.94 2010-02-12 10:31:27 cvsbert Exp $";

#include "segytr.h"
#include "seistrc.h"
#include "seispacketinfo.h"
#include "segyhdr.h"
#include "segyfiledef.h"
#include "datainterp.h"
#include "conn.h"
#include "errh.h"
#include "iopar.h"
#include "iostrm.h"
#include "timefun.h"
#include "scaler.h"
#include "survinfo.h"
#include "seisselection.h"
#include "envvars.h"
#include "separstr.h"
#include "keystrs.h"
#include "settings.h"
#include "zdomain.h"
#include "strmprov.h"
#include "bendpoints2coords.h"
#include <math.h>
#include <ctype.h>

#define mBPS(cd) (int)cd->datachar.nrBytes()
#define mZStepFac \
    ( (!othdomain_ && SI().zIsTime()) || (othdomain_ && !SI().zIsTime()) \
      	? 1e-6 : 0.001)

static const int cSEGYWarnBadFmt = 1;
static const int cSEGYWarnPos = 2;
static const int cSEGYWarnZeroSampIntv = 3;
static const int cSEGYWarnDataReadIncomplete = 4;
static const int cSEGYWarnNonrectCoord = 5;
static const int cMaxNrSamples = 1000000;
static int maxnrconsecutivebadtrcs = -1;
static const char* sKeyMaxConsBadTrcs = "SEGY.Max Bad Trace Block";


SEGYSeisTrcTranslator::SEGYSeisTrcTranslator( const char* nm, const char* unm )
	: SeisTrcTranslator(nm,unm)
	, trchead_(*new SEGY::TrcHeader(headerbuf_,true,fileopts_.thdef_))
	, txthead_(0)
	, binhead_(*new SEGY::BinHeader)
	, trcscale_(0)
	, curtrcscale_(0)
	, forcerev0_(false)
	, storinterp_(0)
	, blockbuf_(0)
	, headerbufread_(false)
	, headerdone_(false)
	, userawdata_(false)
	, useinpsd_(false)
	, inpcd_(0)
	, outcd_(0)
	, estnrtrcs_(-1)
	, curoffs_(-1)
	, curcoord_(mUdf(float),0)
	, prevoffs_(0)
	, offsdef_(0,1)
	, othdomain_(false)
	, bp2c_(0)
{
    if ( maxnrconsecutivebadtrcs < 0 )
    {
	maxnrconsecutivebadtrcs = 50;
	Settings::common().get( sKeyMaxConsBadTrcs, maxnrconsecutivebadtrcs );
    }
    curtrcnr_ = prevtrcnr_ = -1;
    mSetUdf(curbid_.inl); mSetUdf(prevbid_.inl);
}


SEGYSeisTrcTranslator::~SEGYSeisTrcTranslator()
{
    cleanUp();
    delete txthead_;
    delete &binhead_;
    delete &trchead_;
}


void SEGYSeisTrcTranslator::cleanUp()
{
    SeisTrcTranslator::cleanUp();

    delete storinterp_; storinterp_ = 0;
    delete [] blockbuf_; blockbuf_ = 0;
    delete bp2c_; bp2c_ = 0;
    headerbufread_ = headerdone_ = false;

    prevoffs_ = curoffs_ = -1; mSetUdf(curcoord_.x);
}


int SEGYSeisTrcTranslator::dataBytes() const
{
    return SEGY::BinHeader::formatBytes(
	    	filepars_.fmt_ > 0 ? filepars_.fmt_ : 1 );
}


#define mErrRet(s) { fillErrMsg(s,false); return false; }
#define mPosErrRet(s) { fillErrMsg(s,true); return false; }


bool SEGYSeisTrcTranslator::readTapeHeader()
{
    std::streamoff so0 = 0;
    sConn().iStream().seekg( so0, std::ios::end );
    std::streampos endstrmpos = sConn().iStream().tellg();
    sConn().iStream().seekg( so0, std::ios::beg );

    if ( !txthead_ )
	txthead_ = new SEGY::TxtHeader;
    if ( !sConn().doIO(txthead_->txt_,SegyTxtHeaderLength) )
	mErrRet( "Cannot read SEG-Y Text header" )
    txthead_->setAscii();

    binhead_.needswap = filepars_.byteswap_ > 1;
    unsigned char binheaderbuf[400];
    if ( !sConn().doIO( binheaderbuf, SegyBinHeaderLength ) )
	mErrRet( "Cannot read SEG-Y Text header" )
    binhead_.getFrom( binheaderbuf );
    if ( forcerev0_ ) binhead_.isrev1 = false;

    trchead_.setNeedSwap( filepars_.byteswap_ > 1 );
    trchead_.isrev1 = binhead_.isrev1;
    if ( trchead_.isrev1 )
    {
	if ( binhead_.nrstzs > 100 ) // protect against wild values
	    binhead_.nrstzs = 0;
	for ( int idx=0; idx<binhead_.nrstzs; idx++ )
	{
	    if ( !sConn().doIO(txthead_->txt_,SegyTxtHeaderLength) )
		mErrRet( "No traces found in the SEG-Y file" )
	}
    }

    if ( filepars_.fmt_ == 0 )
    {
	filepars_.fmt_ = binhead_.format;
	if ( filepars_.fmt_ == 4 && read_mode != Seis::PreScan )
	    mErrRet( "SEG-Y format '4' (fixed point/gain code) not supported" )
	else if ( filepars_.fmt_<1 || filepars_.fmt_>8
		|| filepars_.fmt_==6 || filepars_.fmt_==7 )
	{
	    addWarn( cSEGYWarnBadFmt, getStringFromInt(filepars_.fmt_) );
	    filepars_.fmt_ = 1;
	}
    }

    txthead_->getText( pinfo.usrinfo );
    pinfo.nr = binhead_.lino;
    pinfo.zrg.step = binhead_.hdt * mZStepFac;
    insd.step = pinfo.zrg.step;
    innrsamples = binhead_.hns;

    estnrtrcs_ = (endstrmpos - (std::streampos)3600)
		/ (240 + dataBytes() * innrsamples);
    return true;
}


void SEGYSeisTrcTranslator::addWarn( int nr, const char* detail )
{
    static const bool nowarn = Settings::common().isTrue("SEG-Y.No warnings");
    if ( nowarn || warnnrs_.indexOf(nr) >= 0 ) return;

    BufferString msg;
    if ( nr == cSEGYWarnBadFmt )
    {
	msg = "SEG-Y format '"; msg += detail;
	msg += "' found.\n\tReplaced with '1' (4-byte floating point)";
	if ( atoi(detail) > 254 )
	    msg += "\n-> The file may not be SEG-Y, or byte-swapped";
    }
    else if ( nr == cSEGYWarnPos )
    {
	msg = "Bad position found. Such traces are ignored.\nFirst occurrence ";
	msg += detail;
    }
    else if ( nr == cSEGYWarnZeroSampIntv )
    {
	msg = "Zero sample interval found in trace header.\n"
	      "First occurrence ";
	msg += detail;
    }
    else if ( nr == cSEGYWarnDataReadIncomplete )
    {
	msg = detail;
    }
    else if ( nr == cSEGYWarnNonrectCoord )
    {
	msg = "Trace header indicates Geographic Coordinates (byte 89).\n"
	      "These are not supported.\n"
	      "Will bluntly load them as rectangular coordinates "
	      "(which they are most often)."
	      "\nBeware that the positions may therefore not be correct.\n"
	      "First occurrence ";
	msg += detail;
    }

    SeisTrcTranslator::addWarn( nr, msg );
}


void SEGYSeisTrcTranslator::updateCDFromBuf()
{
    SeisTrcInfo info; trchead_.fill( info, fileopts_.coordscale_ );
    if ( othdomain_ )
	info.sampling.step *= SI().zIsTime() ? 1000 : 0.001;

    insd.start = info.sampling.start;
    insd.step = pinfo.zrg.step;
    if ( mIsZero(insd.step,1e-8) )
    {
	insd.step = info.sampling.step;
	if ( mIsZero(insd.step,1e-8) )
	    insd.step = SI().zRange(false).step;
    }
    if ( !mIsUdf(fileopts_.timeshift_) )
	insd.start = fileopts_.timeshift_;
    if ( !mIsUdf(fileopts_.sampleintv_) )
	insd.step = fileopts_.sampleintv_;

    innrsamples = filepars_.ns_;
    if ( innrsamples <= 0 || innrsamples > cMaxNrSamples )
    {
	innrsamples = binhead_.hns;
	if ( innrsamples <= 0 || innrsamples > cMaxNrSamples )
	{
	    innrsamples = trchead_.nrSamples();
	    if ( innrsamples <= 0 || innrsamples > cMaxNrSamples )
		innrsamples = SI().zRange(false).nrSteps() + 1;
	}
    }

    addComp( getDataChar(filepars_.fmt_)  );
    DataCharacteristics& dc = tarcds[0]->datachar;
    dc.fmt = DataCharacteristics::Ieee;
    const float scfac = trchead_.postScale(filepars_.fmt_ ? filepars_.fmt_ : 1);
    if ( !mIsEqual(scfac,1,mDefEps)
      || (dc.isInteger() && dc.nrBytes() == BinDataDesc::N4) )
	dc = DataCharacteristics();
}


void SEGYSeisTrcTranslator::interpretBuf( SeisTrcInfo& ti )
{
    trchead_.fill( ti, fileopts_.coordscale_ );
    if ( othdomain_ )
	ti.sampling.step *= SI().zIsTime() ? 1000 : 0.001;
    if ( binhead_.mfeet == 2 ) ti.offset *= mFromFeetFactor;

    if ( is_prestack && fileopts_.psdef_ == SEGY::FileReadOpts::SrcRcvCoords )
    {
	Coord c1( trchead_.getCoord(true,fileopts_.coordscale_) );
	Coord c2( trchead_.getCoord(false,fileopts_.coordscale_) );
	ti.setPSFlds( c1, c2 );
	ti.coord = Coord( (c1.x+c2.x)*.5, (c1.y+c2.y)*.5 );
    }
    float scfac = trchead_.postScale( filepars_.fmt_ ? filepars_.fmt_ : 1 );
    if ( mIsEqual(scfac,1,mDefEps) )
	curtrcscale_ = 0;
    else
    {
	if ( !trcscale_ ) trcscale_ = new LinScaler( 0, scfac );
	else		 trcscale_->factor = scfac;
	curtrcscale_ =	trcscale_;
    }

    if ( !mIsUdf(fileopts_.timeshift_) )
	ti.sampling.start = fileopts_.timeshift_;
    if ( !mIsUdf(fileopts_.sampleintv_) )
	ti.sampling.step = fileopts_.sampleintv_;

    if ( fileopts_.coorddef_ == SEGY::FileReadOpts::Present )
	return;

    if ( fileopts_.coorddef_ == SEGY::FileReadOpts::Generate )
    {
	if ( mIsUdf(curcoord_.x) )
	    curcoord_ = fileopts_.startcoord_;
	else
	    curcoord_ += fileopts_.stepcoord_;
	ti.coord = curcoord_;
    }
    else
    {
	if ( !bp2c_ )
	{
	    StreamData sd = StreamProvider(fileopts_.coordfnm_).makeIStream();
	    if ( !sd.usable() )
		{ errmsg = "Cannot open coordinate file"; return; }
	    bp2c_ = new BendPoints2Coords( *sd.istrm );
	    sd.close();
	    if ( bp2c_->getIDs().size() < 2 )
		{ errmsg = "Cannot read coordinate file"; return; }
	}
	ti.coord = bp2c_->coordAt( ti.nr );
    }
}


void SEGYSeisTrcTranslator::setTxtHeader( SEGY::TxtHeader* th )
{
    delete txthead_; txthead_ = th;
}


bool SEGYSeisTrcTranslator::writeTapeHeader()
{
    if ( filepars_.fmt_ == 0 ) // Auto-detect
	filepars_.fmt_ = nrFormatFor( storinterp_->dataChar() );

    trchead_.isrev1 = !forcerev0_;

    if ( !txthead_ )
    {
	txthead_ = new SEGY::TxtHeader( trchead_.isrev1 );
	txthead_->setUserInfo( pinfo.usrinfo );
	fileopts_.thdef_.linename = curlinekey.buf();
	fileopts_.thdef_.pinfo = &pinfo;
	txthead_->setPosInfo( fileopts_.thdef_ );
	txthead_->setStartPos( outsd.start );
	if ( Settings::common().isTrue("SEGY.Text Header EBCDIC") )
	    txthead_->setEbcdic();
    }
    if ( !sConn().doIO( txthead_->txt_, SegyTxtHeaderLength ) )
	mErrRet("Cannot write SEG-Y textual header")

    SEGY::BinHeader binhead( trchead_.isrev1 );
    binhead.format = filepars_.fmt_ < 2 ? 1 : filepars_.fmt_;
    filepars_.fmt_ = binhead.format;
    binhead.lino = pinfo.nr;
    static int jobid = 0;
    binhead.jobid = ++jobid;
    binhead.hns = (short)outnrsamples;
    binhead.hdt = (short)(outsd.step / mZStepFac + .5);
    binhead.tsort = is_prestack ? 0 : 4; // To make Strata users happy
    binhead.mfeet = SI().xyInFeet() ? 2 : 1;
    unsigned char binheadbuf[400];
    binhead.putTo( binheadbuf );
    if ( !sConn().doIO( binheadbuf, SegyBinHeaderLength ) )
	mErrRet("Cannot write SEG-Y binary header")

    return true;
}


void SEGYSeisTrcTranslator::fillHeaderBuf( const SeisTrc& trc )
{
    if ( SI().xyInFeet() )
    {
	SeisTrcInfo info = trc.info();
	info.offset *= mToFeetFactor;
	trchead_.use( info );
    }
    else
    {
	trchead_.use( trc.info() );
    }

    SamplingData<float> sdtoput( useinpsd_ ? trc.info().sampling : outsd );
    const int nstoput = useinpsd_ ? trc.size() : outnrsamples;
    if ( othdomain_ )
	sdtoput.step *= SI().zIsTime() ? 0.001 : 1000;

    trchead_.putSampling( sdtoput, nstoput );
}


void SEGYSeisTrcTranslator::usePar( const IOPar& iopar )
{
    SeisTrcTranslator::usePar( iopar );

    filepars_.usePar( iopar );
    fileopts_.usePar( iopar );
    fileopts_.setGeomType( Seis::geomTypeOf(is_2d,is_prestack) );

    iopar.getYN( SEGY::FileDef::sKeyForceRev0(), forcerev0_ );
    othdomain_ = !ZDomain::isSIDomain( iopar );
}


bool SEGYSeisTrcTranslator::isRev1() const
{
    return trchead_.isrev1;
}


void SEGYSeisTrcTranslator::toSupported( DataCharacteristics& dc ) const
{
    if ( dc.isInteger() || !dc.isIeee() )
	dc = getDataChar( nrFormatFor(dc) );
}


void SEGYSeisTrcTranslator::toPreSelected( DataCharacteristics& dc ) const
{
    if ( filepars_.fmt_ > 0 ) dc = getDataChar( filepars_.fmt_ );
}


int SEGYSeisTrcTranslator::nrFormatFor( const DataCharacteristics& dc ) const
{
    int nrbytes = dc.nrBytes();
    if ( nrbytes > 4 ) nrbytes = 4;
    else if ( !dc.isSigned() && nrbytes < 4 )
	nrbytes *= 2;

    int nf = 1;
    if ( nrbytes == 1 )
	nf = 8;
    else if ( nrbytes == 2 )
	nf = 3;
    else
	nf = dc.isInteger() ? 2 : 1;

    return nf;
}


DataCharacteristics SEGYSeisTrcTranslator::getDataChar( int nf ) const
{
    DataCharacteristics dc( true, true, BinDataDesc::N4,
			DataCharacteristics::Ibm,
			filepars_.byteswap_ ? !__islittle__ : __islittle__ );

    switch ( nf )
    {
    case 3:
        dc.setNrBytes( 2 );
    case 2:
    break;
    case 8:
        dc.setNrBytes( 1 );
    break;
    case 5:
	dc.fmt = DataCharacteristics::Ieee;
	dc.setInteger( false );
	dc.littleendian = false;
    break;
    default:
	dc.setInteger( false );
    break;
    }

    return dc;
}


bool SEGYSeisTrcTranslator::commitSelections_()
{
    const bool forread = conn->forRead();
    fileopts_.forread_ = forread;
    fileopts_.setGeomType( Seis::geomTypeOf( is_2d, is_prestack ) );

    inpcd_ = inpcds[0]; outcd_ = outcds[0];
    storinterp_ = new TraceDataInterpreter( forread ? inpcd_->datachar
	    					   : outcd_->datachar );
    if ( mIsEqual(outsd.start,insd.start,mDefEps)
      && mIsEqual(outsd.step,insd.step,mDefEps) )
	useinpsd_ = true;
    userawdata_ = inpcd_->datachar == outcd_->datachar;

    if ( blockbuf_ )
	{ delete [] blockbuf_; blockbuf_ = 0; }
    int bufsz = innrsamples;
    if ( outnrsamples > bufsz ) bufsz = outnrsamples;
    bufsz += 10;
    int nbts = inpcd_->datachar.nrBytes();
    if ( outcd_->datachar.nrBytes() > nbts ) nbts = outcd_->datachar.nrBytes();
    
    blockbuf_ = new unsigned char [ nbts * bufsz ];
    return forread || writeTapeHeader();
}


bool SEGYSeisTrcTranslator::initRead_()
{
    if ( !readTapeHeader() || !readTraceHeadBuffer() )
	return false;

    trchead_.initRead();

    if ( tarcds.isEmpty() )
	updateCDFromBuf();

    if ( innrsamples <= 0 || innrsamples > cMaxNrSamples )
	mErrRet(BufferString("Cannot find a reasonable number of samples."
		    	     "\nFound: ",innrsamples,
			     ".\nPlease 'Overrule' to set something usable"))
    return true;
}


bool SEGYSeisTrcTranslator::initWrite_( const SeisTrc& trc )
{
    for ( int idx=0; idx<trc.data().nrComponents(); idx++ )
    {
	DataCharacteristics dc(trc.data().getInterpreter(idx)->dataChar());
	addComp( dc );
	toSupported( dc );
	toPreSelected( dc );
	tarcds[idx]->datachar = dc;
	if ( idx ) tarcds[idx]->destidx = -1;
    }

    return true;
}


bool SEGYSeisTrcTranslator::goToTrace( int nr )
{
    std::streamoff so = nr;
    so *= (240 + dataBytes() * innrsamples);
    so += 3600;
    sConn().iStream().clear();
    sConn().iStream().seekg( so, std::ios::beg );
    headerbufread_ = headerdone_ = false;
    return sConn().iStream().good();
}



const char* SEGYSeisTrcTranslator::getTrcPosStr() const
{
    static BufferString ret;
    int usecur = 1; const bool is2d = Seis::is2D(fileopts_.geomType());
    if ( is2d )
    {
	if ( curtrcnr_ < 0 )
	    usecur = prevtrcnr_ < 0 ? -1 : 0;
    }
    else
    {
	if ( mIsUdf(curbid_.inl) )
	    usecur = mIsUdf(prevbid_.inl) ? -1 : 0;
    }

    ret = usecur ? "at " : "after ";
    if ( usecur < 0 )
	{ ret += "start of data"; return ret.buf(); }

    if ( is2d )
	{ ret += "trace number "; ret += usecur ? curtrcnr_ : prevtrcnr_; }
    else
    {
	const BinID bid( usecur ? curbid_ : prevbid_ );
	ret += "position "; ret += bid.inl; ret += "/"; ret += bid.crl;
    }

    if ( Seis::isPS(fileopts_.geomType()) )
	{ ret += " (offset "; ret += usecur ? curoffs_ : prevoffs_; ret += ")";}

    return ret.buf();
}


bool SEGYSeisTrcTranslator::tryInterpretBuf( SeisTrcInfo& ti )
{
    errmsg = 0;
    interpretBuf( ti );
    return !errmsg;
}


bool SEGYSeisTrcTranslator::skipThisTrace( SeisTrcInfo& ti, int& nrbadtrcs )
{
    addWarn( cSEGYWarnPos, getTrcPosStr() );
    nrbadtrcs++;
    if ( nrbadtrcs >= maxnrconsecutivebadtrcs )
    {
	const BufferString str( "More than ", maxnrconsecutivebadtrcs,
			        " traces with invalid position found." );
	mPosErrRet(str);
    }
    sConn().iStream().seekg( innrsamples * mBPS(inpcd_), std::ios::cur );
    if ( !readTraceHeadBuffer() )
	return false;
    if ( !tryInterpretBuf(ti) )
	return false;
    return true;
}


#define mBadCoord(ti) \
	(ti.coord.x < 0.01 && ti.coord.y < 0.01)
#define mBadBid(ti) \
	(ti.binid.inl <= 0 && ti.binid.crl <= 0)
#define mSkipThisTrace() { if ( !skipThisTrace(ti,nrbadtrcs) ) return false; }


bool SEGYSeisTrcTranslator::readInfo( SeisTrcInfo& ti )
{
    if ( !storinterp_ ) commitSelections();
    if ( headerdone_ ) return true;

    if ( read_mode != Seis::Scan )
	{ mSetUdf(curbid_.inl); mSetUdf(curtrcnr_); }

    if ( !headerbufread_ && !readTraceHeadBuffer() )
	return false;
    if ( !tryInterpretBuf(ti) )
	return false;

    bool goodpos = true;
    int nrbadtrcs = 0;
    if ( fileopts_.icdef_ == SEGY::FileReadOpts::XYOnly )
    {
	if ( read_mode == Seis::Scan )
	    goodpos = !mBadCoord(ti);
	else if ( read_mode == Seis::Prod )
	{
	    while ( mBadCoord(ti) )
		mSkipThisTrace()
	}
	ti.binid = SI().transform( ti.coord );
    }
    else if ( fileopts_.icdef_ == SEGY::FileReadOpts::ICOnly )
    {
	if ( read_mode == Seis::Scan )
	    goodpos = !mBadBid(ti);
	else if ( read_mode == Seis::Prod )
	{
	    while ( mBadBid(ti) )
		mSkipThisTrace()
	}
	ti.coord = SI().transform( ti.binid );
    }
    else
    {
	if ( read_mode == Seis::Scan )
	    goodpos = !mBadBid(ti) || !mBadCoord(ti);
	if ( read_mode == Seis::Prod )
	{
	    while ( mBadBid(ti) && mBadCoord(ti) )
		mSkipThisTrace()
	    if ( mBadBid(ti) )
		ti.binid = SI().transform( ti.coord );
	    else if ( mBadCoord(ti) )
		ti.coord = SI().transform( ti.binid );
	}
    }

    if ( trchead_.isusable )
	trchead_.isusable = goodpos;

    if ( !useinpsd_ ) ti.sampling = outsd;

    if ( fileopts_.psdef_ == SEGY::FileReadOpts::UsrDef )
    {
	const bool is2d = Seis::is2D(fileopts_.geomType());
	if ( (is2d && ti.nr != prevtrcnr_) || (!is2d && ti.binid != prevbid_) )
	    curoffs_ = -1;

	if ( curoffs_ < 0 )
	    curoffs_ = offsdef_.start;
	else
	    curoffs_ += offsdef_.step;

	ti.offset = curoffs_;
    }

    if ( goodpos )
    {
	prevtrcnr_ = curtrcnr_; curtrcnr_ = ti.nr;
	prevbid_ = curbid_; curbid_ = ti.binid;
	prevoffs_ = curoffs_; curoffs_ = ti.offset;
    }

    if ( mIsZero(ti.sampling.step,mDefEps) )
    {
	addWarn(cSEGYWarnZeroSampIntv,getTrcPosStr());
	ti.sampling.step = insd.step;
    }

    if ( trchead_.nonrectcoords )
	addWarn(cSEGYWarnNonrectCoord,getTrcPosStr());

    return (headerdone_ = true);
}


bool SEGYSeisTrcTranslator::read( SeisTrc& trc )
{
    if ( !readInfo(trc.info()) )
	return false;

    return readData( trc );
}


bool SEGYSeisTrcTranslator::skip( int ntrcs )
{
    if ( ntrcs < 1 ) return true;
    if ( !storinterp_ ) commitSelections();

    std::istream& strm = sConn().iStream();
    if ( !headerdone_ )
	strm.seekg( mSEGYTraceHeaderBytes, std::ios::cur );
    strm.seekg( innrsamples * mBPS(inpcd_), std::ios::cur );
    if ( ntrcs > 1 )
	strm.seekg( (ntrcs-1) * (mSEGYTraceHeaderBytes
		    		+ innrsamples * mBPS(inpcd_)) );

    headerbufread_ = headerdone_ = false;

    if ( !strm.good() )
	mPosErrRet("Read error during trace skipping")
    return true;
}


bool SEGYSeisTrcTranslator::writeTrc_( const SeisTrc& trc )
{
    memset( headerbuf_, 0, mSEGYTraceHeaderBytes );
    fillHeaderBuf( trc );

    if ( !sConn().doIO( headerbuf_, mSEGYTraceHeaderBytes ) )
	mErrRet("Cannot write trace header")

    return writeData( trc );
}


bool SEGYSeisTrcTranslator::readTraceHeadBuffer()
{
    if ( !sConn().doIO( headerbuf_, mSEGYTraceHeaderBytes )
	    || sConn().iStream().eof() )
    {
	if ( !sConn().iStream().eof() )
	    mPosErrRet("Error reading trace header")
	return noErrMsg();
    }

    return (headerbufread_ = true);
}


bool SEGYSeisTrcTranslator::readDataToBuf( unsigned char* tdptr )
{
    std::istream& strm = sConn().iStream();
    if ( samps.start > 0 )
	strm.seekg( samps.start * mBPS(inpcd_), std::ios::cur );

    unsigned char* ptr = userawdata_ ? tdptr : blockbuf_;
    int rdsz = (samps.width()+1) *  mBPS(inpcd_);
    if ( !sConn().doIO(ptr,rdsz) )
    {
	if ( strm.gcount() != rdsz )
	    addWarn( cSEGYWarnDataReadIncomplete, strm.gcount()
		    ? "Last trace is incomplete" : "No data in last trace" );
	return noErrMsg();
    }

    if ( samps.stop < innrsamples-1 )
	strm.seekg( (innrsamples-samps.stop-1) * mBPS(inpcd_), std::ios::cur );

    return strm.good();
}


bool SEGYSeisTrcTranslator::readData( SeisTrc& trc )
{
    noErrMsg();
    const int curcomp = selComp();
    prepareComponents( trc, outnrsamples );
    headerbufread_ = headerdone_ = false;

    unsigned char* tdptr = trc.data().getComponent(curcomp)->data();
    if ( !readDataToBuf(tdptr) ) return false;

    if ( !userawdata_ )
    {
	const unsigned char* ptr = blockbuf_;
	// Convert data into other format
	for ( int isamp=0; isamp<outnrsamples; isamp++ )
	    trc.set( isamp, storinterp_->get(ptr,isamp), curcomp );
    }

    if ( curtrcscale_ )
    {
	const int tsz = trc.size();
	for ( int idx=0; idx<tsz; idx++ )
	    trc.set(idx,curtrcscale_->scale(trc.get(idx,curcomp)),curcomp);
    }

    return true;
}


bool SEGYSeisTrcTranslator::writeData( const SeisTrc& trc )
{
    const int curcomp = selComp();

    static bool allowudfs = GetEnvVarYN( "OD_SEIS_SEGY_ALLOW_UDF" );
    static float udfreplaceval = GetEnvVarDVal( "OD_SEIS_SEGY_UDF_REPLACE", 0 );
    for ( int idx=0; idx<outnrsamples; idx++ )
    {
	float val = trc.getValue( outsd.atIndex(idx), curcomp );
	if ( !allowudfs && mIsUdf(val) )
	    val = udfreplaceval;
	storinterp_->put( blockbuf_, idx, val );
    }

    if ( !sConn().doIO( (void*)blockbuf_,
			 outnrsamples * outcd_->datachar.nrBytes() ) )
	mErrRet("Cannot write trace data")

    headerdone_ = headerbufread_ = false;
    return true;
}


void SEGYSeisTrcTranslator::fillErrMsg( const char* s, bool withpos )
{
    static BufferString msg;
    msg = "";

    const char* fnm = sConn().streamData().fileName();
    if ( !fnm || !*fnm )
	msg = usrname_;
    else
	{ msg = "In file '"; msg += fnm; msg += "'"; }
    if ( withpos )
	{ msg += " "; msg += getTrcPosStr(); }
    msg += ":\n"; msg += s;
    errmsg = msg.buf();
}


bool SEGYSeisTrcTranslator::noErrMsg()
{
    errmsg = ""; return false;
}
