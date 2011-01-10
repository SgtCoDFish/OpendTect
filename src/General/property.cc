/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : A.H. Bril
 * DATE     : Dec 2003
-*/

static const char* rcsID = "$Id: property.cc,v 1.36 2011-01-10 13:29:58 cvsbert Exp $";

#include "propertyimpl.h"
#include "propertyref.h"
#include "mathexpression.h"
#include "unitofmeasure.h"
#include "survinfo.h"
#include "ascstream.h"
#include "safefileio.h"
#include "ioman.h"
#include "separstr.h"
#include "globexpr.h"
#include "keystrs.h"
#include "iopar.h"
#include "errh.h"

static const char* filenamebase = "Properties";
static const char* sKeyAliases = "Aliases";
static const char* sKeyIsLog = "Logarithmic";

mImplFactory1Param(Property,const PropertyRef&,Property::factory)

DefineEnumNames(PropertyRef,StdType,0,"Standard Property")
{
	"Other",
	"Time",
	"Distance/Depth",
	"Porosity",
	"Permeability",
	"Gamma Ray",
	"Temperature",
	"Pressure",
	"Density",
	"Velocity",
	"Sonic travel time",
	"Acoustic Impedance",
	"Saturation",
	"Electrical Potential",
	"Resistivity",
	"Poisson's Ratio",
	"Compressibility",
	0
};


float PropertyRef::DispDefs::possibleValue() const
{
    const bool udf0 = mIsUdf(range_.start);
    const bool udf1 = mIsUdf(range_.stop);
    if ( udf0 && udf1 )
	return 0;
    if ( udf0 || udf1 )
	return udf0 ? range_.stop : range_.start;
    return range_.center();
}


const PropertyRef& PropertyRef::undef()
{
    static PropertyRef* udf = 0;
    if ( !udf )
    {
	udf = new PropertyRef( "Undef" );
	udf->aliases().add( "" );
	udf->aliases().add( "undef*" );
	udf->aliases().add( "?undef?" );
	udf->aliases().add( "?undefined?" );
	udf->aliases().add( "udf" );
	udf->aliases().add( "unknown" );
	udf->disp_.color_ = Color::LightGrey();
    }
    return *udf;
}


PropertyRef::StdType PropertyRef::surveyZType()
{
    return SI().zIsTime() ? Time : Dist;
}


bool PropertyRef::isKnownAs( const char* nm ) const
{
    if ( !nm || !*nm )
	return this == &undef();

    if ( caseInsensitiveEqual(nm,name().buf(),0) )
	return true;
    for ( int idx=0; idx<aliases_.size(); idx++ )
    {
	GlobExpr ge( aliases_.get(idx), false );
	if ( ge.matches(nm) )
	    return true;
    }
    return false;
}


void PropertyRef::usePar( const IOPar& iop )
{
    aliases_.erase();
    FileMultiString fms( iop.find(sKeyAliases) );
    int sz = fms.size();
    for ( int ifms=0; ifms<sz; ifms++ )
	aliases_.add( fms[ifms] );

    fms = iop.find( sKey::Range );
    sz = fms.size();
    if ( sz > 1 )
    {
	disp_.range_.start = toFloat( fms[0] );
	disp_.range_.stop = toFloat( fms[1] );
	if ( sz > 2 )
	{
	    disp_.unit_ = fms[2];
	    const UnitOfMeasure* uom = UoMR().get( disp_.unit_ );
	    if ( uom )
	    {
		if ( !mIsUdf(disp_.range_.start) )
		    disp_.range_.start = uom->getSIValue(disp_.range_.start);
		if ( !mIsUdf(disp_.range_.stop) )
		    disp_.range_.stop = uom->getSIValue(disp_.range_.stop);
	    }
	}
    }

    iop.get( sKey::Color, disp_.color_ );
    iop.getYN( sKeyIsLog, disp_.logarithmic_ );
}


void PropertyRef::fillPar( IOPar& iop ) const
{
    if ( aliases_.isEmpty() )
	iop.removeWithKey( sKeyAliases );
    else
    {
	FileMultiString fms( aliases_.get(0) );
	for ( int idx=1; idx<aliases_.size(); idx++ )
	    fms += aliases_.get(idx);
	iop.set( sKeyAliases, fms );
    }

    iop.set( sKey::Color, disp_.color_ );
    FileMultiString fms;
    char str[255];
    fms += toString( disp_.range_.start, str );
    fms += toString( disp_.range_.stop, str );
    if ( !disp_.unit_.isEmpty() )
	fms += disp_.unit_;
    iop.set( sKey::Range, fms );
    if ( disp_.logarithmic_ )
	iop.setYN( sKeyIsLog, true );
    else
	iop.removeWithKey( sKeyIsLog );
}


