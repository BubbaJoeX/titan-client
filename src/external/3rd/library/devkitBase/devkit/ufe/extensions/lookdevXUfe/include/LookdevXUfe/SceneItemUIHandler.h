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
#ifndef SCENE_ITEM_UI_HANDLER_H
#define SCENE_ITEM_UI_HANDLER_H

#include "SceneItemUI.h"

#include <memory>

#include <ufe/handlerInterface.h>
#include <ufe/runTimeMgr.h>

LOOKDEVXUFE_NS_DEF
{

//! \brief Factory base class for SceneItemUI interface.
/*!

  This base class defines an interface for factory objects that runtimes
  can implement to create a SceneItemUI interface object.
*/
class LOOKDEVX_UFE_EXPORT SceneItemUIHandler : public Ufe::HandlerInterface
{
public:
    using Ptr = std::shared_ptr<SceneItemUIHandler>;

    static constexpr auto id = "LookdevXHandler_SceneItemUI";

    static Ptr get(const Ufe::Rtid& rtId);

    /*!
        Creates a SceneItemUI interface on the given SceneItem.
        \param item SceneItem to use to retrieve its SceneItemUI interface.
        \return SceneItemUI interface of given SceneItem. Returns a null
        pointer if no SceneItemUI interface can be created for the item.
    */
    [[nodiscard]] virtual LookdevXUfe::SceneItemUI::Ptr sceneItemUI(const Ufe::SceneItem::Ptr& item) const = 0;
};

} // LOOKDEVXUFE_NS_DEF

#endif
