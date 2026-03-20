import sys
import maya.api.OpenMaya as om
import maya.api.OpenMayaRender as omr
import maya.api.OpenMayaUI as omui
import maya.cmds as cmds


def maya_useNewAPI():
    """
    The presence of this function tells Maya that the plugin produces, and
    expects to be passed, objects created using the Maya Python API 2.0.
    """

    pass


# Class which filters what to render in a scene draw by
# returning the objects in a named set as the object set filter.

# Has the option of what to do as the clear operation.
# Usage can be to clear on first set draw, and not clear
# on subsequent draws.

class ObjectSetSceneRender(omr.MSceneRender):
    def __init__(self, name, setName, clearMask):
        omr.MSceneRender.__init__(self, name)
        self.mSetName = setName
        self.mClearMask = clearMask
        self.mFilterSet = ""
        self.mPanelName = ""
        self.mExcludeFilters = [ ]

    # Return filtered list of items to draw
    def objectSetOverride(self):
        """
        Return filtered list of items to draw
        """

        selList = om.MSelectionList()
        selList.add( self.mSetName )

        obj = selList.getDependNode( 0 )

        selSet = om.MFnSet( obj )
        self.mFilterSet = selSet.getMembers( True )

        return self.mFilterSet

    # Use the M3dView API to disable each registered displayFilter in
    # this MSceneRender's exclusion list.

    # Note that this example does not consider the existing state of
    # the displayFilter (i.e., the state displayed in the
    # modelPanel's "Show > Viewport > Plugins" list of geometry
    # filters) before disabling it.
    def preRender(self):
        if self.mPanelName:
            mView = omui.M3dView.getM3dViewFromModelPanel(self.mPanelName)
            for excl in self.mExcludeFilters:
                mView.setPluginObjectDisplay( excl, False )

    # Use the M3dView API to re-enable each registered displayFilter
    # in this's MSceneRender's exclusion list.
    #
    # Note that this example does not attempt to restore the
    # displayFilters to the states they were in before preRender()
    # disabled them.

    def postRender(self):
        if self.mPanelName:
            mView = omui.M3dView.getM3dViewFromModelPanel(self.mPanelName)
            for excl in self.mExcludeFilters:
                mView.setPluginObjectDisplay( excl, True )

    def clearOperation(self):
        self.mClearOperation.setMask( self.mClearMask )

        self.mClearOperation.setClearColor( [ 0.0, 0.2, 0.8, 1.0 ] )
        self.mClearOperation.setClearColor2( [ 0.5, 0.4, 0.1, 1.0 ] )
        self.mClearOperation.setClearGradient( True )

        return self.mClearOperation

    def pluginDisplayFilterExclusions(self):
        """
        The list of displayFilters to exclude
        """
        return self.mExcludeFilters

    def panelName(self):
        return self.mPanelName

    def setPanelName(self, name):
        self.mPanelName = name


