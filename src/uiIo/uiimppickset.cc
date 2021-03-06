/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Nanne Hemstra
 Date:          June 2002
________________________________________________________________________

-*/

#include "uiimppickset.h"
#include "uibutton.h"
#include "uicolor.h"
#include "uicombobox.h"
#include "uifileinput.h"
#include "uiioobjsel.h"
#include "uimsg.h"
#include "uipickpartserv.h"
#include "uiseparator.h"
#include "uistrings.h"
#include "uitblimpexpdatasel.h"

#include "ctxtioobj.h"
#include "file.h"
#include "filepath.h"
#include "ioman.h"
#include "ioobj.h"
#include "picksetmgr.h"
#include "picksettr.h"
#include "randcolor.h"
#include "od_istream.h"
#include "strmprov.h"
#include "surfaceinfo.h"
#include "survinfo.h"
#include "tabledef.h"
#include "od_helpids.h"

#include <math.h>

static const char* zoptions[] =
{
    "Input file",
    "Constant Z",
    "Horizon",
    0
};


uiImpExpPickSet::uiImpExpPickSet(uiParent* p, uiPickPartServer* pps, bool imp )
    : uiDialog(p,uiDialog::Setup(imp
				     ? uiStrings::phrImport(sPicksetPolygon())
				     : uiStrings::phrExport(sPicksetPolygon()),
				 mNoDlgTitle,
				 imp
					? mODHelpKey(mImpPickSetHelpID)
					: mODHelpKey(mExpPickSetHelpID) )
				.modal(false))
    , serv_(pps)
    , import_(imp)
    , fd_(*PickSetAscIO::getDesc(true))
    , zfld_(0)
    , constzfld_(0)
    , dataselfld_(0)
    , importReady(this)
    , storedid_(MultiID::udf())
{
    setOkCancelText( import_ ? uiStrings::sImport() : uiStrings::sExport(),
		     uiStrings::sClose() );
    if ( import_ ) enableSaveButton( tr("Display after import") );

    const uiString tp = uiStrings::phrASCII( uiStrings::sFile() );
    uiString label = import_
	? uiStrings::sInputASCIIFile()
	: uiStrings::sOutputASCIIFile();

    filefld_ = new uiFileInput( this, label, uiFileInput::Setup()
					    .withexamine(import_)
					    .forread(import_) );
    if ( import_ )
	filefld_->valuechanged.notify( mCB(this,uiImpExpPickSet,inputChgd) );

    IOObjContext ctxt( mIOObjContext(PickSet) );
    ctxt.forread_ = !import_;
    label = import_
	? uiStrings::phrOutput( sPicksetPolygon() )
	: uiStrings::phrInput( sPicksetPolygon() );

    objfld_ = new uiIOObjSel( this, ctxt, label );

    if ( import_ )
    {
	zfld_ = new uiLabeledComboBox( this, tr("Get Z values from") );
	zfld_->box()->addItems( zoptions );
	zfld_->box()->selectionChanged.notify( mCB(this,uiImpExpPickSet,
				formatSel) );
	zfld_->attach( alignedBelow, filefld_ );

	uiString constzlbl = tr("Specify constant Z value %1")
				.arg( SI().getUiZUnitString() );
	constzfld_ = new uiGenInput( this, constzlbl, FloatInpSpec(0) );
	constzfld_->attach( alignedBelow, zfld_ );
	constzfld_->display( zfld_->box()->currentItem() == 1 );

	horinpfld_ = new uiLabeledComboBox( this,
			    uiStrings::phrSelect( uiStrings::sHorizon() ) );
	serv_->fetchHors( false );
	const ObjectSet<SurfaceInfo> hinfos = serv_->horInfos();
	for ( int idx=0; idx<hinfos.size(); idx++ )
	    horinpfld_->box()->addItem( toUiString(hinfos[idx]->name) );
	horinpfld_->attach( alignedBelow, zfld_ );
	horinpfld_->display( zfld_->box()->currentItem() == 2 );

	uiSeparator* sep = new uiSeparator( this, "H sep" );
	sep->attach( stretchedBelow, constzfld_ );

	dataselfld_ = new uiTableImpDataSel( this, fd_,
                      mODHelpKey(mTableImpDataSelpicksHelpID) );
	dataselfld_->attach( alignedBelow, constzfld_ );
	dataselfld_->attach( ensureBelow, sep );

	sep = new uiSeparator( this, "H sep" );
	sep->attach( stretchedBelow, dataselfld_ );

	objfld_->attach( alignedBelow, constzfld_ );
	objfld_->attach( ensureBelow, sep );

	colorfld_ = new uiColorInput( this,
				   uiColorInput::Setup(getRandStdDrawColor()).
				   lbltxt(uiStrings::sColor()) );
	colorfld_->attach( alignedBelow, objfld_ );

	polyfld_ = new uiCheckBox( this, tr("Import as Polygon") );
	polyfld_->attach( rightTo, colorfld_ );
    }
    else
	filefld_->attach( alignedBelow, objfld_ );
}


uiImpExpPickSet::~uiImpExpPickSet()
{}


void uiImpExpPickSet::inputChgd( CallBacker* )
{
    storedid_.setUdf();
    FilePath fnmfp( filefld_->fileName() );
    objfld_->setInputText( fnmfp.baseName() );
}