class PropertyRefSetMgr : public CallBacker
{
public:

PropertyRefSetMgr()
    : prs_(0)
{
    IOM().surveyChanged.notify( mCB(this,PropertyRefSetMgr,doNull) );
}

~PropertyRefSetMgr()
{
    delete prs_;
}

void doNull( CallBacker* )
{
    delete prs_; prs_ = 0;
}

void createSet()
{
    Repos::FileProvider rfp( filenamebase, true );
    while ( rfp.next() )
    {
	const BufferString fnm( rfp.fileName() );
	SafeFileIO sfio( fnm );
	if ( !sfio.open(true) )
	    continue;

	ascistream astrm( sfio.istrm(), true );
	PropertyRefSet* tmp = new PropertyRefSet;
	tmp->readFrom( astrm );
	if ( tmp->isEmpty() )
	    delete tmp;
	else
	    { prs_ = tmp; break; }
    }

    if ( !prs_ )
	prs_ = new PropertyRefSet;
}

    PropertyRefSet*	prs_;

};

const PropertyRefSet& PROPS()
{
    static PropertyRefSetMgr rsm;
    if ( !rsm.prs_ )
	rsm.createSet();
    return *rsm.prs_;
}


int PropertyRefSet::indexOf( const char* nm ) const
{
    if ( nm && *nm )
    {
	for ( int idx=0; idx<size(); idx++ )
	{
	    const PropertyRef& pr = *(*this)[idx];
	    if ( pr.name() == nm )
		return idx;
	}
	for ( int idx=0; idx<size(); idx++ )
	{
	    const PropertyRef& pr = *(*this)[idx];
	    if ( pr.isKnownAs(nm) )
		return idx;
	}
    }
    return -1;
}


int PropertyRefSet::indexOf( PropertyRef::StdType st, int occ ) const
{
    for ( int idx=0; idx<size(); idx++ )
    {
	const PropertyRef& pr = *(*this)[idx];
	if ( pr.hasType(st) )
	{
	    occ--;
	    if ( occ < 0 )
		return idx;
	}
    }

    return -1;
}


PropertyRef* PropertyRefSet::fnd( const char* nm ) const
{
    const int idx = indexOf( nm );
    return idx < 0 ? 0 : const_cast<PropertyRef*>( (*this)[idx] );
}


int PropertyRefSet::add( PropertyRef* pr )
{
    if ( !pr ) return -1;

    const int idx = indexOf( pr->name() );
    if ( idx < 0 )
	{ ObjectSet<PropertyRef>::operator+=( pr ); return size()-1; }

    return -1;
}


int PropertyRefSet::ensurePresent( PropertyRef::StdType st, const char* nm1,
				   const char* nm2, const char* nm3 )
{
    int idx = indexOf( nm1 );
    if ( idx < 0 && nm2 )
	idx = indexOf( nm2 );
    if ( idx < 0 && nm3 )
	idx = indexOf( nm3 );
    if ( idx < 0 )
    {
	idx = indexOf( st );
	if ( idx >= 0 )
	    return idx;
    }
    if ( idx < 0 )
    {
	PropertyRef* pr = new PropertyRef( nm1, st );
	if ( nm2 && *nm2 ) pr->aliases().add( nm2 );
	if ( nm3 && *nm3 ) pr->aliases().add( nm3 );
	pr->disp_.color_ = Color::stdDrawColor( (int)st );
	idx = add( pr );
    }
    return idx;
}


bool PropertyRefSet::save( Repos::Source src ) const
{
    Repos::FileProvider rfp( filenamebase );
    BufferString fnm = rfp.fileName( src );

    SafeFileIO sfio( fnm, true );
    if ( !sfio.open(false) )
    {
	BufferString msg( "Cannot write to " ); msg += fnm;
	ErrMsg( sfio.errMsg() );
	return false;
    }

    ascostream astrm( sfio.ostrm() );
    if ( !writeTo(astrm) )
	{ sfio.closeFail(); return false; }

    sfio.closeSuccess();
    return true;
}


