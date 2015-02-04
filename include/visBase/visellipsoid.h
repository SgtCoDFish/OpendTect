#ifndef visellipsoid_h
#define visellipsoid_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Yuancheng Liu
 Date:		10-10-2007
 RCS:		$Id$
________________________________________________________________________


-*/

#include "visbasemod.h"
#include "visshape.h"
#include "position.h"

class SoMatrixTransform;
class SoSphere;

namespace Geometry { class Pos; };

namespace visBase
{
class Scene;

/*!\brief

Ellipsoid is a basic ellipsoid that is settable in size. It is generated by 
streching a sphere from a scale vector "getWidth()".

*/


mExpClass(visBase) Ellipsoid : public Shape
{
public:
    static Ellipsoid*	create()
			mCreateDataObj(Ellipsoid);

    void		setCenterPos( const Coord3& );
    Coord3		getCenterPos() const;
    
    void		setWidth( const Coord3& );
    Coord3		getWidth() const; 
    
    void		setDisplayTransformation( const mVisTrans* );
    const mVisTrans*	getDisplayTransformation() const { return transformation_; }

    int			usePar( const IOPar& );
    void		fillPar( IOPar&, TypeSet<int>& ) const;

protected:
    			~Ellipsoid();
    const mVisTrans*	transformation_;
    SoMatrixTransform*	position_;
    
    static const char*	centerposstr();
    static const char*	widthstr();
};

};


#endif
