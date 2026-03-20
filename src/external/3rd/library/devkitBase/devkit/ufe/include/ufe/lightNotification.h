#ifndef _ufeLightNotification
#define _ufeLightNotification
// ===========================================================================
// Copyright 2022 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
// ===========================================================================

#include "notification.h"
#include "sceneItem.h"
#include <set>
#include <string>

UFE_NS_DEF {

//! \brief Base class for all Light notifications.
/*!

  This class is the base class for all Light changed notifications.
*/

class UFE_SDK_DECL LightChanged: public Notification
{
public:
    //! Constructor.
    //! \param item LightChanged Notification on the given SceneItem.
    LightChanged(const SceneItem::Ptr& item);
    //! Default copy constructor.
    LightChanged(const LightChanged&) = default;
    //! Destructor.
    ~LightChanged() override;
    //! \return SceneItem of this Notification.
    SceneItem::Ptr item() const;

private:
    const SceneItem::Ptr fItem;
};

//! \brief Light2 metadata changed notification.
/*!

  An object of this class is sent to observers when the metadata of an Light2
  on a single node has changed.
*/
class UFE_SDK_DECL Light2MetadataChanged : public LightChanged
{
public:
    //! Constructor.
    //! \param item LightChanged Notification on the given SceneItem.
    //! \param key A single metadata key that had it's value changed.
    Light2MetadataChanged(const SceneItem::Ptr& item, const std::string& key);
    //! Constructor.
    //! \param item LightChanged Notification on the given SceneItem.
    //! \param keys The metadata keys that had values changed.
    Light2MetadataChanged(const SceneItem::Ptr& item, const std::set<std::string>& keys);
    //! Default copy constructor.
    Light2MetadataChanged(const Light2MetadataChanged&) = default;
    //! Destructor.
    ~Light2MetadataChanged() override;

    //! \return Whether this notification was sent for the provided metadata key.
    bool has(const std::string& key) const;

    //! \return Metadata keys that were changed for this Notification.
    const std::set<std::string>& keys() const;

private:
    const std::set<std::string> fKeys;
};

}

#endif /* _ufeLightNotification */