void PropertyRefSet::readFrom( ascistream& astrm )
{
    deepErase( *this );

    while ( !atEndOfSection(astrm.next()) )
    {
	IOPar iop; iop.getFrom(astrm);
	const BufferString propnm( iop.getKey(0) );
	if ( find(propnm) )
	    continue;

	const BufferString stdtypstr( iop.getValue(0) );
	PropertyRef::StdType st;
        PropertyRef::parseEnumStdType(stdtypstr, st );
	PropertyRef* pr = new PropertyRef( propnm, st );
	pr->usePar( iop );

	if ( add(pr) < 0 )
	    delete pr;
    }
}


bool PropertyRefSet::writeTo( ascostream& astrm ) const
{
    astrm.putHeader( "Properties" );
    for ( int idx=0; idx<size(); idx++ )
    {
	const PropertyRef& pr = *(*this)[idx];
	IOPar iop;
	iop.set( pr.name(), PropertyRef::getStdTypeString(pr.stdType()) );
	pr.fillPar( iop );
	iop.putTo( astrm );
    }
    return astrm.stream().good();
}


const char* Property::name() const
{
    return ref_.name().buf();
}


const char* ValueProperty::def() const
{
    static char buf[255];
    return toString( val_, buf );
}


void ValueProperty::setDef( const char* defstr )
{
    if ( defstr && *defstr )
	val_ = toFloat( defstr );
}


bool ValueProperty::isUdf() const
{
    return mIsUdf(val_);
}


float ValueProperty::value( Property::EvalOpts ) const
{
    return val_;
}


const char* RangeProperty::def() const
{
    if ( isUdf() )
	return "1e30`0";

    static FileMultiString fms;
    char str[255];
    fms = toString(rg_.start, str);
    fms += toString(rg_.stop, str);
    return fms.buf();
}


void RangeProperty::setDef( const char* defstr )
{
    if ( !defstr || !*defstr )
	rg_.start = mUdf(float);
    else
    {
	FileMultiString fms( defstr );
	rg_.start = toFloat( fms[0] );
	rg_.stop = toFloat( fms[1] );
    }
}


bool RangeProperty::isUdf() const
{
    return mIsUdf(rg_.start);
}


float RangeProperty::value( Property::EvalOpts eo ) const
{
    if ( isUdf() )
	return mUdf(float);
    else if ( eo.average_ )
	return 0.5 * (rg_.start + rg_.stop);
    return rg_.start + eo.relpos_ * (rg_.stop - rg_.start);
}


MathProperty::MathProperty( const PropertyRef& pr, const char* df )
    : Property(pr)
    , expr_(0)
{
    inps_.allowNull( true );
    if ( df && *df )
	setDef( df );
}


MathProperty::~MathProperty()
{
    delete expr_;
}


int MathProperty::nrInputs() const
{
    return expr_ ? expr_->nrVariables() : 0;
}


const char* MathProperty::inputName( int idx ) const
{
    return expr_ ? expr_->fullVariableExpression(idx) : 0;
}


void MathProperty::setInput( int idx, const Property* p )
{
    if ( p && p->dependsOn(*this) )
    {
	BufferString msg( "Invalid cyclic dependency for property " );
	msg += ref().name();
	ErrMsg( msg );
	p = 0;
    }
    inps_.replace( idx, p );
}


bool MathProperty::dependsOn( const Property& p ) const
{
    if ( &p == this )
	return true;

    for ( int idx=0; idx<inps_.size(); idx++ )
    {
	const Property* inp = inps_[idx];
	if ( inp && inp->dependsOn(p) )
	    return true;
    }
    return false;
}


void MathProperty::ensureGoodVariableName( char* nm )
{
    if ( !nm || !*nm )
        { pFreeFnErrMsg("Knurft","ensureGoodVariableName"); return; }
    cleanupString( nm, false, false, false );
    replaceCharacter( nm, '+', '_' );
    replaceCharacter( nm, '-', '_' );
}


static bool isMathMatch( const BufferString& reqnm, const char* str )
{
    BufferString depnm( str );
    MathProperty::ensureGoodVariableName( depnm.buf() );
    return depnm == reqnm;
}