class viewObjectSetOverride(omr.MRenderOverride):
    """Render override which draws three sets of objects in multiple "passes"
    (MSceneRenders) by using a filtered draw for each pass.

    Additionaly, as a demonstration of how to filter out specific types of
    plugin geometry from each MSceneRender "pass":

    "Render Set 1" excludes nodes of type "apiMesh" by using the displayFilter
    named "apiMeshFilter"

    "Render Set 2" excludes nodes of type "footPrint_py" by using the
    displayFilter named "footPrintFilter_py"

    "Render Set 3" excludes nodes of both the "apiMesh" and "footPrint_py"
    types.

    """

    def __init__(self, name ):
        omr.MRenderOverride.__init__(self, name)
        self.mUIName = "(PY) Multi-pass filtered object-set renderer"
        self.mOperation = 0
        self.supportApis = omr.MRenderer.kAllDevices

        render1Name = "Render Set 1"
        render2Name = "Render Set 2"
        render3Name = "Render Set 3"

        set1Name = "set1"
        set2Name = "set2"
        set3Name = "set3"

        presentName = "Present Target"

        # Clear and render set 1 (excluding any apiMesh shapes)
        self.mRenderSet1 = ObjectSetSceneRender( render1Name, set1Name, omr.MClearOperation.kClearAll )
        exclusions1 = self.mRenderSet1.pluginDisplayFilterExclusions()
        exclusions1.append( "apiMeshFilter" )

        # Don't clear and render set 2 (excluding any footPrint_py shapes)
        self.mRenderSet2 = ObjectSetSceneRender( render2Name, set2Name, omr.MClearOperation.kClearNone )
        exclusions2 = self.mRenderSet2.pluginDisplayFilterExclusions()
        exclusions2.append( "footPrintFilter_py" )

        # Don't clear and render set 3 (excluding both apiMesh and footPrint_py shapes)
        self.mRenderSet3 = ObjectSetSceneRender( render3Name, set3Name, omr.MClearOperation.kClearNone )
        exclusions3 = self.mRenderSet3.pluginDisplayFilterExclusions()
        exclusions3.append( "apiMeshFilter" )
        exclusions3.append( "footPrintFilter_py" )

        # Present results
        self.mPresentTarget = omr.MPresentTarget( presentName )

    def setup(self, destination ):
        self.mPanelName = destination
        # Update the attributes for the custom operations
        self.mRenderSet1.setPanelName( self.mPanelName )
        self.mRenderSet2.setPanelName( self.mPanelName )
        self.mRenderSet3.setPanelName( self.mPanelName )

    def __del__(self):
        self.mRenderSet1 = None
        self.mRenderSet2 = None
        self.mRenderSet3 = None
        self.mPresentTarget = None

    def supportedDrawAPIs(self):
        # this plugin supports both GL and DX
        return self.supportApis

    def startOperationIterator(self):
        self.mOperation = 0
        return True

    def renderOperation(self):
        if self.mOperation == 0:
            return self.mRenderSet1
        elif self.mOperation == 1:
            return self.mRenderSet2
        elif self.mOperation == 2 :
            return self.mRenderSet3
        elif self.mOperation == 3 :
            return self.mPresentTarget
        else:
            return None

    def nextRenderOperation(self):
        self.mOperation += 1
        return self.mOperation < 4

    def uiName(self):
        # UI name to appear as renderer
        return self.mUIName

viewObjectSetOverrideInstance = None

def initializePlugin(obj):
    """
    Register an override
    """
    try:
        global viewObjectSetOverrideInstance
        viewObjectSetOverrideInstance = viewObjectSetOverride("py_viewObjectSetOverride")
        omr.MRenderer.registerOverride( viewObjectSetOverrideInstance )
    except:
        sys.stderr.write("registerOverride")
        raise

    try:
        cmds.pluginDisplayFilter("apiMeshFilter", register=True, label="API Mesh Shape", classification="drawdb/geometry/apiMesh")
    except:
        sys.stderr.write("Failed to register displayFilter apiMeshFilter\n")
        raise

    try:
        cmds.pluginDisplayFilter("footPrintFilter_py", register=True, label="Footprint (Python)", classification="drawdb/geometry/footPrint_py")
    except:
        sys.stderr.write("Failed to register displayFilter footPrintFilter_py\n")
        raise


def uninitializePlugin(obj):
    """
    Deregister an override
    """
    try:
        global viewObjectSetOverrideInstance
        if not viewObjectSetOverrideInstance is None:
            omr.MRenderer.deregisterOverride( viewObjectSetOverrideInstance )
            viewObjectSetOverrideInstance = None
    except Exception as e:
        print(e)
        sys.stderr.write("deregisterOverride")

    try:
        cmds.pluginDisplayFilter("apiMeshFilter", deregister = True)
    except Exception as e:
        print(e)
        sys.stderr.write("Failed to deregister displayFilter apiMeshFilter\n")

    try:
        cmds.pluginDisplayFilter("footPrintFilter_py", deregister = True)
    except Exception as e:
        print(e)
        sys.stderr.write("Failed to deregister displayFilter footPrintFilter_py\n")
