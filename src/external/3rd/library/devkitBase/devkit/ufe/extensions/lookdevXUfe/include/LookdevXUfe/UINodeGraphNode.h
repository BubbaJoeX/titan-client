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
#ifndef UI_NODE_GRAPH_NODE_H
#define UI_NODE_GRAPH_NODE_H

#include "Export.h"

#include <ufe/sceneItem.h>
#include <ufe/uiNodeGraphNode.h>
#include <ufe/undoableCommand.h>

#include <memory>

LOOKDEVXUFE_NS_DEF
{
/*!
  \class UINodeGraphNode
  \brief Abstract base class for UINodeGraphNode interface.

  This class defines an interface for factory objects that runtimes
  implement to handle UINodeGraphNodes. Note: this interface only
  includes information not found on the Ufe::UINodeGraphNode class.

*/
class LOOKDEVX_UFE_EXPORT UINodeGraphNode : public Ufe::UINodeGraphNode
{
public:
    using Ptr = std::shared_ptr<UINodeGraphNode>;

    /*! Convenience method that calls the UINodeGraphNode method on the UINodeGraphNode
        handler for the item. Returns a null pointer if the argument is null,
        or has an empty path.
        \param item SceneItem to build UINodeGraphNode on
        \return UINodeGraphNode of given SceneItem.
    */
    static Ptr uiNodeGraphNode(const Ufe::SceneItem::Ptr& item);

    //! Constructor.
    UINodeGraphNode() = default;
    //! Destructor.
    ~UINodeGraphNode() override = default;

    //@{
    //! Delete the copy/move constructors assignment operators.
    UINodeGraphNode(const UINodeGraphNode&) = delete;
    UINodeGraphNode& operator=(const UINodeGraphNode&) = delete;
    UINodeGraphNode(UINodeGraphNode&&) = delete;
    UINodeGraphNode& operator=(UINodeGraphNode&&) = delete;
    //@}
};

} // LOOKDEVXUFE_NS_DEF

#endif
