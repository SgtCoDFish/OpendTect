/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Kristofer Tingdahl
 Date:          December 2007
 RCS:           $Id: initvolumeprocessing.cc,v 1.5 2008-11-19 15:01:57 cvskris Exp $
________________________________________________________________________

-*/

#include "initvolumeprocessing.h"

#include "velocitygridder.h"
#include "volprochorinterfiller.h"
#include "volprocattrib.h"
#include "volprocsmoother.h"
#include "volprocmarchingcubes.h"
#include "volprocvolreader.h"


void VolumeProcessing::initStdClasses()
{
    VolProc::HorInterFiller::initClass();
    VolProc::Smoother::initClass();
    VolProc::ExternalAttribCalculator::initClass();
    VolProc::MarchingCubes::initClass();
    VolProc::VelGriddingStep::initClass();
    VolProc::VolumeReader::initClass();
}


