#ifndef emsurface_h
#define emsurface_h

/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Kristofer Tingdahl
 Date:		4-11-2002
 RCS:		$Id: emsurface.h,v 1.67 2008-09-09 17:22:02 cvsyuancheng Exp $
________________________________________________________________________


-*/

#include "emobject.h"
#include "position.h"

class BinID;
class IOObj;
class RowCol;
template <class T, class AT> class TopList;

namespace EM
{
class EMManager;

class EdgeLineManager;
class SurfaceAuxData;
class SurfaceGeometry;

/*!\brief Base class for surfaces
  This is the base class for surfaces like horizons and faults. A surface is
  made up by one or more segments or patches, so they can overlap. 
*/


class Surface : public EMObject
{
public:
    int				nrSections() const;
    EM::SectionID		sectionID(int) const;
    BufferString		sectionName(const SectionID&) const;
    bool			canSetSectionName() const;
    bool			setSectionName(const SectionID&,const char*,
					       bool addtohistory);
    bool			removeSection(SectionID,bool hist);

    virtual void		removeAll();

    bool			isAtEdge(const EM::PosID&) const;
    bool			isLoaded() const;
    virtual Executor*		saver();
    virtual Executor*		saver(IOObj*) { return 0;}
    virtual Executor*		loader();

    const char*			dbInfo() const		 { return dbinfo.buf();}
    void			setDBInfo(const char* s) { dbinfo = s; }

    virtual bool		usePar(const IOPar&);
    virtual void		fillPar(IOPar&) const;

    virtual EMObjectIterator*	createIterator(const SectionID&,
	    				       const CubeSampling* =0) const;

    bool			enableGeometryChecks(bool);
    bool			isGeometryChecksEnabled() const;

    virtual SurfaceGeometry&		geometry()			= 0;
    virtual const SurfaceGeometry&	geometry() const;

    static BufferString		getParFileName(const IOObj&);
    static BufferString		getSetupFileName(const IOObj&);

protected:
    friend class		SurfaceGeometry;
    friend class		SurfaceAuxData;
    friend class		EMObject;

    				Surface(EMManager&);
    				~Surface();
    virtual Geometry::Element*	sectionGeometryInternal(const SectionID&);

    BufferString		dbinfo;
};


}; // Namespace


#endif
