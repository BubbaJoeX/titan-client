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

#include <ufe/handlerInterface.h>
#include <ufe/rtid.h>
#include <ufe/runTimeMgr.h>
#include <ufe/sceneItem.h>

#include <any>
#include <string>

LOOKDEVXUFE_NS_DEF
{

class LOOKDEVX_UFE_EXPORT DebugHandler : public Ufe::HandlerInterface
{
public:
    using Ptr = std::shared_ptr<DebugHandler>;

    static constexpr auto id = "DebugHandler";

    static Ptr get(const Ufe::Rtid& rtId);

    virtual std::string exportToString(Ufe::SceneItem::Ptr sceneItem) = 0;

    virtual void runCommand(const std::string& command, const std::unordered_map<std::string, std::any>& args = {}) = 0;

    /**
     * @brief Checks whether a \p nodeDef has viewport support.
     * @note We might move this function to a more specialized interface in the future. But for now, given that the
     * current utilization of this function can be considered a "debugging" feature by users, we can keep it here.
     * @param nodeDef The nodeDef to check for viewport support.
     * @return True if the nodeDef has viewport support, false otherwise.
     */
    [[nodiscard]] virtual bool hasViewportSupport(const Ufe::NodeDef::Ptr& nodeDef) const = 0;

    /**
     * @brief Returns the implementation of the specified node definition.
     *
     * This method will look for an implementation of the specified node definition and return its scene item. It will
     * only consider generic implementations that are not limited to a specific target (like GLSL or OSL). This means it
     * will usually return a functional node graph.
     *
     * @param nodeDef The node definition whose implementation will be returned.
     * @return The scene item of the node definition's implementation.
     */
    [[nodiscard]] virtual Ufe::SceneItem::Ptr getImplementation(const Ufe::NodeDef::Ptr& /*nodeDef*/) const
    {
        return nullptr;
    }

    /**
     * @brief Checks whether a scene item is a library item.
     * @param sceneItem The scene item to check.
     * @return True if the scene item is a library item, false otherwise.
     */
    [[nodiscard]] virtual bool isLibraryItem(const Ufe::SceneItem::Ptr& /*sceneItem*/) const
    {
        return false;
    }

    /**
     * @brief Checks whether a scene item should be editable in the UI.
     * @param sceneItem The scene item to check.
     * @return true if the scene item should be editable in the UI, false otherwise.
     */
    [[nodiscard]] virtual bool isEditable(const Ufe::SceneItem::Ptr& /*sceneItem*/) const
    {
        return true;
    }
};

} // LOOKDEVXUFE_NS_DEF
