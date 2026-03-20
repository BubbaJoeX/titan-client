#ifndef UFE_COLOR_MANAGEMENT_NOTIFICATION_H
#define UFE_COLOR_MANAGEMENT_NOTIFICATION_H

// =======================================================================
// Copyright 2025 Autodesk, Inc. All rights reserved.
//
// This computer source code and related instructions and comments are the
// unpublished confidential  and proprietary information of Autodesk, Inc.
// and are protected under applicable copyright and trade secret law. They 
// may not be disclosed to, copied  or used by any third party without the 
// prior written consent of Autodesk, Inc.
// =======================================================================

#include "notification.h"

UFE_NS_DEF {

//! \brief Color management changed notification.
/*!

  An object of this class is sent to observers when color management
  settings have changed.
*/

class UFE_SDK_DECL ColorManagementChanged: public Notification
{
public:
    enum class Reason {
        PrefsReloaded,
        EnableChanged,
        ConfigChanged,
        WorkingSpaceChanged,
        ViewTransformChanged,
    };

    //! Constructor.
    ColorManagementChanged(Reason c);
    //! Default copy constructor.
    ColorManagementChanged(const ColorManagementChanged&) = default;
    //! Destructor.
    ~ColorManagementChanged() override;

    Reason reason() const;

private:
    Reason fReason;
};

}

#endif /* _ufeColorManagementNotification */
