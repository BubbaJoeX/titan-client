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

#include <ufe/notification.h>
#include <ufe/path.h>

LOOKDEVXUFE_NS_DEF
{

/**
 * @brief Notification to force the recreation of a node to work around: LOOKDEVX-3574.
 */
class LOOKDEVX_UFE_EXPORT ObjectRecreate : public Ufe::Notification
{
public:
    explicit ObjectRecreate(Ufe::Path path);

    ~ObjectRecreate() override;

    //! Returns the path of the object that was recreated.
    [[nodiscard]] Ufe::Path path() const;

private:
    Ufe::Path m_path;
};

} // LOOKDEVXUFE_NS_DEF
