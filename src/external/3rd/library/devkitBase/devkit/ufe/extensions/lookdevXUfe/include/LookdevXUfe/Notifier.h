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

#include <ufe/subject.h>

LOOKDEVXUFE_NS_DEF
{

/**
 * @brief Singleton Subject to provide LookdevX specific notifications.
 *
 * This subject is used for soloing and for rendering notification.
 */
class LOOKDEVX_UFE_EXPORT Notifier : public Ufe::Subject
{
public:
    static Notifier& instance();
    ~Notifier() override;
};

} // LOOKDEVXUFE_NS_DEF
