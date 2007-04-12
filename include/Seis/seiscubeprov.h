#ifndef seismscprov_h
#define seismscprov_h

/*
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	A.H. Bril
 Date:		Jan 2007
 RCS:		$Id: seiscubeprov.h,v 1.7 2007-04-12 06:40:57 cvsbert Exp $
________________________________________________________________________

*/


#include "rowcol.h"
#include "sets.h"
class IOObj;
class BinID;
class MultiID;
class SeisTrc;
class SeisTrcBuf;
class SeisSelData;
class SeisTrcReader;


/*!\brief Reads seismic data into buffers providing a Moving Virtual Subcube
          of seismic data.

This is a SeisTrcGroup that allows advancing by reading traces from storage.
Note that the provider may skip incomplete parts.

The get() method returns a pointer to the trace, where you specify the
inline and crossline number relative to the center. This is irrespective
the steps in the cube's numbers. Therefore, the actual inline number of
get(1,0) may be 10 higher than get(0,0) .

The next() method moves the cube one further along the seismic storage.
Depending on what is available in the cube, the return value will tell
you whether you can work on the new position.

You canspecify two stepouts: required and desired. The required stepout traces
will always be available when the return of next() is DataOK. If DataIncomplete
is returned, then the provider is still gathering more traces.
 
 */


class SeisMSCProvider
{
public:

			SeisMSCProvider(const MultiID&);
				//!< Use any real user entry from '.omf' file
			SeisMSCProvider(const IOObj&);
				//!< Use any real user entry from '.omf' file
			SeisMSCProvider(const char* fnm);
				//!< 'loose' 3D Post-stack CBVS files only.
    virtual		~SeisMSCProvider();

    bool		is2D() const;
    bool		prepareWork();
    			//!< Opens the input data. Can still set stepouts etc.

    			// use the following after prepareWork
    			// but before the first next()
    void		forceFloatData( bool yn )
    			{ intofloats_ = yn; }
    void		setStepout(int,int,bool required);
    void		setStepoutStep( int i, int c )
			{ stepoutstep_.r() = i; stepoutstep_.c() = c; }
    int			inlStepout( bool req ) const
    			{ return req ? reqstepout_.r() : desstepout_.r(); }
    int			crlStepout( bool req ) const
    			{ return req ? reqstepout_.c() : desstepout_.c(); }

    enum AdvanceState	{ NewPosition, Buffering, EndReached, Error };
    AdvanceState	advance();	
    const char*		errMsg() const		{ return errmsg_; }

    BinID		getPos() const;
    int			getTrcNr() const;
    SeisTrc*		get(int deltainl,int deltacrl);
    SeisTrc*		get(const BinID&);
    const SeisTrc*	get( int i, int c ) const
			{ return const_cast<SeisMSCProvider*>(this)->get(i,c); }
    const SeisTrc*	get( const BinID& bid ) const
			{ return const_cast<SeisMSCProvider*>(this)->get(bid); }

    int			comparePos(const SeisMSCProvider&) const;
    			//!< 0 = equal; -1 means I need to next(), 1 the other
    int			estimatedNrTraces() const; //!< returns -1 when unknown

    SeisTrcReader&	reader()		{ return rdr_; }
    const SeisTrcReader& reader() const		{ return rdr_; }

protected:

    SeisTrcReader&	rdr_;
    ObjectSet<SeisTrcBuf> tbufs_;
    RowCol		reqstepout_;
    RowCol		desstepout_;
    RowCol		stepoutstep_;
    bool		intofloats_;
    SeisSelData*	seldata_;
    bool		workstarted_;
    enum ReadState	{ NeedStart, ReadOK, ReadAtEnd, ReadErr };
    ReadState		readstate_;

    BufferString	errmsg_;
    mutable int		estnrtrcs_;
    int			reqmininl_;
    int			reqmaxinl_;
    int			reqmincrl_;
    int			reqmaxcrl_;

    int			posidx_;	// Index of new position ready, 
					// equals -1 while buffering.
    int			pivotidx_;	// Next position to be examined.
    
    void		init();
    bool		startWork();
    int			readTrace(SeisTrc&);
    bool 		reqBoxFilled(int,bool) const;
    bool 		doAdvance();
};


#endif
