//-
// Copyright 2019 Autodesk, Inc.  All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license agreement
// provided at the time of installation or download, or which otherwise
// accompanies this software in either electronic or hard copy form.
//+
#include <maya/MFnPlugin.h>
#include <maya/M3dView.h>
#include <maya/MViewport2Renderer.h>
#include <maya/MSelectionList.h>
#include <maya/MFnSet.h>
#include <maya/MApiVersion.h>

#include <memory>

static std::unique_ptr < MHWRender::MRenderOverride> gsRenderOverride;

//
// Class which filters what to render in a scene draw by
// returning the objects in a named set as the object set filter.
//
// Has the option of what to do as the clear operation.
// Usage can be to clear on first set draw, and not clear
// on subsequent draws.
//
class ObjectSetSceneRender : public MHWRender::MSceneRender
{
public:
	ObjectSetSceneRender( const MString& name, const MString setName, unsigned int clearMask ) 
		: MHWRender::MSceneRender( name )
		, mSetName( setName ) 
		, mClearMask( clearMask )
	{}

	// Return filtered list of items to draw
	const MSelectionList* objectSetOverride() override
	{
		// Get members of the specified object set.
		MSelectionList list; 
		list.add( mSetName );

		MObject obj; 
		list.getDependNode( 0, obj );

		MFnSet set( obj ); 
		if (!set.getMembers( mFilterSet, true ))
		{
			mFilterSet.clear();
		}

		return &mFilterSet;
	}

	// Use the M3dView API to disable each registered displayFilter in
	// this MSceneRender's exclusion list.
	// 
	// Note that this example does not consider the existing state of
	// the displayFilter (i.e., the state displayed in the
	// modelPanel's "Show > Viewport > Plugins" list of geometry
	// filters) before disabling it.
	//
	void preRender() override
	{
		M3dView mView;
		if( mPanelName.isEmpty() 
		||( MStatus::kSuccess != M3dView::getM3dViewFromModelPanel(mPanelName, mView ) ) )
		{
			printf( "ObjectSetSceneRender %s: could not find M3dView from panel name \"%s\"\n", name().asChar(), mPanelName.asChar() );
			return;
		}

		for( auto excl : mExcludeFilters ) {
			mView.setPluginObjectDisplay( excl.asChar(), false );
		}
	}

	// Use the M3dView API to re-enable each registered displayFilter
	// in this's MSceneRender's exclusion list.
	// 
	// Note that this example does not attempt to restore the
	// displayFilters to the states they were in before preRender()
	// disabled them.
	// 
	void postRender() override
	{
		M3dView mView;
		if( mPanelName.isEmpty()
		||( MStatus::kSuccess != M3dView::getM3dViewFromModelPanel(mPanelName, mView ) ) )
		{
			printf( "ObjectSetSceneRender %s: could not find M3dView from panel name \"%s\"\n", name().asChar(), mPanelName.asChar() );
			return;
		}

		for( auto excl : mExcludeFilters ) {
			mView.setPluginObjectDisplay( excl.asChar(), true );
		}
	}

	// Return clear operation to perform
	MHWRender::MClearOperation & clearOperation() override
	{
		mClearOperation.setMask( mClearMask );
		return mClearOperation;
	}

	// The list of names of displayFilters to exclude
	MStringArray &pluginDisplayFilterExclusions()
	{
		return mExcludeFilters;
	}

	void setPanelName( const MString &name )
	{
		mPanelName = name;
	}

protected:
	MSelectionList mFilterSet;
	MString mSetName;
	unsigned int mClearMask;
	MStringArray mExcludeFilters;
	MString mPanelName;
};

