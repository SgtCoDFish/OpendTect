#ifndef uiodviewer2dmgr_h
#define uiodviewer2dmgr_h

/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Umesh Sinha
 Date:		Apr 2010
________________________________________________________________________

-*/

#include "uiodmainmod.h"
#include "callback.h"
#include "datapack.h"
#include "emposid.h"
#include "geom2dintersections.h"
#include "uigeom.h"
#include "uigraphicsviewbase.h"
#include "uiodapplmgr.h"
#include "uiodviewer2dposgrp.h"

class uiFlatViewer;
class uiODViewer2D;
class uiTreeFactorySet;
class MouseEventHandler;
class TrcKeyZSampling;
class Vw2DDataObject;
namespace Attrib	{ class SelSpec; }

mExpClass(uiODMain) uiODViewer2DMgr : public CallBacker
{ mODTextTranslationClass(uiODViewer2DMgr);
public:

    struct SelectedAuxAnnot
    {
				SelectedAuxAnnot(int auxposidx=-1,
					bool isx1=true,bool selected=false)
				    : auxposidx_(auxposidx)
				    , isx1_(isx1)
				    , oldauxpos_(mUdf(float))
				    , isselected_(selected)	{}
	int			auxposidx_;
	bool			isx1_;
	bool			isselected_;
	float			oldauxpos_;
	bool			isValid() const		{ return auxposidx_>=0;}
    };

    uiODViewer2D*		getParent2DViewer(int vw2dobjid);
    uiODViewer2D*		find2DViewer(int id,bool byvisid);
    uiODViewer2D*		find2DViewer(const MouseEventHandler&);
    uiODViewer2D*		find2DViewer(const Pos::GeomID&);
    uiODViewer2D*		find2DViewer(const TrcKeyZSampling&);

    int				displayIn2DViewer(
	    				Viewer2DPosDataSel&,bool wva,
				        float initialx1pospercm=mUdf(float),
				        float initialx2pospercm=mUdf(float));
    void			displayIn2DViewer(int visid,int attribid,
						  bool wva);
    void			remove2DViewer(int id,bool byvisid);

    uiTreeFactorySet*		treeItemFactorySet2D()	{ return tifs2d_; }
    uiTreeFactorySet*		treeItemFactorySet3D()	{ return tifs3d_; }
    bool			isItemPresent(const uiTreeItem*) const;

    //3D Horizons
    void			getHor3DVwr2DIDs( EM::ObjectID emid,
						  TypeSet<int>& vw2dids) const;
    void			removeHorizon3D(EM::ObjectID emid);
    void			addHorizon3Ds(const TypeSet<EM::ObjectID>&);
    void			addNewTrackingHorizon3D(EM::ObjectID mid);
    void			addNewTrackingHorizon3D(EM::ObjectID mid,
	    						int sceneid);
    void			getLoadedHorizon3Ds(
					TypeSet<EM::ObjectID>&) const;
    // 2D Horizons
    void			getHor2DVwr2DIDs( EM::ObjectID emid,
						  TypeSet<int>& vw2dids) const;
    void			removeHorizon2D(EM::ObjectID emid);
    void			getLoadedHorizon2Ds(
					TypeSet<EM::ObjectID>&) const;
    void			addHorizon2Ds(const TypeSet<EM::ObjectID>&);
    void			addNewTrackingHorizon2D(EM::ObjectID mid);
    void			addNewTrackingHorizon2D(EM::ObjectID mid,
	    						int sceneid);

    //Faults
    void			removeFault(EM::ObjectID emid);
    void			addFaults(const TypeSet<EM::ObjectID>&);
    void			addNewTempFault(EM::ObjectID mid);
    void			addNewTempFault(EM::ObjectID mid,int sceneid);
    void			getLoadedFaults( TypeSet<EM::ObjectID>&) const;

    //FaultStickSet
    void			getFaultSSVwr2DIDs( EM::ObjectID emid,
						    TypeSet<int>& vw2ids) const;
    void			removeFaultSS(EM::ObjectID emid);
    void			addFaultSSs(const TypeSet<EM::ObjectID>&);
    void			addNewTempFaultSS(EM::ObjectID mid);
    void			addNewTempFaultSS(EM::ObjectID mid,int sceneid);
    void			getLoadedFaultSSs(TypeSet<EM::ObjectID>&) const;

    void			getVwr2DObjIDs(TypeSet<int>& vw2ids) const;
    //FaultStickSet2D
    void			getFaultSS2DVwr2DIDs( EM::ObjectID emid,
						    TypeSet<int>& vw2ids) const;
    void			removeFaultSS2D(EM::ObjectID emid);
    void			addFaultSS2Ds(const TypeSet<EM::ObjectID>&);
    void			addNewTempFaultSS2D(EM::ObjectID mid);
    void			addNewTempFaultSS2D(EM::ObjectID mid,
	    					    int sceneid);
    void			getLoadedFaultSS2Ds(
	   				 TypeSet<EM::ObjectID>&) const;
    
    //PickSets
    void			getPickSetVwr2DIDs(const MultiID& mid,
						   TypeSet<int>& vw2ids) const;
    void			removePickSet(const MultiID&);
    void			getLoadedPickSets(TypeSet<MultiID>&) const;
    void			addPickSets(const TypeSet<MultiID>&);


    static int			cNameColumn()		{ return 0; }
    static int			cColorColumn()		{ return 1; }

    static const char*		sKeyVisID()		{ return "VisID"; }
    static const char*		sKeyAttrID()		{ return "Attrib ID"; }
    static const char*		sKeyWVA()		{ return "WVA"; }

    CNotifier<uiODViewer2DMgr,int>  vw2dObjAdded;
    CNotifier<uiODViewer2DMgr,int>  vw2dObjToBeRemoved;

protected:

				uiODViewer2DMgr(uiODMain*);
				~uiODViewer2DMgr();

    uiODViewer2D&		addViewer2D(int visid);
    ObjectSet<uiODViewer2D>     viewers2d_;
    Line2DInterSectionSet*	l2dintersections_;
    SelectedAuxAnnot		selauxannot_;
    TypeSet<Pos::GeomID>	geom2dids_;

    uiTreeFactorySet*		tifs2d_;
    uiTreeFactorySet*		tifs3d_;

    uiODMain&			appl_;

    inline uiODApplMgr&         applMgr()     { return appl_.applMgr(); }
    inline uiVisPartServer&     visServ()     { return *applMgr().visServer(); }

    void			surveyChangedCB(CallBacker*);
    void			viewObjAdded(CallBacker*);
    void			viewObjToBeRemoved(CallBacker*);
    void			viewWinClosedCB(CallBacker*);
    void			vw2DPosChangedCB(CallBacker*);
    void			homeZoomChangedCB(CallBacker*);
    void			mouseClickCB(CallBacker*);
    void			mouseClickedCB(CallBacker*);
    void			mouseMoveCB(CallBacker*);

    void			create2DViewer(const uiODViewer2D& curvwr2d,
					       const TrcKeyZSampling& newtkzs,
					       const uiWorldPoint& initcentr);
				/*!< \param newtkzs is the new TrcKeyZSampling
				for which a new uiODViewer2D will be created.
				\param curvwr2d is the current 2D Viewer of
				which the newly created 2D Viewer will inherit
				Attrib::SelSpec and other display properties.*/
    void			attachNotifiersAndSetAuxData(uiODViewer2D*);
    Line2DInterSection::Point	intersectingLineID(const uiODViewer2D*,
						   float pos) const;
    int				intersection2DIdx(Pos::GeomID) const;
    void			getVWR2DDataGeomIDs(const uiODViewer2D*,
	    					TypeSet<Pos::GeomID>&) const;
    void			reCalc2DIntersetionIfNeeded(Pos::GeomID);
    void			setAllIntersectionPositions();
    void			setVWR2DIntersectionPositions(uiODViewer2D*);
    void			handleLeftClick(uiODViewer2D*);
    void			setAuxAnnotLineStyles(uiFlatViewer&,bool forx1);
    void			setupHorizon3Ds(uiODViewer2D*);
    void			setupHorizon2Ds(uiODViewer2D*);
    void			setupFaults(uiODViewer2D*);
    void			setupFaultSSs(uiODViewer2D*);
    void			setupPickSets(uiODViewer2D*);
    void			reSetPrevDragMode(uiODViewer2D*);

    void			fillPar(IOPar&) const;
    void			usePar(const IOPar&);

    friend class                uiODMain;
};

#endif
