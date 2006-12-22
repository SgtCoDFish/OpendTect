/*+
 * COPYRIGHT: (C) dGB Beheer B.V.
 * AUTHOR   : A.H. Bril
 * DATE     : Nov 2006
-*/

static const char* rcsID = "$Id: tableascio.cc,v 1.7 2006-12-22 10:53:26 cvsbert Exp $";

#include "tableascio.h"
#include "tabledef.h"
#include "tableconvimpl.h"
#include "unitofmeasure.h"
#include <iostream>

namespace Table
{

struct SpecID
{
    		SpecID( int formnr, int snr )
		    : formnr_(formnr), elemnr_(snr)		{}
    int		formnr_;
    int		elemnr_;
    bool	operator ==( const SpecID& sid ) const
		{ return formnr_==sid.formnr_ && elemnr_==sid.elemnr_; }
};


class AscIOImp_ExportHandler : public ExportHandler
{

public:

AscIOImp_ExportHandler( const AscIO& aio, bool hdr )
    : ExportHandler(std::cerr)
    , aio_(const_cast<AscIO&>(aio))
    , hdr_(hdr)
    , ready_(false)
    , rownr_(0)
{
}

const char* putRow( const BufferStringSet& bss )
{
    return hdr_ ? putHdrRow( bss ) : putBodyRow( bss );
}

const char* putHdrRow( const BufferStringSet& bss )
{
    if ( aio_.fd_.needToken() )
    {
	ready_ = bss.size() >= aio_.fd_.tokencol_
	      && bss.get( aio_.fd_.tokencol_ ) == aio_.fd_.token_;
	if ( ready_ )
	    return finishHdr();
    }

    for ( int itar=0; itar<aio_.fd_.headerinfos_.size(); itar++ )
    {
	const Table::TargetInfo& tarinf = *aio_.fd_.headerinfos_[itar];
	SpecID sid( tarinf.selection_.form_, 0 );
	const Table::TargetInfo::Form& selform = tarinf.form( sid.formnr_ );
	for ( ; sid.elemnr_<selform.elems_.size(); sid.elemnr_++ )
	{
	    if ( !tarinf.selection_.havePos(sid.elemnr_) )
		continue;

	    const RowCol& rc( tarinf.selection_.elems_[sid.elemnr_].pos_ );
	    if ( rc.r() == rownr_ )
	    {
		if ( rc.c() >= bss.size() )
		    return mkErrMsg( tarinf, sid, rc,
				     "Data not present in header" );
		else
		{
		    formvals_.add( bss.get(rc.c()) );
		    formids_ += sid;
		}
	    }
	}
    }

    rownr_++;
    ready_ = ready_ || rownr_ >= aio_.fd_.nrHdrLines();
    return ready_ ? finishHdr() : 0;
}


const char* finishHdr()
{
    for ( int itar=0; itar<aio_.fd_.headerinfos_.size(); itar++ )
    {
	const Table::TargetInfo& tarinf = *aio_.fd_.headerinfos_[itar];
	SpecID sid( tarinf.selection_.form_, 0 );
	const Table::TargetInfo::Form& selform = tarinf.form( sid.formnr_ );
	for ( ; sid.elemnr_<selform.elems_.size(); sid.elemnr_++ )
	{
	    if ( !tarinf.selection_.havePos(sid.elemnr_) )
		aio_.addVal( tarinf.selection_.getVal(sid.elemnr_),
		             tarinf.selection_.getUnit(sid.elemnr_) );
	    else
	    {
		bool found = false;
		for ( int idx=0; idx<formids_.size(); idx++ )
		{
		    if ( formids_[idx] == sid )
		    {
			aio_.addVal( formvals_.get(idx),
			             tarinf.selection_.getUnit(sid.elemnr_) );
			found = true; break;
		    }
		}
		if ( !found && !tarinf.isOptional() )
		    return mkErrMsg( tarinf, sid, RowCol(-1,-1),
			    	     "Required field not found" );
	    }
	}
    }
    return 0;
}


const char* mkErrMsg( const TargetInfo& tarinf, SpecID sid,
		      const RowCol& rc, const char* msg )
{
    errmsg_ = msg; errmsg_ += ":\n";
    errmsg_ += tarinf.name(); errmsg_ += " [";
    errmsg_ += tarinf.form(sid.formnr_).name();
    if ( tarinf.nrForms() > 0 )
	{ errmsg_ += " (field "; sid.elemnr_; errmsg_ += ")"; }
    errmsg_ += "]";
    if ( rc.c() >= 0 )
    {
	errmsg_ += "\nwas specified at ";
	if ( rc.r() < 0 )
	    errmsg_ += "column ";
	else
	    { errmsg_ += "row/col "; errmsg_ += rc.r()+1; errmsg_ += "/"; }
	errmsg_ += rc.c()+1;
    }
    return errmsg_;
}


const char* putBodyRow( const BufferStringSet& bss )
{
    aio_.emptyVals();

    for ( int itar=0; itar<aio_.fd_.bodyinfos_.size(); itar++ )
    {
	const Table::TargetInfo& tarinf = *aio_.fd_.bodyinfos_[itar];
	SpecID sid( tarinf.selection_.form_, 0 );
	const Table::TargetInfo::Form& selform = tarinf.form( sid.formnr_ );
	for ( ; sid.elemnr_<selform.elems_.size(); sid.elemnr_++ )
	{
	    if ( !tarinf.selection_.havePos(sid.elemnr_) )
		aio_.addVal( tarinf.selection_.getVal(sid.elemnr_),
		             tarinf.selection_.getUnit(sid.elemnr_) );
	    else
	    {
		const RowCol& rc( tarinf.selection_.elems_[sid.elemnr_].pos_ );
		if ( rc.c() < bss.size() )
		    aio_.addVal( bss.get(rc.c()),
			    	 tarinf.selection_.getUnit(sid.elemnr_) );
		else
		    return mkErrMsg( tarinf, sid, RowCol(rc.c(),-1),
			    	     "Column missing in file" );
	    }
	}
    }
    return 0;
}

