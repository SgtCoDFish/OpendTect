#ifndef unitofmeasure_h
#define unitofmeasure_h

/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	A.H.Bril
 Date:		Feb 2004
 RCS:		$Id: unitofmeasure.h,v 1.5 2006-08-21 17:14:44 cvsbert Exp $
________________________________________________________________________

-*/

#include "namedobj.h"
#include "property.h"
#include "scaler.h"
#include "repos.h"

class UnitOfMeasureRepository;

UnitOfMeasureRepository& UoMR();


/*!\brief Unit of Measure
 
 Only linear transformations to SI units supported.

 All units of measure in OpendTect are available through the UoMR() instance
 accessor of the singleton UnitOfMeasureRepository instance.

 */

class UnitOfMeasure : public NamedObject
{
public:

    			UnitOfMeasure()
			    : proptype_(PropertyRef::Other)
			    , source_(Repos::Temp) {}
    			UnitOfMeasure( const char* n, const char* s, double f,
				      PropertyRef::StdType t=PropertyRef::Other)
			    : NamedObject(n), symbol_(s)
			    , scaler_(0,f), source_(Repos::Temp)
			    , proptype_(t)	{}
			UnitOfMeasure( const UnitOfMeasure& uom )
			    			{ *this = uom; }
    UnitOfMeasure&	operator =(const UnitOfMeasure&);

    const char*		symbol() const		{ return symbol_.buf(); }
    PropertyRef::StdType propType() const	{ return proptype_; }
    const LinScaler&	scaler() const		{ return scaler_; }

    void		setSymbol( const char* s )
						{ symbol_ = s; }
    void		setScaler( const LinScaler& s )
						{ scaler_ = s; }
    void		setPropType( PropertyRef::StdType t )
						{ proptype_ = t; }

    template <class T>
    T			internalValue( T inp ) const
    						{ return scaler_.scale(inp); }
    template <class T>
    T			userValue( T inp ) const
						{ return scaler_.unScale(inp); }

    static const UnitOfMeasure* getGuessed(const char*);
    Repos::Source	source() const			{ return source_; }
    void		setSource( Repos::Source s )	{ source_ = s; }

protected:

    BufferString	symbol_;
    LinScaler		scaler_;
    PropertyRef::StdType proptype_;
    Repos::Source	source_;

};


/*!\brief Repository of all Units of Measure in the system.
 
 At first usage of the singleton instance of this class (accessible through
 the global UoMR() function), the data files for the repository are
 searched, by iterating through the different 'Repos' sources (see repos.h).
 Then, the standard ones like 'feet' are added if they are not yet defined
 in one of the files.

 */


class UnitOfMeasureRepository
{
public:

    const UnitOfMeasure* get(const char* nm) const;
    			//!< Will try names first, then symbols, otherwise null
    static const char*	guessedStdName(const char*);
    			//!< May return null

    const ObjectSet<const UnitOfMeasure>& all() const	{ return entries; }
    void		getRelevant(PropertyRef::StdType,
	    			    ObjectSet<const UnitOfMeasure>&) const;

    bool		add(const UnitOfMeasure&);
    			//!< returns whether already present
    bool		write(Repos::Source) const;

private:

    			UnitOfMeasureRepository();

    ObjectSet<const UnitOfMeasure> entries;

    void		addUnitsFromFile(const char*,Repos::Source);

    friend UnitOfMeasureRepository& UoMR();

};


#endif
