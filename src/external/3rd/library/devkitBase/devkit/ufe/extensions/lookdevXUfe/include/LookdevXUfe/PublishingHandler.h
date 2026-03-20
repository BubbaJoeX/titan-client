//*****************************************************************************
// Copyright (c) 2024 Autodesk, Inc.
// All rights reserved.
//
// These coded instructions, statements, and computer programs contain
// unpublished proprietary information written by Autodesk, Inc. and are
// protected by Federal copyright law. They may not be disclosed to third
// parties or copied or duplicated in any form, in whole or in part, without
// the prior written consent of Autodesk, Inc.
//*****************************************************************************
#pragma once

#include "Export.h"

#include <ufe/runTimeMgr.h>
#include <ufe/sceneItem.h>
#include <ufe/undoableCommandMgr.h>

LOOKDEVXUFE_NS_DEF
{

class LOOKDEVX_UFE_EXPORT PublishingHandler : public Ufe::HandlerInterface
{
public:
    using Ptr = std::shared_ptr<PublishingHandler>;

    static constexpr auto id = "LookdevXHandler_Publishing";

    static Ptr get(const Ufe::Rtid& rtId);

    //! @brief Scans the library seach path and reloads the node definitions, so that any changes made to the library
    //! files will be applied to all shading graphs of this runtime within the current scene.
    //!
    //! Any new node definitions will become available, any deleted node definitions will become unavailable and any
    //! changes to existing node definitions will be picked up.
    virtual void reloadNodeDefinitions() const = 0;

    //! @brief Makes a scene item editable which means that the scene item is replaced with a compound with
    //! an editable definition implementation.
    //!
    //! @param sceneItem The scene item to make editable.
    //! @throws runtime_error if issue encountered making the scene item editable.
    //! @return An command to execute the operation.
    [[nodiscard]] virtual Ufe::SceneItemResultUndoableCommand::Ptr makeEditable(
        const Ufe::SceneItem::Ptr& /*sceneItem*/) const
    {
        return nullptr;
    }

    //! @brief Publishes a compound NodeGraph. Creates a node definition and a functional NodeGraph and exports it to
    //! the specified filepath.
    //!
    //! @param item Scene item of a compound NodeGraph.
    //! @param nodeName Node name (or category) of the new node definition. Can be overloaded for different output
    //! types.
    //! @param nodeDefName Name of the new node definition. Must be unique.
    //! @param nodeGraphName Name of the new functional NodeGraph. Must be unique.
    //! @param namespaceName Name of the namespace of the node definition.
    //! @param nodeGroup An optional group to which the node definition belongs.
    //! @param documentation An optional description of the function or purpose of this node; may include basic HTML
    //! formatting.
    //! @param filepath File to which the new node definition and functional NodeGraph will get exported. Existing files
    //! will overwritten.
    //!
    //! @throws std::exception if \p item is not a NodeGraph, if \p nodeDefName or \p nodeGraphName is not unique or if
    //! writing to \p filepath fails.
    // TODO(frohnej): Consider creating a struct for all the parameters.
    //     See https://git.autodesk.com/media-and-entertainment/lookdevx/pull/2198#discussion_r4026161
    // TODO(frohnej): Consider passing `nodeGroup` and `documentation` as a dictionary, alongside other attributes that
    //     can be part of the node definition like `uiname`.
    //     See https://git.autodesk.com/media-and-entertainment/lookdevx/pull/2198#discussion_r4031026
    virtual void publish(const Ufe::SceneItem::Ptr& item,
                         const std::string& nodeName,
                         const std::string& nodeDefName,
                         const std::string& nodeGraphName,
                         const std::string& namespaceName,
                         const std::string& nodeGroup,
                         const std::string& documentation,
                         const std::string& filepath) const = 0;

    //! @brief Checks if the scene item is a node graph that can be published.
    //!
    //! @param item The scene item to check.
    //! @return true if the scene item can be published. False otherwise.
    [[nodiscard]] virtual bool isPublishable(const Ufe::SceneItem::Ptr& /*item*/) const
    {
        return false;
    }
};

} // LOOKDEVXUFE_NS_DEF