// Render override which draws 3 sets of objects in multiple "passes"
// (multiple MSceneRenders) by using a filtered draw for each pass.
// 
// Additionaly, as a demonstration of how to filter out specific types
// of plugin geometry from each MSceneRender "pass":
// 
//	"Render Set 1" excludes nodes of type "apiMesh" (provided by the
// 	devkit "apiMeshShape" C++ plug-in).
// 
//	"Render Set 2" excludes nodes of type "footPrint_py" (provided by
// 	the devkit "py2FootPrintNode.py" Python plug-in.
// 
//	"Render Set 3" excludes nodes of both the "apiMesh" and
//	"footPrint_py" types.
// 
class viewObjectSetOverride : public MHWRender::MRenderOverride
{
public:
	viewObjectSetOverride( const MString& name ) 
		: MHWRender::MRenderOverride( name ) 
		, mUIName("Multi-pass filtered object-set renderer")
		, mOperation(0)
	{
		const MString render1Name("Render Set 1");
		const MString render2Name("Render Set 2");
		const MString render3Name("Render Set 3");
		const MString set1Name("set1");
		const MString set2Name("set2");
		const MString set3Name("set3");
		const MString presentName("Present Target");

		// Clear + render set 1
		mRenderSet1 = new ObjectSetSceneRender( render1Name, set1Name,  (unsigned int)MHWRender::MClearOperation::kClearAll );
		MStringArray &exclusions1 = mRenderSet1->pluginDisplayFilterExclusions();
		exclusions1.append( "apiMeshShape_filter" );

		// Don't clear and render set 2
		mRenderSet2 = new ObjectSetSceneRender( render2Name, set2Name,  (unsigned int)MHWRender::MClearOperation::kClearNone );
		MStringArray &exclusions2 = mRenderSet2->pluginDisplayFilterExclusions();
		exclusions2.append( "footPrintFilter_py" );

		// Don't clear and render set 3
		mRenderSet3 = new ObjectSetSceneRender( render3Name, set3Name,  (unsigned int)MHWRender::MClearOperation::kClearNone );
		MStringArray &exclusions3 = mRenderSet3->pluginDisplayFilterExclusions();
		exclusions3.append( "apiMeshShape_filter" );
		exclusions3.append( "footPrintFilter_py" );

		// Present results
		mPresentTarget = new MHWRender::MPresentTarget( presentName ); 
	}

	~viewObjectSetOverride() override
	{
		delete mRenderSet1; mRenderSet1 = NULL;
		delete mRenderSet2; mRenderSet2 = NULL;
		delete mRenderSet3; mRenderSet3 = NULL;
		delete mPresentTarget; mPresentTarget = NULL;
	}

	MHWRender::DrawAPI supportedDrawAPIs() const override
	{
		// this plugin supports both GL and DX
		return (MHWRender::kOpenGL | MHWRender::kOpenGLCoreProfile | MHWRender::kDirectX11);
	}

	bool startOperationIterator() override
	{
		mOperation = 0; 
		return true;
	}

	MHWRender::MRenderOperation* renderOperation() override
	{
		switch( mOperation )
		{
		case 0 : return mRenderSet1;
		case 1 : return mRenderSet2;
		case 2 : return mRenderSet3;
		case 3 : return mPresentTarget;
		}
		return NULL;
	}

	bool nextRenderOperation() override
	{
		mOperation++; 
		return mOperation < 4 ? true : false;
	}

	MStatus setup( const MString & destination ) override
	{
		if( mRenderSet1 ) mRenderSet1->setPanelName( destination );
		if( mRenderSet2 ) mRenderSet2->setPanelName( destination );
		if( mRenderSet3 ) mRenderSet3->setPanelName( destination );

		return MStatus::kSuccess;
	}
	
	// UI name to appear as renderer 
	MString uiName() const override
	{
		return mUIName;
	}

protected:
	ObjectSetSceneRender*		mRenderSet1;
	ObjectSetSceneRender*		mRenderSet2;
	ObjectSetSceneRender*		mRenderSet3;
	MHWRender::MPresentTarget*	mPresentTarget;
	int							mOperation;
	MString						mUIName;
};


//
// Plugin registration
//
MStatus 
initializePlugin( MObject obj )
{
    MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer(); 
	MStatus status = MStatus::kFailure;
	if (renderer)
	{
		gsRenderOverride.reset(new viewObjectSetOverride("viewObjectSetOverride"));
		status = renderer->registerOverride(gsRenderOverride.get());
	}

	MFnPlugin plugin( obj, PLUGIN_COMPANY, "3.0", "Any");

	// Registering displayFilters allows them to appear in the
	// modelPanel's "Show > Viewport > Plugins" list
	//
	status = plugin.registerDisplayFilter( "apiMeshShape_filter", 
										   "API Mesh Shape",
										   "drawdb/geometry/apiMesh" );
	status = plugin.registerDisplayFilter( "footPrintFilter_py",
										   "Footprint (Python)",
										   "drawdb/geometry/footPrint_py" );

    return status;
}

//
// Plugin deregistration
//
MStatus 
uninitializePlugin( MObject obj )
{
    MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer(); 
	MStatus status = MStatus::kFailure;
	if (renderer)
	{
		status = renderer->deregisterOverride(gsRenderOverride.get());
		gsRenderOverride = nullptr;
	}

	MFnPlugin plugin( obj, PLUGIN_COMPANY, "3.0", "Any");

	status = plugin.deregisterDisplayFilter( "apiMeshShape_filter" );
	status = plugin.deregisterDisplayFilter( "footPrintFilter_py" );

    return status;
}
