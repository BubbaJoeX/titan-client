//*****************************************************************************
// Copyright (c) 2023 Autodesk, Inc.
// All rights reserved.
//
// These coded instructions, statements, and computer programs contain
// unpublished proprietary information written by Autodesk, Inc. and are
// protected by Federal copyright law. They may not be disclosed to third
// parties or copied or duplicated in any form, in whole or in part, without
// the prior written consent of Autodesk, Inc.
//*****************************************************************************
#ifndef UI_NODE_GRAPH_NODE_HANDLER_H
#define UI_NODE_GRAPH_NODE_HANDLER_H

#include "Export.h"
#include "UINodeGraphNode.h"

#include <ufe/handlerInterface.h>
#include <ufe/runTimeMgr.h>
#include <ufe/sceneItem.h>
#include <ufe/uiNodeGraphNodeHandler.h>

#include <memory>

LOOKDEVXUFE_NS_DEF
{

//! \brief Factory base class for UINodeGraphNode interface.
/*!

  This base class defines an interface for factory objects that runtimes
  can implement to create a UINodeGraphNode interface object.
*/
class LOOKDEVX_UFE_EXPORT UINodeGraphNodeHandler : public Ufe::UINodeGraphNodeHandler
{
public:
    using Ptr = std::shared_ptr<UINodeGraphNodeHandler>;

    UINodeGraphNodeHandler() = default;
    ~UINodeGraphNodeHandler() override = default;

    // Delete the copy/move constructors assignment operators.
    UINodeGraphNodeHandler(const UINodeGraphNodeHandler&) = delete;
    UINodeGraphNodeHandler& operator=(const UINodeGraphNodeHandler&) = delete;
    UINodeGraphNodeHandler(UINodeGraphNodeHandler&&) = delete;
    UINodeGraphNodeHandler& operator=(UINodeGraphNodeHandler&&) = delete;

    /*!
        Creates a UINodeGraphNode interface on the given SceneItem.
        \param item SceneItem to use to retrieve its UINodeGraphNode interface.
        \return UINodeGraphNode interface of given SceneItem. Returns a null
        pointer if no UINodeGraphNode interface can be created for the item.
    */
    [[nodiscard]] virtual UINodeGraphNode::Ptr lxUINodeGraphNode(const Ufe::SceneItem::Ptr& item) const = 0;
};

} // LOOKDEVXUFE_NS_DEF

#endif
