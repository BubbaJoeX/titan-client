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

#include <memory>

#include <ufe/handlerInterface.h>
#include <ufe/runTimeMgr.h>

LOOKDEVXUFE_NS_DEF
{

//! \brief Provides an interface for runtime specific capabilities.

class LOOKDEVX_UFE_EXPORT CapabilityHandler : public Ufe::HandlerInterface
{
public:
    using Ptr = std::shared_ptr<CapabilityHandler>;

    static constexpr auto id = "CapabilityHandler";

    enum class Capability
    {
        kCanPromoteToMaterial,
        kCanPromoteInputAtTopLevel,
        kCanHaveNestedNodeGraphs,
        kCanUseCreateShaderCommandForComponentConnections
    };

    static Ptr get(const Ufe::Rtid& rtId);

    /*!
        \brief Returns true if the runtime supports the capability.
        \param capability Capability to check.
    */
    [[nodiscard]] virtual bool hasCapability(Capability capability) const = 0;
};

} // LOOKDEVXUFE_NS_DEF
