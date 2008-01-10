/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Helene Huck
 Date:          January 2008
 RCS:           $Id: uiattremout.cc,v 1.1 2008-01-10 08:41:18 cvshelene Exp $
________________________________________________________________________

-*/


#include "uiattremout.h"
#include "attribdescset.h"
#include "attribengman.h"
#include "attriboutput.h"
#include "uiattrsel.h"
#include "ioobj.h"
#include "iopar.h"
#include "nladesign.h"
#include "nlamodel.h"
#include "uimsg.h"
#include "multiid.h"
#include "keystrs.h"

using namespace Attrib;

uiAttrEMOut::uiAttrEMOut( uiParent* p, const DescSet& ad,
			  const NLAModel* n, const MultiID& mid,
			  const char* dlgnm )
    : uiFullBatchDialog(p,Setup(dlgnm).procprognm("process_attrib_em"))
    , ads_(const_cast<DescSet&>(ad))
    , nlamodel_(n)
    , nlaid_(mid)
    , nladescid_( -1, true )
{
    setTitleText( "" );

    attrfld_ = new uiAttrSel(uppgrp_, &ads_, ads_.is2D(), "Quantity to output");
    attrfld_->setNLAModel( nlamodel_ );
    attrfld_->selectiondone.notify( mCB(this,uiAttrEMOut,attribSel) );
}


bool uiAttrEMOut::prepareProcessing()
{
    attrfld_->processInput();
    if ( attrfld_->attribID() < 0 && attrfld_->outputNr() < 0 )
    {
	uiMSG().error( "Please select the output quantity" );
	return false;
    }

    return true;
}


bool uiAttrEMOut::fillPar( IOPar& iopar )
{
    if ( nlamodel_ && attrfld_->outputNr() >= 0 )
    {
	if ( !nlaid_ || !(*nlaid_) )
	{ 
	    uiMSG().message( "NN needs to be stored before creating volume" ); 
	    return false; 
	}
	if ( !addNLA( nladescid_ ) )	return false;
    }

    const DescID targetid = nladescid_ < 0 ? attrfld_->attribID() : nladescid_;
    DescSet* clonedset = ads_.optimizeClone( targetid );
    if ( !clonedset )
	return false;

    IOPar attrpar( "Attribute Descriptions" );
    clonedset->fillPar( attrpar );
	    
    for ( int idx=0; idx<attrpar.size(); idx++ )
    {
        const char* nm = attrpar.getKey( idx );
        iopar.add( IOPar::compKey(SeisTrcStorOutput::attribkey,nm),
		   attrpar.getValue(idx) );
    }

    ads_.removeDesc( nladescid_ );
    return true;
}


void uiAttrEMOut::fillOutPar( IOPar& iopar, const char* outtyp,
			      const char* idlbl, MultiID mid )
{
    BufferString key;
    BufferString keybase = Output::outputstr; keybase += ".1.";
    key = keybase; key += sKey::Type;
    iopar.set( key, outtyp );

    key = keybase; key += SeisTrcStorOutput::attribkey;
    key += "."; key += DescSet::highestIDStr();
    iopar.set( key, 1 );

    key = keybase; key += SeisTrcStorOutput::attribkey; key += ".0";
    iopar.set( key, nladescid_ < 0 ? attrfld_->attribID().asInt() 
	    			  : nladescid_.asInt() );

    key = keybase; key += idlbl;
    iopar.set( key, mid );
}

#define mErrRet(str) { uiMSG().message( str ); return false; }

bool uiAttrEMOut::addNLA( DescID& id )
{
    BufferString defstr("NN specification=");
    defstr += nlaid_;

    const int outputnr = attrfld_->outputNr();
    BufferString errmsg;
    EngineMan::addNLADesc( defstr, id, ads_, outputnr, nlamodel_, errmsg );
    if ( errmsg.size() )
	mErrRet( errmsg );

    return true;
}