void uiImpExpPickSet::formatSel( CallBacker* )
{
    const int zchoice = zfld_->box()->currentItem();
    const bool iszreq = zchoice == 0;
    constzfld_->display( zchoice == 1 );
    horinpfld_->display( zchoice == 2 );
    PickSetAscIO::updateDesc( fd_, iszreq );
    dataselfld_->updateSummary();
}


#define mErrRet(s) { uiMSG().error(s); return false; }

bool uiImpExpPickSet::doImport()
{
    const char* fname = filefld_->fileName();
    od_istream strm( fname );
    if ( !strm.isOK() )
	mErrRet( uiStrings::phrCannotOpen(uiStrings::sInputFile().toLower()) )

    const char* psnm = objfld_->getInput();
    Pick::Set ps( psnm );
    const int zchoice = zfld_->box()->currentItem();
    float constz = zchoice==1 ? constzfld_->getfValue() : 0;
    if ( SI().zIsTime() ) constz /= 1000;

    ps.disp_.mkstyle_.color_ = colorfld_->color();
    PickSetAscIO aio( fd_ );
    aio.get( strm, ps, zchoice==0, constz );

    if ( zchoice == 2 )
    {
	serv_->fillZValsFrmHor( &ps, horinpfld_->box()->currentItem() );
    }

    const IOObj* objfldioobj = objfld_->ioobj();
    if ( !objfldioobj ) return false;
    PtrMan<IOObj> ioobj = objfldioobj->clone();
    const bool ispolygon = polyfld_->isChecked();
    if ( ispolygon )
    {
	ps.disp_.connect_ = Pick::Set::Disp::Close;
	ioobj->pars().set( sKey::Type(), sKey::Polygon() );
    }
    else
    {
	ps.disp_.connect_ = Pick::Set::Disp::None;
	ioobj->pars().set(sKey::Type(), PickSetTranslatorGroup::sKeyPickSet());
    }

    if ( !IOM().commitChanges(*ioobj) )
	mErrRet(IOM().errMsg())

    uiString errmsg;
    if ( !PickSetTranslator::store(ps,ioobj,errmsg) )
	mErrRet(errmsg);

    storedid_ = ioobj->key();
    if ( saveButtonChecked() )
    {
	Pick::SetMgr& psmgr = Pick::Mgr();
	int setidx = psmgr.indexOf( storedid_ );
	if ( setidx < 0 )
	{
	    Pick::Set* newps = new Pick::Set( ps );
	    psmgr.set( storedid_, newps );
	    setidx = psmgr.indexOf( storedid_ );
	    importReady.trigger();
	}
	else
	{
	    Pick::Set& oldps = psmgr.get( setidx );
	    oldps = ps;
	    psmgr.reportChange( 0, oldps );
	    psmgr.reportDispChange( 0, oldps );
	}

	psmgr.setUnChanged( setidx, true );
    }

    return true;
}


bool uiImpExpPickSet::doExport()
{
    const IOObj* objfldioobj = objfld_->ioobj();
    if ( !objfldioobj ) return false;

    PtrMan<IOObj> ioobj = objfldioobj->clone();
    uiString errmsg; Pick::Set ps;
    if ( !PickSetTranslator::retrieve(ps,ioobj,errmsg) )
	mErrRet(errmsg)

    const char* fname = filefld_->fileName();
    StreamData sdo = StreamProvider( fname ).makeOStream();
    if ( !sdo.usable() )
    {
	sdo.close();
	mErrRet(uiStrings::phrCannotOpen(uiStrings::phrOutput(
		uiStrings::sFile())))
    }

    *sdo.ostrm << std::fixed;
    BufferString buf;
    for ( int locidx=0; locidx<ps.size(); locidx++ )
    {
	ps[locidx].toString( buf, true );
	*sdo.ostrm << buf.buf() << '\n';
    }

    *sdo.ostrm << '\n';
    sdo.close();
    return true;
}


bool uiImpExpPickSet::acceptOK( CallBacker* )
{
    uiMsgMainWinSetter mws( this );

    if ( !checkInpFlds() ) return false;
    bool ret = import_ ? doImport() : doExport();
    if ( !ret ) return false;

    uiString msg = tr("Pickset successfully %1."
		      "\n\nDo you want to %2 more PickSets?")
		 .arg(import_ ? tr("imported") : tr("exported"))
		 .arg(import_ ? uiStrings::sImport() : uiStrings::sExport());
    return !uiMSG().askGoOn( msg, uiStrings::sYes(), tr("No, close window") );
}


bool uiImpExpPickSet::checkInpFlds()
{
    BufferString filenm = filefld_->fileName();
    if ( import_ && !File::exists(filenm) )
	mErrRet( uiStrings::phrSelect(uiStrings::sInputFile().toLower()) );

    if ( !import_ && filenm.isEmpty() )
	mErrRet( uiStrings::sSelOutpFile() );

    if ( !objfld_->commitInput() )
	return false;

    if ( import_ )
    {
	if ( !dataselfld_->commit() )
	    mErrRet( uiStrings::phrSpecify(tr("data format")) );

	const int zchoice = zfld_->box()->currentItem();
	if ( zchoice == 1 )
	{
	    float constz = constzfld_->getfValue();
	    if ( SI().zIsTime() ) constz /= 1000;

	    if ( !SI().zRange(false).includes( constz,false ) )
		mErrRet( uiStrings::phrEnter(tr("a valid Z value")) );
	}
    }

    return true;
}
