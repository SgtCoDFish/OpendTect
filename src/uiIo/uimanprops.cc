/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bert
 Date:          June 2011
________________________________________________________________________

-*/
static const char* rcsID = "$Id: uimanprops.cc,v 1.6 2011-10-14 12:09:02 cvsbert Exp $";

#include "uimanprops.h"
#include "uibuildlistfromlist.h"
#include "uigeninput.h"
#include "uilistbox.h"
#include "uiunitsel.h"
#include "uicolor.h"
#include "uitoolbutton.h"
#include "uimsg.h"
#include "propertyref.h"
#include "separstr.h"
#include "unitofmeasure.h"


class uiBuildPROPS : public uiBuildListFromList
{
public:
			uiBuildPROPS(uiParent*,PropertyRefSet&);

    PropertyRefSet& 	props_;

    virtual const char*	avFromDef(const char*) const;
    virtual void	editReq(bool);
    virtual void	removeReq();
    virtual void	itemSwitch(const char*,const char*);
    void		initGrp(CallBacker*);

};


uiBuildPROPS::uiBuildPROPS( uiParent* p, PropertyRefSet& prs )
    : uiBuildListFromList(p,
	    uiBuildListFromList::Setup(false,"property type","usable property")
	    .withio(false).withtitles(true), "PropertyRef selection group")
    , props_(prs)
{
    const BufferStringSet dispnms( PropertyRef::StdTypeNames() );

    setAvailable( dispnms );

    BufferStringSet pnms;
    for ( int idx=0; idx<props_.size(); idx++ )
	pnms.add( props_[idx]->name() );
    pnms.sort();
    for ( int idx=0; idx<pnms.size(); idx++ )
	addItem( pnms.get(idx) );

    finaliseDone.notify( mCB(this,uiBuildPROPS,initGrp) );
}

void uiBuildPROPS::initGrp( CallBacker* )
{
    if ( !props_.isEmpty() )
	setCurDefSel( props_[0]->name() );
}

const char* uiBuildPROPS::avFromDef( const char* nm ) const
{
    const PropertyRef* pr = props_.find( nm );
    if ( !pr ) return 0;
    return PropertyRef::toString( pr->stdType() );
}


class uiEditPropRef : public uiDialog
{
public:

	    		uiEditPropRef(uiParent*,PropertyRef&,bool);
    bool		acceptOK(CallBacker*);

    PropertyRef&	pr_;

    uiGenInput*		namefld_;
    uiGenInput*		aliasfld_;
    uiGenInput*		rgfld_;
    uiUnitSel*		unfld_;
    uiColorInput*	colfld_;

};


uiEditPropRef::uiEditPropRef( uiParent* p, PropertyRef& pr, bool isadd )
    : uiDialog(p,uiDialog::Setup("Property definition",
		BufferString(isadd?"Add '":"Edit '",
		    PropertyRef::toString(pr.stdType()),"' property"),
		mTODOHelpID))
    , pr_(pr)
{
    namefld_ = new uiGenInput( this, "Name", StringInpSpec(pr.name()) );
    SeparString ss;
    for ( int idx=0; idx<pr_.aliases().size(); idx++ )
	ss += pr_.aliases().get(idx);
    aliasfld_ = new uiGenInput( this, "Aliases (e.g. 'abc, uvw*xyz')",
	   			StringInpSpec(ss.buf()) );
    aliasfld_->attach( alignedBelow, namefld_ );

    colfld_ = new uiColorInput( this, uiColorInput::Setup(pr_.disp_.color_)
	    				.lbltxt("Default display color") );
    colfld_->attach( alignedBelow, aliasfld_ );
    rgfld_ = new uiGenInput( this, "Typical value range",
			     FloatInpIntervalSpec() );
    rgfld_->attach( alignedBelow, colfld_ );
    unfld_ = new uiUnitSel( this, pr_.stdType() );
    unfld_->setUnit( pr_.disp_.unit_ );
    unfld_->attach( rightOf, rgfld_ );

    const UnitOfMeasure* un = unfld_->getUnit();
    Interval<float> vintv( pr_.disp_.range_ );
    if ( un )
    {
	if ( !mIsUdf(vintv.start) )
	    vintv.start = un->getUserValueFromSI( vintv.start );
	if ( !mIsUdf(vintv.stop) )
	    vintv.stop = un->getUserValueFromSI( vintv.stop );
    }
    rgfld_->setValue( vintv );
}


#define mErrRet(s) { uiMSG().error(s); return false; }

bool uiEditPropRef::acceptOK( CallBacker* )
{
    const BufferString newnm( namefld_->text() );
    if ( newnm.isEmpty() || !isalpha(newnm[0]) || newnm.size() < 2 )
	mErrRet("Please enter a valid name");

    pr_.setName( newnm );
    SeparString ss( aliasfld_->text() ); const int nral = ss.size();
    pr_.aliases().erase();
    for ( int idx=0; idx<nral; idx++ )
	pr_.aliases().add( ss[idx] );
    pr_.disp_.color_ = colfld_->color();
    const UnitOfMeasure* un = unfld_->getUnit();
    Interval<float> vintv( rgfld_->getFInterval() );
    if ( !un )
	pr_.disp_.unit_.setEmpty();
    else
    {
	pr_.disp_.unit_ = un->name();
	if ( !mIsUdf(vintv.start) )
	    vintv.start = un->getSIValue( vintv.start );
	if ( !mIsUdf(vintv.stop) )
	    vintv.stop = un->getSIValue( vintv.stop );
    }
    pr_.disp_.range_ = vintv;

    return true;
}


