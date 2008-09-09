/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Satyaki	
 Date:          December 2007
 RCS:           $Id: initearthmodel.cc,v 1.4 2008-09-09 17:22:03 cvsyuancheng Exp $
________________________________________________________________________

-*/


#include "initearthmodel.h"
#include "emfault.h"
#include "emhorizon3d.h"
#include "emhorizon2d.h"
#include "emmarchingcubessurface.h"
#include "emhorizonztransform.h"
#include "emobject.h"
#include "empolygonbody.h"
#include "emsurfaceiodata.h"
#include "emsurfacetr.h"
#include "emsurfaceposprov.h"


void EarthModel::initStdClasses()
{
    EM::Fault::initClass();
    EM::Horizon2D::initClass();
    EM::Horizon3D::initClass();
    EM::HorizonZTransform::initClass();
    EM::MarchingCubesSurface::initClass();
    EM::PolygonBody::initClass();
    Pos::EMSurfaceProvider3D::initClass();
    Pos::EMSurfaceProvider2D::initClass();
}

