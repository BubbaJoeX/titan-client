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
#ifndef SCENE_ITEM_UI_H
#define SCENE_ITEM_UI_H

#include "Export.h"

#include <ufe/sceneItem.h>
#include <ufe/undoableCommand.h>

LOOKDEVXUFE_NS_DEF
{

//! \brief Abstract base class for scene item UI interface.
/*!

  This base class defines the interface that runtimes can implement to
  provide basic scene item UI behavior. Scene item UI objects can be
  hidden.
*/

class LOOKDEVX_UFE_EXPORT SceneItemUI
{
public:
    using Ptr = std::shared_ptr<SceneItemUI>;

    /*!
        Convenience method that calls the sceneItemUI method on the
        SceneItemUI handler for the item.  Returns a null pointer if the
        argument is null, or has an empty path.
        \param item SceneItem for which SceneItemUI interface is desired.
        \return SceneItemUI interface of the given SceneItem.
    */
    static Ptr sceneItemUI(const Ufe::SceneItem::Ptr& item);

    //! Constructor.
    SceneItemUI() = default;
    //! Destructor.
    virtual ~SceneItemUI() = default;

    //@{
    //! Delete the copy/move constructors assignment operators.
    SceneItemUI(const SceneItemUI&) = default;
    SceneItemUI& operator=(const SceneItemUI&) = delete;
    SceneItemUI(SceneItemUI&&) = delete;
    SceneItemUI& operator=(SceneItemUI&&) = delete;
    //@}

    //! Scene item accessor.
    [[nodiscard]] virtual Ufe::SceneItem::Ptr sceneItem() const = 0;

    //! Return the hidden state of the object.
    //! \return true if the object is hidden
    [[nodiscard]] virtual bool hidden() const = 0;

    //! Create an undoable hidden command.
    [[nodiscard]] virtual Ufe::UndoableCommand::Ptr setHiddenCmd(bool hidden) = 0;

    //! Return the hidden key metadata for the scene item.
    //! Note: For backward compatibility, it may be necessary to set different hidden key metadata based on the scene
    //! item. return the hidden key metadata.
    [[nodiscard]] static std::string getHiddenKeyMetadata(const Ufe::SceneItemPtr& item);
};

} // LOOKDEVXUFE_NS_DEF

#endif
