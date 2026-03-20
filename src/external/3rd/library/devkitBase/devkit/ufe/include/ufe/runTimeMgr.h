#ifndef _runTimeMgr
#define _runTimeMgr
// ===========================================================================
// Copyright 2022 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
// ===========================================================================

#include "attributesHandler.h"
#include "connectionHandler.h"
#include "colorManagementHandler.h"
#include "contextOpsHandler.h"
#include "hierarchyHandler.h"
#include "object3dHandler.h"
#include "materialHandler.h"
#include "sceneItemOpsHandler.h"
#include "sceneSegmentHandler.h"
#include "transform3dHandler.h"
#include "uiInfoHandler.h"
#include "cameraHandler.h"
#include "lightHandler.h"
#include "light2Handler.h"
#include "pathMappingHandler.h"
#include "nodeDefHandler.h"
#include "uiNodeGraphNodeHandler.h"
#include "batchOpsHandler.h"
#include "clipboardHandler.h"
#include "handlerInterface.h"
#include "rtid.h"

// Can't forward declare std::list or std::string.
#include <list>
#include <string>

UFE_NS_DEF {

//! \brief Singleton class to manage UFE runtimes.
/*!

  This singleton class is where UFE runtimes register handlers for different
  interfaces.  Clients that wish to use interfaces ask the runtime manager to
  return the appropriate handler for that interface, for the appropriate
  runtime.
*/

class UFE_SDK_DECL RunTimeMgr
{
public:

    //! \return The runtime manager singleton instance.
    static RunTimeMgr& instance();

    //! Cannot copy the runtime manager singleton.
    RunTimeMgr(const RunTimeMgr&) = delete;
    //! Cannot assign the runtime manager singleton.
    RunTimeMgr& operator=(const RunTimeMgr&) = delete;


    /*! Runtime handlers used to register a runtime.

        All the Handler::Ptr types are implemented with std::shared_ptr,
        so they all have a default value of nullptr.
    */
    struct UFE_SDK_DECL Handlers {
        //! the Hierarchy interface factory.
        HierarchyHandler::Ptr    hierarchyHandler;
        //! the Transform3d interface factory.
        Transform3dHandler::Ptr  transform3dHandler;
        //! the SceneItemOps interface factory.
        SceneItemOpsHandler::Ptr sceneItemOpsHandler;
        //! the Attributes interface factory.
        AttributesHandler::Ptr   attributesHandler;
        //! the Object3d interface factory.
        Object3dHandler::Ptr     object3dHandler;
        //! the Material interface factory.
        MaterialHandler::Ptr     materialHandler;
        //! the ContextOps interface factory.
        ContextOpsHandler::Ptr   contextOpsHandler;
        //! the UIInfo handler.
        UIInfoHandler::Ptr       uiInfoHandler;
        //! the camera handler.
        CameraHandler::Ptr       cameraHandler;
        //! the light handler.
        LightHandler::Ptr        lightHandler;
        //! the path mapping handler.
        PathMappingHandler::Ptr  pathMappingHandler;
        //! the node definition handler.
        NodeDefHandler::Ptr      nodeDefHandler;
        //! the scene segment handler
        SceneSegmentHandler::Ptr sceneSegmentHandler;
        //! the Connections interface factory.
        ConnectionHandler::Ptr   connectionHandler;
        //! the UINodeGraphNode interface factory.
        UINodeGraphNodeHandler::Ptr uiNodeGraphNodeHandler;
        //! the BatchOps handler.
        BatchOpsHandler::Ptr     batchOpsHandler;
        //! the Clipboard handler.
        ClipboardHandler::Ptr   clipboardHandler;
    };

    /*! Register a runtime and its handlers to create interfaces.
        register is a reserved C++ keyword, using register_ instead.
      \sa unregister
      \param rtName              the name of the runtime for the handlers.
      \param handlers            the handlers for each interface to be registered.
      \exception InvalidRunTimeName Thrown if argument runtime name already exists.
      \return The allocated runtime ID.
     */
    Rtid register_(
        const std::string&              rtName,
        const Handlers&                 handlers
    );
    /*!
        Unregister the given runtime ID.
        \param rtId   the ID of the runtime for the handlers.
        \return True if the runtime was unregistered.
    */
    bool unregister(const Rtid& rtId);

    //! \exception InvalidRunTimeId Thrown if argument runtime ID does not exist.
    //! \return The runtime name corresponding to the argument ID.
    std::string getName(const Rtid& rtId) const;

    //! \exception InvalidRunTimeName Thrown if argument runtime name does not exist.
    //! \return The runtime ID corresponding to the argument name.
    Rtid getId(const std::string& rtName) const;

    //! \return True if the runtime ID exists.
    bool hasId(const Rtid& rtId) const;

    /*!
        Set a HierarchyHandler to a given runtime ID
        \param rtId the ID of the runtime for the handler.
        \param hierarchyHandler the Hierarchy interface factory.
    */
    void setHierarchyHandler(
        const Rtid& rtId, const HierarchyHandler::Ptr& hierarchyHandler
    );

    /*!
        Set a Transform3dHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param transform3dHandler the Transform3d interface factory.
    */
    void setTransform3dHandler(
        const Rtid& rtId, const Transform3dHandler::Ptr& transform3dHandler
    );

    /*!
        Set a SceneItemOpsHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param sceneItemOpsHandler the SceneItemOps interface factory.
    */
    void setSceneItemOpsHandler(
        const Rtid& rtId, const SceneItemOpsHandler::Ptr& sceneItemOpsHandler
    );

    /*!
        Set an AttributesHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param attributesHandler the Attributes interface factory.
    */
    void setAttributesHandler(
        const Rtid& rtId, const AttributesHandler::Ptr& attributesHandler
    );

    /*!
        Set a ConnectionHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param connectionHandler the Connections interface factory.
    */
    void setConnectionHandler(
        const Rtid& rtId, const ConnectionHandler::Ptr& connectionHandler
    );

    /*!
        Set a UINodeGraphNodeHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param uiNodeGraphNodeHandler the UINodeGraphNode interface factory.
    */
    void setUINodeGraphNodeHandler(
        const Rtid& rtId, const UINodeGraphNodeHandler::Ptr& uiNodeGraphNodeHandler
    );

    /*!
        Set an Object3dHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param object3dHandler the Object3d interface factory.
    */
    void setObject3dHandler(
        const Rtid& rtId, const Object3dHandler::Ptr& object3dHandler
    );

    /*!
        Set a MaterialHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param materialHandler the Material interface factory.
    */
    void setMaterialHandler(
        const Rtid& rtId, const MaterialHandler::Ptr& materialHandler
    );

    /*!
        Set a ContextOpsHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param contextOpsHandler the ContextOps interface factory.
    */
    void setContextOpsHandler(
        const Rtid& rtId, const ContextOpsHandler::Ptr& contextOpsHandler
    );

    /*!
        Set a UIInfoHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param uiInfoHandler the UIInfoHandler object.
    */
    void setUIInfoHandler(
        const Rtid& rtId, const UIInfoHandler::Ptr& uiInfoHandler
    );

    /*!
        Set a CameraHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param cameraHandler the CameraHandler object.
    */
    void setCameraHandler(
        const Rtid& rtId, const CameraHandler::Ptr& cameraHandler
    );

    /*!
     Set a LightHandler to a given runtime ID.
     \param rtId the ID of the runtime for the handler.
     \param lightHandler the LightHandler object.
    */
    void setLightHandler(
        const Rtid &rtId, const LightHandler::Ptr &lightHandler);

    /*!
     Set a Light2Handler to a given runtime ID.
     \param rtId the ID of the runtime for the handler.
     \param light2Handler the Light2Handler object.
    */
    void setLight2Handler(
        const Rtid& rtId, const Light2Handler::Ptr& light2Handler);

    /*!
        Set a PathMappingHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param pathMappingHandler the PathMappingHandler object.
    */
    void setPathMappingHandler(
        const Rtid& rtId, const PathMappingHandler::Ptr& pathMappingHandler
    );

    /*!
        Set a NodeDefHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param nodeDefHandler the NodeDefHandler object.
    */
    void setNodeDefHandler(
        const Rtid& rtId, const NodeDefHandler::Ptr& nodeDefHandler
    );

    /*!
        Set a SceneSegmentHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param sceneSegmentHandler the SceneSegmentHandler object.
    */
    void setSceneSegmentHandler(
        const Rtid& rtId, const SceneSegmentHandler::Ptr& sceneSegmentHandler
    );

    /*!
        Set a BatchOpsHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param batchOpsHandler the BatchOps interface factory.
    */
    void setBatchOpsHandler(
        const Rtid& rtId, const BatchOpsHandler::Ptr& batchOpsHandler
    );

    /*!
        Set a ClipboardHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param clipboardHandler the Clipboard interface factory.
    */
    void setClipboardHandler(
        const Rtid& rtId, const ClipboardHandler::Ptr& clipboardHandler
    );

    /*!
        Set a ColorManagementHandler to a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \param colorManagementHandler the Color Management interface factory.
    */
    void setColorManagementHandler(
        const Rtid& rtId, const ColorManagementHandler::Ptr& colorManagementHandler
    );

    /*!
        Retrieve the HierarchyHandler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return HierarchyHandler corresponding to runtime type. 
        Null pointer if the argument runtime does not exist
    */
    HierarchyHandler::Ptr hierarchyHandler(const Rtid& rtId) const;

    /*!
        Retrieve the Hierarchy handler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return HierarchyHandler corresponding to runtime type.
        \exception InvalidRunTimeId Thrown if argument runtime does not exist.
    */
    const HierarchyHandler& hierarchyHandlerRef(const Rtid& rtId) const;

    /*!
        Retrieve the Transform3d handler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return Transform3dHandler corresponding to runtime type.
        Null pointer if the argument runtime does not exist.
    */
    Transform3dHandler::Ptr transform3dHandler(const Rtid& rtId) const;

    /*!
        Retrieve the SceneItemOps handler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return SceneItemOpsHandler corresponding to runtime type.
        Null pointer if the argument runtime does not exist.
    */
    SceneItemOpsHandler::Ptr sceneItemOpsHandler(const Rtid& rtId) const;

    /*!
        Retrieve the Attributes handler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return AttributesHandler corresponding to runtime type.
        Null pointer if the argument runtime does not exist.
    */
    AttributesHandler::Ptr attributesHandler(const Rtid& rtId) const;

    /*!
        Retrieve the ConnectionHandler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return ConnectionHandler corresponding to runtime type.
        Null pointer if the argument runtime does not exist.
    */
    ConnectionHandler::Ptr connectionHandler(const Rtid& rtId) const;

    /*!
        Retrieve the UINodeGraphNodeHandler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return UINodeGraphNodeHandler corresponding to runtime type.
        Null pointer if the argument runtime does not exist.
    */
    UINodeGraphNodeHandler::Ptr uiNodeGraphNodeHandler(const Rtid& rtId) const;

    /*!
        Retrieve the Object3d handler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return Object3dHandler corresponding to runtime type.
        Null pointer if the argument runtime does not exist.
    */
    Object3dHandler::Ptr object3dHandler(const Rtid& rtId) const;

    /*!
        Retrieve the Material handler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return MaterialHandler corresponding to runtime type.
        Null pointer if the argument runtime does not exist.
    */
    MaterialHandler::Ptr materialHandler(const Rtid& rtId) const;

    /*!
        Retrieve the ContextOps handler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return ContextOpsHandler corresponding to runtime type.
        Null pointer if the argument runtime does not exist.
    */
    ContextOpsHandler::Ptr contextOpsHandler(const Rtid& rtId) const;

    /*!
        Retrieve the UI Info handler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return UIInfoHandler corresponding to runtime type.
        Null pointer if the argument runtime does not exist.
    */
    UIInfoHandler::Ptr uiInfoHandler(const Rtid& rtId) const;

    /*!
        Retrieve the Camera handler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return CameraHandler corresponding to runtime type.
        Null pointer if the argument runtime does not exist.
    */
    CameraHandler::Ptr cameraHandler(const Rtid& rtId) const;

    /*!
        Retrieve the Light handler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return LightHandler corresponding to runtime type.
        Null pointer if the argument runtime does not exist.
    */
    LightHandler::Ptr lightHandler(const Rtid &rtId) const;

    /*!
    Retrieve the Light2 handler of a given runtime ID.
    \param rtId the ID of the runtime for the handler.
    \return Light2Handler corresponding to runtime type.
    Null pointer if the argument runtime does not exist.
    */
    Light2Handler::Ptr light2Handler(const Rtid& rtId) const;

    /*!
        Retrieve the path mapping handler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return PathMappingHandler corresponding to runtime type.
        Null pointer if the argument runtime does not exist.
    */
    PathMappingHandler::Ptr pathMappingHandler(const Rtid& rtId) const;

    /*!
        Retrieve the node definition handler of a given
        runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return NodeDefHandler corresponding to runtime
        type. Null pointer if the argument runtime does not exist.
    */
    NodeDefHandler::Ptr nodeDefHandler(const Rtid& rtId) const;

    /*!
        Retrieve the scene segment handler of a given
        runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return SceneSegmentHandler corresponding to runtime
        type. Null pointer if the argument runtime does not exist.
    */
    SceneSegmentHandler::Ptr sceneSegmentHandler(const Rtid& rtId) const;

    /*!
        Retrieve the BatchOps handler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return BatchOpsHandler corresponding to runtime type.
        Null pointer if the argument runtime does not exist.
    */
    BatchOpsHandler::Ptr batchOpsHandler(const Rtid& rtId) const;

    /*!
        Retrieve the clipboard handler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return ClipboardHandler corresponding to runtime type.
        Null pointer if the argument runtime does not exist.
    */
    ClipboardHandler::Ptr clipboardHandler(const Rtid& rtId) const;

    /*!
        Retrieve the color management handler of a given runtime ID.
        \param rtId the ID of the runtime for the handler.
        \return ColorManagementHandler corresponding to runtime type.
        Null pointer if the argument runtime does not exist.
    */
    ColorManagementHandler::Ptr colorManagementHandler(const Rtid& rtId) const;

    //! \return List of all registered runtime IDs.
    std::list<Rtid> getIds() const;

    /*!
        Registers a dynamic handler given a handler id.
        \param rtId the ID of the runtime for the handler to register to.
        \param handlerId the ID of the handler to be registered for the runtime.
        \param handler pointer to the handler object
    */
    void registerHandler(const Ufe::Rtid& rtId,
        const std::string& handlerId,
        const HandlerInterface::Ptr& handler);

    /*!
        Unregisters a dynamic handler given a handler id and a runtime id.
        \param rtId the ID of the runtime this handler was registered to.
        \param handlerId the ID of the handler to be unregistered.
    */
    void unregisterHandler(const Ufe::Rtid& rtId, const std::string& handlerId);

    /*!
        Provides a pointer to a handler of type T in the specified runtime.
        \param rtId the ID of the runtime the handler is register to.
        \return A handler of type T that is derived from HandlerInterface.
        nullptr if no handler of such type was found on this runtime.
    */
    template <typename T> std::shared_ptr<T> handler(const Ufe::Rtid& rtId)
    {
        static_assert(std::is_base_of<HandlerInterface, T>::value, "Only Ufe::HandlerInterface types are allowed");
        return std::static_pointer_cast<T>(resolveHandler(rtId, T::id));
    }

private:

    //! Cannot create a runtime manager aside from the singleton instance.
    RunTimeMgr();

    HandlerInterface::Ptr resolveHandler(const Ufe::Rtid& rtId, const std::string& handlerId);

};

}

#endif /* _runTimeMgr */