const Property* MathProperty::findInput( const PropertySet& ps, const char* nm,
					 bool mainname ) const
{
    BufferString reqnm( nm ); ensureGoodVariableName( reqnm.buf() );
    for ( int idx=0; idx<ps.size(); idx++ )
    {
	const Property& depp = ps.get( idx );
	if ( this == &depp ) continue;
	if ( mainname )
	{
	    if ( isMathMatch(reqnm,depp.name()) )
		return &depp;
	}
	else
	{
	    for ( int ial=0; ial<depp.ref().aliases().size(); ial++ )
	    {
		if ( isMathMatch(reqnm,depp.ref().aliases().get(ial).buf()) )
		    return &depp;
	    }
	}
    }

    return 0;
}


bool MathProperty::init( const PropertySet& ps ) const
{
    const int nrinps = expr_->nrVariables();
    inps_.erase();
    while ( nrinps > inps_.size() )
	inps_ += findInput( ps, inputName(inps_.size()), true );

    for ( int idep=0; idep<nrinps; idep++ )
    {
	if ( inps_[idep] ) continue;
	const char* nm = inputName( idep );
	inps_.replace( idep, findInput( ps, nm, false ) );
	if ( !inps_[idep] )
	{
	    errmsg_ = "Missing input for '";
	    errmsg_.add(name()).add("': '").add(nm).add("'");
	    return false;
	}
    }

    return true;
}


const char* MathProperty::def() const
{
    return def_.buf();
}


void MathProperty::setDef( const char* s )
{
    inps_.erase();
    def_ = s;
    MathExpressionParser mep( def_ );
    delete expr_; expr_ = mep.parse();
    if ( !expr_ ) return;
    const int sz = expr_->nrVariables();
    while ( sz > inps_.size() )
	inps_ += 0;
}


bool MathProperty::isUdf() const
{
    if ( !expr_ )
	return true;
    for ( int idx=0; idx<inps_.size(); idx++ )
	if ( !inps_[idx] )
	    return true;
    return false;
}


float MathProperty::value( Property::EvalOpts eo ) const
{
    if ( !expr_ )
	return mUdf(float);

    for ( int idx=0; idx<inps_.size(); idx++ )
    {
	const Property* p = inps_[idx];
	if ( !p )
	    return mUdf(float);

	const float v = p->value(eo);
	if ( mIsUdf(v) )
	    return mUdf(float);

	expr_->setVariableValue( idx, v );
    }

    return expr_->getValue();
}


PropertySet::PropertySet( const PropertyRefSelection& prs )
{
    for ( int idx=0; idx<prs.size(); idx++ )
	props_ += new ValueProperty( *prs[idx] );
}


void PropertySet::replace( int idx, Property* p )
{
    if ( p )
	delete props_.replace( idx, p );
}

int PropertySet::indexOf( const char* nm, bool matchaliases ) const
{
    if ( !nm || !*nm ) return -1;

    for ( int idx=0; idx<props_.size(); idx++ )
    {
	const Property& p = *props_[idx];
	if ( p.ref().name() == nm )
	    return idx;
    }
    if ( matchaliases )
    {
	for ( int idx=0; idx<props_.size(); idx++ )
	{
	    const Property& p = *props_[idx];
	    if ( p.ref().isKnownAs(nm) )
		return idx;
	}
    }

    return -1;
}


Property* PropertySet::fnd( const char* nm, bool ma ) const
{
    const int idx = indexOf(nm,ma);
    return idx < 0 ? 0 : const_cast<Property*>( props_[idx] );
}


int PropertySet::indexOf( PropertyRef::StdType st, int occ ) const
{
    for ( int idx=0; idx<size(); idx++ )
    {
	const Property& pr = *props_[idx];
	if ( pr.ref().hasType(st) )
	{
	    occ--;
	    if ( occ < 0 )
		return idx;
	}
    }

    return -1;
}


bool PropertySet::add( Property* p )
{
    if ( !p ) return false;
    if ( indexOf(p->name(),false) >= 0 )
	return false;
    props_ += p;
    return true;
}


int PropertySet::set( Property* p )
{
    if ( !p ) return -1;

    int idxof = indexOf( p->name(), false );
    if ( idxof >= 0 )
    	delete props_.replace( idxof, p );
    else
    {
	idxof = props_.size();
	props_ += p;
    }
    return idxof;
}


void PropertySet::remove( int idx )
{
    delete props_.remove( idx );
}


bool PropertySet::prepareUsage() const
{
    for ( int idx=0; idx<size(); idx++ )
    {
	if ( !props_[idx]->init(*this) )
	    { errmsg_ = props_[idx]->errMsg(); return false; }
    }
    return true;
}
