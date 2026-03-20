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
#pragma once

#include "Material.h"
#include "ValidationLog.h"

#include <ufe/attribute.h>
#include <ufe/handlerInterface.h>
#include <ufe/nodeDef.h>
#include <ufe/runTimeMgr.h>
#include <ufe/sceneItem.h>
#include <ufe/undoableCommand.h>

#include <memory>

LOOKDEVXUFE_NS_DEF
{

//! \brief Factory base class for Material interface.
/*!

  This base class defines an interface for factory objects that runtimes
  can implement to create a Material interface object.
*/
class LOOKDEVX_UFE_EXPORT MaterialHandler : public Ufe::HandlerInterface
{
public:
    typedef std::shared_ptr<MaterialHandler> Ptr;

    static constexpr auto id = "LookdevXHandler_Material";

    static Ptr get(const Ufe::Rtid& rtId);

    /*!
        Creates a Material interface on the given SceneItem.
        \param item SceneItem to use to retrieve its Material interface.
        \return Material interface of given SceneItem. Returns a null
        pointer if no Material interface can be created for the item.
    */
    virtual LookdevXUfe::Material::Ptr material(const Ufe::SceneItem::Ptr& item) const = 0;

    /*!
        Validates all the contents of the material. Items that are critical to building a successful shader will be
       marked as errors. Issues on isolated islands that are not part of the material construction will be demoted to
       warnings.

        \param material The material to inspect.
        \return A validation log for the material. The log will be empty if no errors were detected. A null pointer will
                be returned if the SceneItem was not a material.
    */
    [[nodiscard]] virtual ValidationLog::Ptr validateMaterial(const Ufe::SceneItem::Ptr& material) const = 0;

    /*!
        Convenience method that calls \ref createBackdropCmdImpl on the appropriate material handler.

        Returns an undoable command to create a backdrop as a child of \p parent. If the
        command cannot be created, a null pointer is returned.

        \param parent SceneItem under which the backdrop is to be created.
        \param name Name of the new backdrop.
        \return An undoable command that will create a new backdrop when executed. If the command
                cannot be created, a null pointer is returned.
    */
    static Ufe::SceneItemResultUndoableCommand::Ptr createBackdropCmd(const Ufe::SceneItem::Ptr& parent,
                                                                      const Ufe::PathComponent& name);
    /*!
        Convenience method that calls \ref createNodeGraphImpl on the appropriate material handler.

        Creates an empty node graph as a child of \p parent. If the node graph cannot be created, a
        null pointer is returned.

        \param parent SceneItem under which the node graph is to be created.
        \param name Name of the new node graph.
        \return The scene item of the newly created node graph if the node graph was created
                successfully. If the creation fails, a null pointer is returned.
    */
    static Ufe::SceneItem::Ptr createNodeGraph(const Ufe::SceneItem::Ptr& parent, const Ufe::PathComponent& name);

    /*!
        Convenience method that calls \ref createNodeGraphCmdImpl on the appropriate material handler.

        Returns an undoable command to create an empty node graph as a child of \p parent. If the
        command cannot be created, a null pointer is returned.

        \param parent SceneItem under which the node graph is to be created.
        \param name Name of the new node graph.
        \return An undoable command that will create a new node graph when executed. If the command
                cannot be created, a null pointer is returned.
    */
    static Ufe::SceneItemResultUndoableCommand::Ptr createNodeGraphCmd(const Ufe::SceneItem::Ptr& parent,
                                                                       const Ufe::PathComponent& name);

    /*!
        Convenience method that calls \ref isBackdropImpl on the appropriate material handler.
    */
    static bool isBackdrop(const Ufe::SceneItem::Ptr& item);

    /*!
        Convenience method that calls \ref isNodeGraphImpl on the appropriate material handler.
    */
    static bool isNodeGraph(const Ufe::SceneItem::Ptr& item);

    /*!
        Convenience method that calls \ref isMaterialImpl on the appropriate material handler.
    */
    static bool isMaterial(const Ufe::SceneItem::Ptr& item);

    /*!
        Convenience method that calls \ref isShaderImpl on the appropriate material handler.
    */
    static bool isShader(const Ufe::SceneItem::Ptr& item);

    /*!
        Convenience method that calls \ref allowedInNodeGraphImpl on the appropriate material handler.
    */
    [[nodiscard]] virtual bool allowedInNodeGraph(const std::string& nodeDefType) const = 0;

private:
    [[nodiscard]] virtual Ufe::SceneItemResultUndoableCommand::Ptr createBackdropCmdImpl(
        const Ufe::SceneItem::Ptr& parent, const Ufe::PathComponent& name) const = 0;

    [[nodiscard]] virtual Ufe::SceneItem::Ptr createNodeGraphImpl(const Ufe::SceneItem::Ptr& parent,
                                                                  const Ufe::PathComponent& name) const;

    [[nodiscard]] virtual Ufe::SceneItemResultUndoableCommand::Ptr createNodeGraphCmdImpl(
        const Ufe::SceneItem::Ptr& parent, const Ufe::PathComponent& name) const = 0;

    [[nodiscard]] virtual bool isBackdropImpl(const Ufe::SceneItem::Ptr& item) const = 0;

    [[nodiscard]] virtual bool isNodeGraphImpl(const Ufe::SceneItem::Ptr& item) const = 0;

    [[nodiscard]] virtual bool isMaterialImpl(const Ufe::SceneItem::Ptr& item) const = 0;

    [[nodiscard]] virtual bool isShaderImpl(const Ufe::SceneItem::Ptr& item) const = 0;
};

} // LOOKDEVXUFE_NS_DEF