void uiBuildPROPS::editReq( bool isadd )
{
    const char* nm = isadd ? curAvSel() : curDefSel();
    if ( !nm || !*nm ) return;

    PropertyRef* pr = 0;
    if ( !isadd )
	pr = props_.find( nm );
    else
    {
	PropertyRef::StdType typ = PropertyRef::Other;
	PropertyRef::parseEnumStdType( nm, typ );
	pr = new PropertyRef( "", typ );
    }
    if ( !pr ) return;

    uiEditPropRef dlg( this, *pr, isadd );
    if ( dlg.go() )
    {
	if ( isadd )
	    props_ += pr;
	handleSuccessfullEdit( isadd, pr->name() );
    }
}


void uiBuildPROPS::removeReq()
{
    const char* prnm = curDefSel();
    if ( prnm && *prnm )
    {
	const int idx = props_.indexOf( prnm );
	if ( idx < 0 ) return;
	delete props_.remove( idx );
	removeItem();
    }
}


void uiBuildPROPS::itemSwitch( const char* nm1, const char* nm2 )
{
    const int idx1 = props_.indexOf( nm1 );
    const int idx2 = props_.indexOf( nm2 );
    if ( idx1 < 0 || idx2 < 0 ) { pErrMsg("Huh"); return; }
    props_.swap( idx1, idx2 );
}


uiManPROPS::uiManPROPS( uiParent* p )
    : uiDialog(p,uiDialog::Setup("Layer Properties",
				"Define possible layer properties",mTODOHelpID))
{
    setCtrlStyle( LeaveOnly );
    buildfld_ = new uiBuildPROPS( this, ePROPS() );
    static const char* strs[] = { "For this survey only",
				  "As default for all surveys",
				  "As default for my user ID only", 0 };
    srcfld_ = new uiGenInput( this, "Store", StringListInpSpec(strs) );
    srcfld_->attach( centeredBelow, buildfld_ );
}


bool uiManPROPS::rejectOK( CallBacker* )
{
    if ( !haveUserChange() )
	return true;

    ePROPS() = buildfld_->props_;
    const int isrc = srcfld_->getIntValue();
    const Repos::Source repsrc =   isrc == 0	? Repos::Survey
				: (isrc == 1	? Repos::Data
						: Repos::User);

    if ( !PROPS().save(repsrc) )
	uiMSG().warning( "Could not store the definitions to file."
			 "\nPlease check file/directory permissions." );
    else if ( repsrc != Repos::Survey )
       	PROPS().save(Repos::Survey);

    return true;
}

bool uiManPROPS::haveUserChange() const
{
    return buildfld_->haveUserChange();
}



uiSelectPropRefs::uiSelectPropRefs( uiParent* p, PropertyRefSelection& prs,
			      const char* lbl )
    : uiDialog(p,uiDialog::Setup("Layer Properties",
				"Select layer properties to use",mTODOHelpID))
    , props_(PROPS())
    , prsel_(prs)
    , thref_(&PropertyRef::thickness())
    , structchg_(false)
{
    uiLabeledListBox* llb = 0;
    if ( !lbl || !*lbl )
	propfld_ = new uiListBox( this, "Available properties" );
    else
    {
	llb = new uiLabeledListBox( this, lbl, true,
				    uiLabeledListBox::AboveMid );
	propfld_ = llb->box();
    }
    propfld_->setItemsCheckable( true );
    fillList();

    uiToolButton* manpropsbut = new uiToolButton( this, "man_props.png",
	    				"Manage available properties",
					mCB(this,uiSelectPropRefs,manPROPS) );
    if ( llb )
	manpropsbut->attach( centeredRightOf, llb );
    else
	manpropsbut->attach( centeredRightOf, propfld_ );
}


void uiSelectPropRefs::fillList()
{
    BufferStringSet dispnms;
    for ( int idx=0; idx<props_.size(); idx++ )
    {
	const PropertyRef* ref = props_[idx];
	if ( ref != thref_ )
	    dispnms.add( ref->name() );
    }
    if ( dispnms.isEmpty() )
	return;

    dispnms.sort();
    propfld_->addItems( dispnms );

    int firstsel = -1;
    for ( int idx=0; idx<dispnms.size(); idx++ )
    {
	const char* nm = dispnms.get( idx ).buf();
	const bool issel = prsel_.isPresent( nm );
	propfld_->setItemChecked( idx, issel );
	if ( issel && firstsel < 0 ) firstsel = idx;
    }

    propfld_->setCurrentItem( firstsel < 0 ? 0 : firstsel );
}


void uiSelectPropRefs::manPROPS( CallBacker* )
{
    BufferStringSet orgnms;
    for ( int idx=0; idx<prsel_.size(); idx++ )
	orgnms.add( prsel_[idx]->name() );

    uiManPROPS dlg( this );
    if ( !dlg.go() )
	return;

    structchg_ = structchg_ || dlg.haveUserChange();

    // Even if user will cancel we cannot leave removed PROP's in the set
    for ( int idx=0; idx<orgnms.size(); idx++ )
    {
	if ( !props_.isPresent(prsel_[idx]) )
	    { structchg_ = true; prsel_.remove( idx ); idx--; }
    }

    propfld_->setEmpty();
    fillList();
}


bool uiSelectPropRefs::acceptOK( CallBacker* )
{
    prsel_.erase();
    prsel_.insertAt( &PropertyRef::thickness(), 0 );

    for ( int idx=0; idx<propfld_->size(); idx++ )
    {
	if ( !propfld_->isItemChecked(idx) )
	    continue;

	const char* pnm = propfld_->textOfItem( idx );
	const PropertyRef* pr = props_.find( pnm );
	if ( !pr ) { pErrMsg("Huh"); structchg_ = true; continue; }
	prsel_ += pr;
    }

    return true;
}
