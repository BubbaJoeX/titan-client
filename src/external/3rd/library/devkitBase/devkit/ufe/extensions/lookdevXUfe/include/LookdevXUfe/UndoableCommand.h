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
#ifndef LOOKDEVXUFE_UNDOABLECOMMAND_H
#define LOOKDEVXUFE_UNDOABLECOMMAND_H

#include "ExtendedConnection.h"

#include <ufe/undoableCommand.h>

#include <functional>

LOOKDEVXUFE_NS_DEF
{

// Retrieves the combine node connected to the provided destination attribute, if one exists.
[[nodiscard]] LOOKDEVX_UFE_EXPORT Ufe::SceneItem::Ptr combineConnectedToDst(const Ufe::Attribute::Ptr& dstAttr);

// Retrieves the separate node connected to the provided source attribute, if one exists.
[[nodiscard]] LOOKDEVX_UFE_EXPORT Ufe::SceneItem::Ptr separateConnectedToSrc(
    const Ufe::Attribute::Ptr& srcAttr,
    const Ufe::Attribute::Ptr& dstAttr,
    const std::vector<std::string>& srcAttrComponentNames);

/*!
  \brief Command that creates a hidden shader.
*/
class CreateHiddenShaderCommand : public Ufe::UndoableCommand
{
public:
    using Ptr = std::shared_ptr<CreateHiddenShaderCommand>;

    //! Default constructor.
    LOOKDEVX_UFE_EXPORT CreateHiddenShaderCommand(const Ufe::SceneItem::Ptr& parentSceneItem,
                                                  const std::string& name, // NOLINT
                                                  const std::string& type);

    //! Destructor.
    LOOKDEVX_UFE_EXPORT ~CreateHiddenShaderCommand() override;

    //@{
    //! No copy or move constructor/assignment.
    CreateHiddenShaderCommand(const CreateHiddenShaderCommand&) = delete;
    CreateHiddenShaderCommand& operator=(const CreateHiddenShaderCommand&) = delete;
    CreateHiddenShaderCommand(CreateHiddenShaderCommand&&) = delete;
    CreateHiddenShaderCommand& operator=(CreateHiddenShaderCommand&&) = delete;
    //@}

    //! Create a CreateHiddenShaderCommand object
    LOOKDEVX_UFE_EXPORT static CreateHiddenShaderCommand::Ptr create(const Ufe::SceneItem::Ptr& parentSceneItem,
                                                                     const std::string& name, // NOLINT
                                                                     const std::string& type);
    LOOKDEVX_UFE_EXPORT Ufe::SceneItem::Ptr getShader();

    LOOKDEVX_UFE_EXPORT void execute() override;
    LOOKDEVX_UFE_EXPORT void undo() override;
    LOOKDEVX_UFE_EXPORT void redo() override;

private:
    Ufe::Path m_parentPath;
    Ufe::Path m_path;
    std::string m_name;
    std::string m_type;
    Ufe::SceneItem::Ptr m_shader;
};

/*!
  \brief Command that modifies the data model and produces an extended connection, e.g. a "regular" connection,
         either a new or modified component connection.

  This class provides the interface to retrieve the extended connection, which is set on
  command execution.
*/
class LOOKDEVX_UFE_EXPORT CreateConnectionResultCommand : public Ufe::UndoableCommand
{
public:
    using Ptr = std::shared_ptr<CreateConnectionResultCommand>;

    //! Default constructor.
    CreateConnectionResultCommand() = default;

    //! Destructor.
    ~CreateConnectionResultCommand() override = default;

    //@{
    //! No copy or move constructor/assignment.
    CreateConnectionResultCommand(const CreateConnectionResultCommand&) = delete;
    CreateConnectionResultCommand& operator=(const CreateConnectionResultCommand&) = delete;
    CreateConnectionResultCommand(CreateConnectionResultCommand&&) = delete;
    CreateConnectionResultCommand& operator=(CreateConnectionResultCommand&&) = delete;
    //@}

    //! Pure virtual method to retrieve the resulting connection.
    [[nodiscard]] virtual std::shared_ptr<ExtendedConnection> extendedConnection() const = 0;

    [[nodiscard]] bool isComponentConnected(const Ufe::Attribute::Ptr& srcAttr,
                                            const std::string& srcComponent,
                                            const Ufe::Attribute::Ptr& dstAttr,
                                            const std::string& dstComponent) const;

    [[nodiscard]] static bool isConverterConnected(const Ufe::Attribute::Ptr& srcAttr,
                                                   const Ufe::Attribute::Ptr& dstAttr);

protected:
    [[nodiscard]] virtual std::vector<std::string> componentNames(const Ufe::Attribute::Ptr& attr) const = 0;

    void connectComponent(const Ufe::Attribute::Ptr& srcAttr,
                          const std::string& srcComponent,
                          const Ufe::Attribute::Ptr& dstAttr,
                          const std::string& dstComponent) const;

    static void throwIfSceneItemsNotComponentConnectable(const Ufe::SceneItem::Ptr& srcSceneItem,
                                                         const Ufe::SceneItem::Ptr& dstSceneItem);

    void createConnection(const AttributeComponentInfo& srcAttrInfo, const AttributeComponentInfo& dstAttrInfo) const;

private:
    static void createRegularConnection(const Ufe::Attribute::Ptr& srcAttr, const Ufe::Attribute::Ptr& dstAttr);

    static void createConverterConnection(const Ufe::Attribute::Ptr& srcAttr, const Ufe::Attribute::Ptr& dstAttr);

    [[nodiscard]] static bool isConversionSupported(const Ufe::Attribute::Ptr& srcAttr,
                                                    const Ufe::Attribute::Ptr& dstAttr);

    [[nodiscard]] static Ufe::SceneItem::Ptr createConverter(const Ufe::Attribute::Ptr& srcAttr,
                                                             const Ufe::Attribute::Ptr& dstAttr);
};

/*!
  \brief Command that modifies the data model and deletes an extended connection, e.g. a "regular" connection,
         either a new or modified component connection.

  This class provides the interface to delete an extended connection, which is set on
  command execution.
*/
class LOOKDEVX_UFE_EXPORT DeleteConnectionCommand : public Ufe::UndoableCommand
{
public:
    using Ptr = std::shared_ptr<DeleteConnectionCommand>;

    //! Default constructor.
    DeleteConnectionCommand() = default;

    //! Destructor.
    ~DeleteConnectionCommand() override = default;

    //@{
    //! No copy or move constructor/assignment.
    DeleteConnectionCommand(const DeleteConnectionCommand&) = delete;
    DeleteConnectionCommand& operator=(const DeleteConnectionCommand&) = delete;
    DeleteConnectionCommand(DeleteConnectionCommand&&) = delete;
    DeleteConnectionCommand& operator=(DeleteConnectionCommand&&) = delete;
    //@}

    [[nodiscard]] bool isComponentConnected(const Ufe::Attribute::Ptr& srcAttr,
                                            const std::string& srcComponent,
                                            const Ufe::Attribute::Ptr& dstAttr,
                                            const std::string& dstComponent) const;

    [[nodiscard]] static bool isConverterConnected(const Ufe::Attribute::Ptr& srcAttr,
                                                   const Ufe::Attribute::Ptr& dstAttr);

protected:
    [[nodiscard]] virtual std::vector<std::string> componentNames(const Ufe::Attribute::Ptr& attr) const = 0;

    void disconnectComponent(const Ufe::Attribute::Ptr& srcAttr,
                             const std::string& srcComponent,
                             const Ufe::Attribute::Ptr& dstAttr,
                             const std::string& dstComponent) const;

    static void disconnectConverter(const Ufe::Attribute::Ptr& srcAttr, const Ufe::Attribute::Ptr& dstAttr);

    void deleteConnection(const AttributeComponentInfo& srcAttrInfo, const AttributeComponentInfo& dstAttrInfo) const;

private:
    static void deleteRegularConnection(const Ufe::Attribute::Ptr& srcAttr, const Ufe::Attribute::Ptr& dstAttr);
};

/*!
  \brief Command that executes provided lambdas for execute/undo/redo.
*/
class LOOKDEVX_UFE_EXPORT LambdaCommand : public Ufe::UndoableCommand
{
public:
    using Ptr = std::shared_ptr<LambdaCommand>;

    //! Default constructor.
    LambdaCommand(std::function<void()> execute, std::function<void()> undo, std::function<void()> redo);

    //! Destructor.
    ~LambdaCommand() override = default;

    //@{
    //! No copy or move constructor/assignment.
    LambdaCommand(const LambdaCommand&) = delete;
    LambdaCommand& operator=(const LambdaCommand&) = delete;
    LambdaCommand(LambdaCommand&&) = delete;
    LambdaCommand& operator=(LambdaCommand&&) = delete;
    //@}

    //! Create a CreateHiddenShaderCommand object
    static LambdaCommand::Ptr create(const std::function<void()>& execute,
                                     const std::function<void()>& undo,
                                     const std::function<void()>& redo);

    void execute() override;
    void undo() override;
    void redo() override;

private:
    std::function<void()> m_execute;
    std::function<void()> m_undo;
    std::function<void()> m_redo;
};

} // LOOKDEVXUFE_NS_DEF

#endif
