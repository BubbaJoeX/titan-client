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

#include "Export.h"

#include <ufe/nodeDef.h>
#include <ufe/runTimeMgr.h>
#include <ufe/undoableCommand.h>

LOOKDEVXUFE_NS_DEF
{

class LOOKDEVX_UFE_EXPORT LookdevHandler : public Ufe::HandlerInterface
{
public:
    using Ptr = std::shared_ptr<LookdevHandler>;

    static constexpr auto id = "LookdevXHandler_Lookdev";

    static Ptr get(const Ufe::Rtid& rtId);

    /*!
        Convenience method that calls \ref createLookdevContainerCmdImpl on the appropriate material handler.

        A lookdev container is a container to hold lookdev/shading items.
        Examples:
        - In USD, shaders are contained within materials.
        - In MaterialX, shaders are contained within documents.

        Returns an undoable command to create an empty lookdev container as a child of \p parent. If the
        command cannot be created, a null pointer is returned.

        \note This method is similar to Ufe::NodeDef::createNodeCmd in that it takes the same
              arguments. Also, both methods create a new node in the data model and don't just
              return a scene item of something that already exists. The difference between
              the two methods is that Ufe::NodeDef::createNodeCmd can only be used to create
              instances of node definitions, which are leaf nodes. This method creates a container.

        \param parent SceneItem under which the lookdev container is to be created.
        \param name Name of the new lookdev container.
        \return An undoable command that will create a lookdev container when executed. If the command
                cannot be created, a null pointer is returned.
    */
    static Ufe::SceneItemResultUndoableCommand::Ptr createLookdevContainerCmd(const Ufe::SceneItem::Ptr& parent,
                                                                              const Ufe::PathComponent& name);

    /*!
        Convenience method that calls \ref createLookdevContainerCmdImpl on the appropriate material handler.

        A lookdev container is a container to hold lookdev/shading items.
        Examples:
        - In USD, shaders are contained within materials.
        - In MaterialX, shaders are contained within documents.

        Returns an undoable command to create a lookdev container as a child of \p parent. If the command cannot be
        created, a null pointer is returned.

        The lookdev container is created from the specified node definition \p nodeDef, that is, the new lookdev
        container will contain an instance of \p nodeDef. The command will also create additional nodes and connections
        within the new lookdev container, so that the result will be an assignable material based on \p nodeDef.

        If the creation succeeds, the resulting scene item will be a of new instance of \p nodeDef.

        \param parent Scene item under which the lookdev container is to be created.
        \param nodeDef Node definition to create the lookdev container from.
        \return An undoable command that will create a new lookdev container when executed. The resulting scene item
                will be a new instance of \p nodeDef inside of the new container. If the command cannot be created, a
                null pointer is returned.
    */
    static Ufe::SceneItemResultUndoableCommand::Ptr createLookdevContainerCmd(const Ufe::SceneItem::Ptr& parent,
                                                                              const Ufe::NodeDef::Ptr& nodeDef);

    /*!
        Convenience method that calls \ref createLookdevEnvironmentCmdImpl on the appropriate material
        handler.

        A lookdev environment is a scene item which is able to contain hold lookdev containers.
        Examples:
        - In USD, a material is a lookdev container. All materials have to reside within a USD stage
          and should be placed in a materials scope. Thus, a lookdev environment in USD is a materials
          scope within a USD stage.
        - In MaterialX, a document is a lookdev container. All documents have to reside within a
          document stack. Thus, a lookdev environment in MaterialX is a document stack.

        Returns an undoable command to create a scene item that can serve as the parent of a
        lookdev container in the specified run time. The scene item will satisfy all runtime specific
        requirements and conventions as to where lookdev containers should be placed.

        Executing the command returns \p ancestor if it is already a suitable parent for lookdev containers.
        Otherwise, the command looks for or creates a suitable parent according to the following
        rules:
        - The parent is a descendant of \p ancestor.
        - The parent satisfies all runtime specific requirements or conventions as to where
          lookdev containers should be placed.
        - The parent is as close to \p ancestor as possible. Creating new objects is not penalized.
        - If the runtime has a multi-level parenting scheme, a hierarchy of parents may be created.
        - If creating new objects and reusing existing objects results in suitable parents equally
          close to \p ancestor, creating new objects is be preferred.

        \param ancestor SceneItem under which to look for or create a suitable parent for lookdev containers
                        of the specified run time.
        \param targetRunTimeId A parent for lookdev containers of this run time will be created.
        \return An undoable command that will create a scene item that can serve as the parent of a
                lookdev container.
    */
    static Ufe::SceneItemResultUndoableCommand::Ptr createLookdevEnvironmentCmd(const Ufe::SceneItem::Ptr& ancestor,
                                                                                Ufe::Rtid targetRunTimeId);

    /*!
        Convenience method that calls \ref isLookdevContainerImpl on the appropriate material handler.
    */
    static bool isLookdevContainer(const Ufe::SceneItem::Ptr& item);

    [[nodiscard]] virtual Ufe::SceneItemResultUndoableCommand::Ptr createLookdevContainerCmdImpl(
        const Ufe::SceneItem::Ptr& parent, const Ufe::PathComponent& name) const = 0;
    [[nodiscard]] virtual Ufe::SceneItemResultUndoableCommand::Ptr createLookdevContainerCmdImpl(
        const Ufe::SceneItem::Ptr& parent, const Ufe::NodeDef::Ptr& nodeDef) const = 0;
    [[nodiscard]] virtual Ufe::SceneItemResultUndoableCommand::Ptr createLookdevEnvironmentCmdImpl(
        const Ufe::SceneItem::Ptr& ancestor, Ufe::Rtid targetRunTimeId) const = 0;
    [[nodiscard]] virtual bool isLookdevContainerImpl(const Ufe::SceneItem::Ptr& item) const = 0;
};

} // LOOKDEVXUFE_NS_DEF
