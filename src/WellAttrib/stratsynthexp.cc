/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Satyaki Maitra
 Date:          July 2013
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";


#include "ctxtioobj.h"
#include "ioobj.h"
#include "ioman.h"
#include "prestackgather.h"
#include "seistrc.h"
#include "seisbufadapters.h"
#include "seiswrite.h"
#include "seisselectionimpl.h"
#include "seispsioprov.h"
#include "stratsynthexp.h"
#include "syntheticdataimpl.h"
#include "posinfo2d.h"
#include "survinfo.h"
#include "transl.h"

StratSynthExporter::StratSynthExporter( const IOObj& outobj,
	const ObjectSet<const SyntheticData>& sds,
	const PosInfo::Line2DData& geom, const SeparString& prepostfix )
    : Executor( "Exporting syntheic data" )
    , sds_(sds)
    , linegeom_(geom)
    , outobj_(outobj)
    , cursdidx_(0)
    , posdone_(0)
    , prepostfixstr_(prepostfix)
    , writer_(0)
{
}


StratSynthExporter::~StratSynthExporter()
{
    delete writer_;
}




od_int64 StratSynthExporter::nrDone() const
{
    return (cursdidx_*linegeom_.positions().size()) + posdone_;
}


od_int64 StratSynthExporter::totalNr() const
{
    return sds_.size()*linegeom_.positions().size();
}

void StratSynthExporter::prepareWriter()
{
    const bool isps = sds_[cursdidx_]->isPS();
    BufferString synthnm( prepostfixstr_[0].str(), "_",sds_[cursdidx_]->name());
    synthnm += "_"; synthnm += prepostfixstr_[1].str();
    PtrMan<CtxtIOObj> psctxt;
    if ( isps )
    {
	psctxt = mMkCtxtIOObj( SeisPS2D );
	psctxt->setName( synthnm.buf() );
	psctxt->ctxt.deftransl = CBVSSeisPS2DTranslator::translKey();
	IOM().getEntry( *psctxt, false );
    }

    delete writer_;
    writer_ = new SeisTrcWriter( isps ? psctxt->ioobj : &outobj_ );
    Seis::SelData* seldata = Seis::SelData::get( Seis::Range );
    seldata->lineKey() = LineKey( linegeom_.lineName(), synthnm );
    writer_->setSelData( seldata );
    writer_->setAttrib( synthnm );
}


int StratSynthExporter::nextStep()
{
    if ( !sds_.validIdx(cursdidx_) )
	return Executor::Finished();

    const bool isps = sds_[cursdidx_]->isPS();
    if ( !posdone_ )
	prepareWriter();

    return !isps ? writePostStackTrace() : writePreStackTraces();
}


int StratSynthExporter::writePostStackTrace()
{
    const SyntheticData* sd = sds_[cursdidx_];
    mDynamicCastGet(const PostStackSyntheticData*,postsd,sd);
    if ( !postsd )
	return Executor::ErrorOccurred();
    const SeisTrcBuf& seisbuf = postsd->postStackPack().trcBuf();
    const TypeSet<PosInfo::Line2DPos>& positions = linegeom_.positions();
    if ( !positions.validIdx(posdone_) )
    {
	cursdidx_++;
	posdone_ = 0;
	return Executor::MoreToDo();
    }

    const PosInfo::Line2DPos& linepos = positions[posdone_];
    const SeisTrc* synthrc = seisbuf.get( posdone_ );
    if ( !synthrc )
	return Executor::ErrorOccurred();
    SeisTrc trc( *synthrc );
    trc.info().nr = linepos.nr_;
    trc.info().binid = SI().transform( linepos.coord_ );
    trc.info().coord = linepos.coord_;
    writer_->put( trc );
    posdone_++;
    return Executor::MoreToDo();
}


int StratSynthExporter::writePreStackTraces()
{
    const SyntheticData* sd = sds_[cursdidx_];
    mDynamicCastGet(const PreStackSyntheticData*,presd,sd);
    if ( !presd )
	return Executor::ErrorOccurred();
    const PreStack::GatherSetDataPack& gsdp = presd->preStackPack();
    const ObjectSet<PreStack::Gather>& gathers = gsdp.getGathers();
    const TypeSet<PosInfo::Line2DPos>& positions = linegeom_.positions();
    if ( !positions.validIdx(posdone_) )
    {
	cursdidx_++;
	posdone_ = 0;
	return Executor::MoreToDo();
    }

    const PosInfo::Line2DPos& linepos = positions[posdone_];

    if ( !gathers.validIdx(posdone_) )
	return Executor::ErrorOccurred();
    const PreStack::Gather* gather = gathers[posdone_];
    for ( int offsidx=0; offsidx<gather->size(true); offsidx++ )
    {
	const float offset = gather->getOffset( offsidx );
	SeisTrc trc( *gsdp.getTrace(posdone_,offsidx) );
	trc.info().nr = linepos.nr_;
	trc.info().binid = SI().transform( linepos.coord_ );
	trc.info().coord = linepos.coord_;
	trc.info().offset = offset;
	writer_->put( trc );
    }

    posdone_++;
    return Executor::MoreToDo();
}

