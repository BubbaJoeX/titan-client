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

#include <ufe/attribute.h>
#include <ufe/connection.h>
#include <ufe/handlerInterface.h>
#include <ufe/notification.h>
#include <ufe/runTimeMgr.h>
#include <ufe/sceneItem.h>
#include <ufe/selection.h>
#include <ufe/subject.h>

#include <memory>

LOOKDEVXUFE_NS_DEF
{

//! \brief Provides an interface for soloing scene items.
/*!
  Soloing is a context-dependent concept. For node graph related
  operations it could mean routing the intermediate results of the
  data flow; for geometry primitives it could mean hiding every other
  geometry in the same scene etc. It is also meant to be mutually
  exclusive among related scene items.
*/
class LOOKDEVX_UFE_EXPORT SoloingHandler : public Ufe::HandlerInterface
{
public:
    using Ptr = std::shared_ptr<SoloingHandler>;

    static constexpr auto id = "SoloingHandler";

    static Ptr get(const Ufe::Rtid& rtId);

    //! Returns whether or not the current runtime supports a soloing operation for the given SceneItem.
    [[nodiscard]] virtual bool isSoloable(const Ufe::SceneItem::Ptr& item) const = 0;

    //! Returns whether or not the current runtime supports a soloing operation for the given Attribute.
    [[nodiscard]] virtual bool isSoloable(const Ufe::Attribute::Ptr& attr) const = 0;

    //! Perform a soloing operation on the given SceneItem if supported. Depending on the use-case, an Attribute
    //! may also be implicitly soloed as a result of this operation.
    [[nodiscard]] virtual Ufe::UndoableCommand::Ptr soloCmd(const Ufe::SceneItem::Ptr& item) const = 0;

    //! Perform a soloing operation on the given Attribute if supported. It effectively solos the parent SceneItem.
    [[nodiscard]] virtual Ufe::UndoableCommand::Ptr soloCmd(const Ufe::Attribute::Ptr& attr) const = 0;

    //! Restore the original state before soloing. Soloing always happens in the context of a SceneItem, so
    //! no further information is needed if there was a specific Attribute soloed. Depending on the use-case,
    //! it can also operate higher in the hierarchy of the original soloed item, e.g. restoring the state of
    //! a Material.
    [[nodiscard]] virtual Ufe::UndoableCommand::Ptr unsoloCmd(const Ufe::SceneItem::Ptr& item) const = 0;

    //! Returns whether or not the given SceneItem is soloed.
    [[nodiscard]] virtual bool isSoloed(const Ufe::SceneItem::Ptr& item) const = 0;

    //! Returns whether or not the given Attribute is soloed.
    [[nodiscard]] virtual bool isSoloed(const Ufe::Attribute::Ptr& attr) const = 0;

    //! Returns the potentially soloed SceneItem of the material the given SceneItem belongs to.
    [[nodiscard]] virtual Ufe::SceneItem::Ptr getSoloedItem(const Ufe::SceneItem::Ptr& item) const = 0;

    //! Returns whether or not the given SceneItem contains a soloed descendant (not just immediate child).
    [[nodiscard]] virtual bool hasSoloedDescendant(const Ufe::SceneItem::Ptr& item) const = 0;

    //! Returns the specific soloed Attribute of the given SceneItem if applicable.
    [[nodiscard]] virtual Ufe::Attribute::Ptr getSoloedAttribute(const Ufe::SceneItem::Ptr& item) const = 0;

    //! Returns whether or not the given SceneItem is a soloing internal item.
    [[nodiscard]] virtual bool isSoloingItem(const Ufe::SceneItem::Ptr& item) const = 0;

    //! Returns a potentially replaced connection on the material output by soloing, for the purpose of UI faking.
    [[nodiscard]] virtual Ufe::Connection::Ptr replacedConnection(const Ufe::SceneItem::Ptr& item) const = 0;
};

//! \brief Provides soloing state change notifications, carrying additional information.
class LOOKDEVX_UFE_EXPORT SoloingStateChanged : public Ufe::Notification
{
public:
    explicit SoloingStateChanged(Ufe::SceneItem::Ptr item, bool soloEnabled)
        : m_sceneItem(std::move(item)), m_soloEnabled(soloEnabled)
    {
    }

    ~SoloingStateChanged() override;

    //! Returns the SceneItem that was just soloed or unsoloed.
    [[nodiscard]] Ufe::SceneItem::Ptr sceneItem() const
    {
        return m_sceneItem;
    }

    //! Returns whether soloing was enabled or disabled for the carried SceneItem.
    [[nodiscard]] bool soloEnabled() const
    {
        return m_soloEnabled;
    }

private:
    Ufe::SceneItem::Ptr m_sceneItem;
    bool m_soloEnabled;
};

} // LOOKDEVXUFE_NS_DEF
