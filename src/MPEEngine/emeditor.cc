/*
___________________________________________________________________

 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : K. Tingdahl
 * DATE     : Nov 2004
___________________________________________________________________

-*/


#include "emeditor.h"

#include "undo.h"
#include "emmanager.h"
#include "emhorizon3d.h"
#include "emsurfacegeometry.h"
#include "emtracker.h"
#include "geeditor.h"
#include "mpeengine.h"
#include "survinfo.h"
#include "rcollinebuilder.h"

namespace MPE
{

mImplFactory1Param( ObjectEditor, EM::EMObject&, EditorFactory );


ObjectEditor::ObjectEditor( EM::EMObject& emobj )
    : emobject(emobj)
    , editpositionchange(this)
    , movingnode( -1,-1,-1 )
    , snapafteredit( true )
    , nrusers(0)
{
    emobject.ref();
}


ObjectEditor::~ObjectEditor()
{
    CallBack::removeFromMainThread( this );

    emobject.unRef();
    deepErase( geeditors );
    sections.erase();
}


static bool nodecloningenabled = false;
static int nodeclonecountdown = -1;

void ObjectEditor::enableNodeCloning( bool yn )
{ nodecloningenabled = yn; }


void ObjectEditor::startEdit(const EM::PosID& pid)
{
    changedpids.erase();

    alongmovingnodes.erase();
    alongmovingnodesfactors.erase();
    alongmovingnodesstart.erase();
    snapafterthisedit = false;

    if ( pid.objectID()!=emobject.id() )
    {
	movingnode = EM::PosID(-1,-1,-1);
	return;
    }

    movingnode = pid;

    startpos = getPosition(movingnode);
    if ( !startpos.isDefined() )
    {
	pErrMsg( "Editnode is undefined");
	movingnode = EM::PosID(-1,-1,-1);
	return;
    }

    getAlongMovingNodes( pid, alongmovingnodes, &alongmovingnodesfactors );
    snapafterthisedit = snapafteredit && canSnapAfterEdit(pid);

    alongmovingnodesstart.erase();
    for ( int idx=0; idx<alongmovingnodes.size(); idx++ )
	alongmovingnodesstart += emobject.getPos(alongmovingnodes[idx]);

    nodeclonecountdown = nodecloningenabled ? 3 : -1;
}


bool ObjectEditor::setPosition(const Coord3& np)
{
    if ( movingnode.objectID()==-1 )
    {
	pErrMsg("Moving unknown node");
	return false;
    }

    if ( !np.isDefined() )
    {
	pErrMsg("You cannot set the editnode to undefined");
	return false;
    }

    const Coord3 diff = np-startpos;
    if ( !setPosition( movingnode, np ) )
	return false;

    for ( int idx=0; idx<alongmovingnodes.size(); idx++ )
    {
	const Coord3 newpos = alongmovingnodesstart[idx] +
		    alongmovingnodesfactors[idx]*diff;
	if ( !setPosition( alongmovingnodes[idx], newpos ) )
	    return false;
    }

    nodeclonecountdown--;
    if ( !nodeclonecountdown )
	mMainThreadCall( ObjectEditor::cloneMovingNode );

    return true;
}


void ObjectEditor::finishEdit()
{
    if ( changedpids.isEmpty() )
	return;

    if ( snapafterthisedit )
    {
//	const int trackeridx = MPE::engine().getTrackerByObject(emobject.id());
//	EMTracker* tracker = MPE::engine().getTracker(trackeridx);
//	tracker->snapPositions(alongmovingnodes);
    }

    EM::EMM().undo().setUserInteractionEnd(
	    EM::EMM().undo().currentEventID() );
}


bool ObjectEditor::canSnapAfterEdit(const EM::PosID& pid) const
{
    if ( pid.objectID()!=emobject.id() ||
	 MPE::engine().getTrackerByObject(emobject.id())==-1 )
	return false;

    const TrcKeyZSampling& trackvolume = MPE::engine().activeVolume();

    TypeSet<EM::PosID> nodes;
    getAlongMovingNodes( pid, nodes, 0 );
    for ( int idx=0; idx<nodes.size(); idx++ )
    {
	const Coord3 pos = emobject.getPos(nodes[idx]);
	const BinID bid = SI().transform(pos);

	if ( !trackvolume.hsamp_.includes( bid ) )
	    return false;
	if ( !trackvolume.zsamp_.includes( pos.z,false ) )
	    return false;
    }

    return true;
}


bool ObjectEditor::getSnapAfterEdit() const { return snapafteredit; }


void ObjectEditor::setSnapAfterEdit(bool yn) { snapafteredit=yn; }


void ObjectEditor::getEditIDs( TypeSet<EM::PosID>& ids ) const
{
    ids.erase();

    for ( int idx=0; idx<emobject.nrSections(); idx++ )
    {
	const EM::SectionID sectionid =  emobject.sectionID(idx);
	const Geometry::ElementEditor* ge = getEditor( sectionid );
	if ( !ge ) continue;

	TypeSet<GeomPosID> gepids;
	ge->getEditIDs( gepids );
	for ( int idy=0; idy<gepids.size(); idy++ )
	    ids += EM::PosID( emobject.id(), sectionid, gepids[idy] );
    }
}


bool ObjectEditor::addEditID( const EM::PosID& ) { return false; }


bool ObjectEditor::removeEditID( const EM::PosID& ) { return false; }


Coord3 ObjectEditor::getPosition( const EM::PosID& pid ) const
{ return emobject.getPos( pid ); }


bool ObjectEditor::setPosition( const EM::PosID& pid,  const Coord3& np )
{
    const bool addtoundo = changedpids.indexOf(pid) == -1;
    if ( addtoundo )
	changedpids += pid;

    return emobject.setPos( pid, np, addtoundo );
}

#define mMayFunction( func ) \
bool ObjectEditor::func( const EM::PosID& pid ) const \
{ \
    const Geometry::ElementEditor* ge = getEditor( pid.sectionID() ); \
    if ( !ge ) return false; \
 \
    return ge->func( pid.subID() ); \
}


#define mGetFunction( func ) \
Coord3 ObjectEditor::func( const EM::PosID& pid ) const\
{\
    const Geometry::ElementEditor* ge = getEditor( pid.sectionID() );\
    if ( !ge ) return Coord3::udf();\
\
    return ge->func( pid.subID() );\
}


mMayFunction( mayTranslate1D )
mGetFunction( translation1DDirection )

mMayFunction( mayTranslate2D )
mGetFunction( translation2DNormal )

mMayFunction( mayTranslate3D )

mMayFunction( maySetNormal )
mGetFunction( getNormal )

mMayFunction( maySetDirection )
mGetFunction( getDirectionPlaneNormal )
mGetFunction( getDirection )


Geometry::ElementEditor* ObjectEditor::getEditor( const EM::SectionID& sid )
{
    const int idx = sections.indexOf(sid);
    if ( idx!=-1 ) return geeditors[idx];

    Geometry::ElementEditor* geeditor = createEditor( sid );
    if ( geeditor )
    {
	geeditors += geeditor;
	sections += sid;
	geeditor->editpositionchange.notify(
		mCB(this,ObjectEditor,editPosChangeTrigger));
    }

    return geeditor;
}


const Geometry::ElementEditor* ObjectEditor::getEditor(
	const EM::SectionID& sid ) const
{ return const_cast<ObjectEditor*>(this)->getEditor(sid); }



void ObjectEditor::editPosChangeTrigger(CallBacker*)
{ editpositionchange.trigger(); }


void ObjectEditor::emSectionChange(CallBacker* cb)
{
    mCBCapsuleUnpack(const EM::EMObjectCallbackData&,cbdata,cb);
    if ( cbdata.event!=EM::EMObjectCallbackData::SectionChange )
	return;

    const EM::SectionID sectionid = cbdata.pid0.sectionID();
    const int editoridx = sections.indexOf(sectionid);

    const Geometry::Element* ge =
	const_cast<const EM::EMObject*>(&emobject)->sectionGeometry(sectionid);

    if ( !ge && editoridx!=-1 )
    {
	delete geeditors.removeSingle(editoridx);
	editpositionchange.trigger();
    }
    else if ( ge && editoridx==-1 )
    {
	Geometry::ElementEditor* geeditor = createEditor( sectionid );
	if ( geeditor )
	{
	    geeditors += geeditor;
	    sections += sectionid;
	    geeditor->editpositionchange.notify(
		    mCB(this,ObjectEditor,editPosChangeTrigger));
	}
    }
}


void ObjectEditor::getAlongMovingNodes( const EM::PosID&,
					TypeSet<EM::PosID>& nodes,
					TypeSet<float>* factors ) const
{
    nodes.erase();
    if ( factors ) factors->erase();
}

} // namespace MPE