    AscIO&		aio_;
    BufferString	errmsg_;
    const bool		hdr_;
    bool		ready_;
    int			rownr_;
    TypeSet<SpecID>	formids_;
    BufferStringSet	formvals_;

};

}; // namespace Table

Table::AscIO::~AscIO()
{
    delete imphndlr_;
    delete exphndlr_;
    delete cnvrtr_;
}


void Table::AscIO::emptyVals() const
{
    Table::AscIO& aio = *const_cast<AscIO*>(this);
    aio.vals_.erase();
    aio.units_.erase();
}


void Table::AscIO::addVal( const char* s, const UnitOfMeasure* mu ) const
{
    Table::AscIO& aio = *const_cast<AscIO*>(this);
    aio.vals_.add( s );
    aio.units_ += mu;
}


#define mErrRet(s) { errmsg_ = s; return false; }

bool Table::AscIO::getHdrVals( std::istream& strm ) const
{
    const int nrhdrlines = fd_.nrHdrLines();
    if ( nrhdrlines < 1 )
    {
	for ( int itar=0; itar<fd_.headerinfos_.size(); itar++ )
	{
	    const Table::TargetInfo& tarinf = *fd_.headerinfos_[itar];
	    const Table::TargetInfo::Form& selform
				= tarinf.form( tarinf.selection_.form_ );
	    for ( int ielem=0; ielem<selform.elems_.size(); ielem++ )
		addVal( tarinf.selection_.getVal(ielem),
		        tarinf.selection_.getUnit(ielem) );
	}
    }
    else
    {
	Table::WSImportHandler hdrimphndlr( strm );
	Table::AscIOImp_ExportHandler hdrexphndlr( *this, true );
	Table::Converter hdrcnvrtr( hdrimphndlr, hdrexphndlr );
	for ( int idx=0; idx<nrhdrlines; idx++ )
	{
	    int res = hdrcnvrtr.nextStep();
	    if ( res < 0 )
		mErrRet( hdrcnvrtr.message() )
	    else if ( res == 0 || hdrexphndlr.ready_ )
		break;
	}
	if ( !hdrexphndlr.ready_ || !strm.good() )
	    mErrRet( "File header does not comply with format description" )
    }

    if ( !strm.good() || strm.eof() )
	mErrRet( "End of file reached before end of header" )

    return true;
}


int Table::AscIO::getNextBodyVals( std::istream& strm ) const
{
    if ( !cnvrtr_ )
    {
	AscIO& self = *const_cast<AscIO*>(this);
	self.imphndlr_ = new Table::WSImportHandler( strm );
	self.exphndlr_ = new Table::AscIOImp_ExportHandler( *this, false );
	self.cnvrtr_ = new Table::Converter( *imphndlr_, *exphndlr_ );
    }

    int ret = cnvrtr_->nextStep();
    if ( ret < 0 )
	errmsg_ = cnvrtr_->message();
    return ret;
}


bool Table::AscIO::putHdrVals( std::ostream& strm ) const
{
    errmsg_ = "TODO: Table::AscIO::putHdrVals not implemented";
    return false;
}


bool Table::AscIO::putNextBodyVals( std::ostream& strm ) const
{
    errmsg_ = "TODO: Table::AscIO::putNextBodyVals not implemented";
    return false;
}


const char* Table::AscIO::text( int ifld ) const
{
    return ifld >= vals_.size() ? "" : ((const char*)vals_.get(ifld));
}


int Table::AscIO::getIntValue( int ifld ) const
{
    if ( ifld >= vals_.size() )
	return mUdf(int);
    const char* val = vals_.get( ifld );
    return *val ? atoi( val ) : mUdf(int);
}


float Table::AscIO::getfValue( int ifld ) const
{
    if ( ifld >= vals_.size() )
	return mUdf(int);
    const char* sval = vals_.get( ifld );
    if ( !*sval ) return mUdf(float);
    float val = atof( sval );
    if ( mIsUdf(val) ) return val;

    const UnitOfMeasure* unit = units_.size() > ifld ? units_[ifld] : 0;
    return unit ? unit->internalValue( val ) : val;
}


double Table::AscIO::getdValue( int ifld ) const
{
    if ( ifld >= vals_.size() )
	return mUdf(int);
    const char* sval = vals_.get( ifld );
    if ( !*sval ) return mUdf(double);
    double val = atof( sval );
    if ( mIsUdf(val) ) return val;

    const UnitOfMeasure* unit = units_.size() > ifld ? units_[ifld] : 0;
    return unit ? unit->internalValue( val ) : val;
}
