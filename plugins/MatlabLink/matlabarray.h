#ifndef matlabarray_h
#define matlabarray_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Nanne Hemstra
 Date:		January 2010
 RCS:		$Id$
________________________________________________________________________

-*/

#include "task.h"

// MATLAB header files
#include "matrix.h"

template <class T> class ArrayND;


mClass(MATLABLink) ArrayNDCopier : public ParallelTask
{
public:
			ArrayNDCopier(const ArrayND<float>&);
			~ArrayNDCopier();
   bool			init(bool managemxarr);

   mxArray*		getMxArray()		{ return mxarr_; }

protected:

    od_int64		nrIterations() const	{ return totalnr_; }
    bool		doWork(od_int64 start,od_int64 stop,int thrid);

    mxArray*		mxarr_;
    const ArrayND<float>& arrnd_;
    od_int64		totalnr_;
    bool		managemxarr_;
};


mClass(MATLABLink) mxArrayCopier : public ParallelTask
{
public:
			mxArrayCopier(const mxArray&,ArrayND<float>&);
			~mxArrayCopier();
    bool		init();

protected:

    od_int64		nrIterations() const	{ return totalnr_; }
    bool		doWork(od_int64 start,od_int64 stop,int thrid);

    const mxArray&	mxarr_;
    ArrayND<float>&	arrnd_;
    od_int64		totalnr_;
};

#endif