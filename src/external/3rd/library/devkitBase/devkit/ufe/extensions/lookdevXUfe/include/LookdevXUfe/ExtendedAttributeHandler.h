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
#ifndef LOOKDEVXUFE_EXTENDEDATTRIBUTE_HANDLER_H
#define LOOKDEVXUFE_EXTENDEDATTRIBUTE_HANDLER_H

#include "Export.h"

#include <ufe/attribute.h>
#include <ufe/handlerInterface.h>
#include <ufe/runTimeMgr.h>

LOOKDEVXUFE_NS_DEF
{

/*!
  \class ExtendedAttributeHandler
  \brief Factory base class for component connection interface.
*/

class LOOKDEVX_UFE_EXPORT ExtendedAttributeHandler : public Ufe::HandlerInterface
{
public:
    using Ptr = std::shared_ptr<ExtendedAttributeHandler>;

    //! Constructor.
    ExtendedAttributeHandler() = default;
    //! Default copy constructor.
    ExtendedAttributeHandler(const ExtendedAttributeHandler&) = default;
    //! Destructor.
    ~ExtendedAttributeHandler() override = default;

    //@{
    ExtendedAttributeHandler& operator=(const ExtendedAttributeHandler&) = delete;
    ExtendedAttributeHandler(ExtendedAttributeHandler&&) = delete;
    ExtendedAttributeHandler& operator=(ExtendedAttributeHandler&&) = delete;
    //@}

    static constexpr auto id = "ExtendedAttributeHandler";

    static Ptr get(const Ufe::Rtid& rtId);

    /*!
        \brief Checks if an attribute was ever authored.

        \todo LOOKDEVX-2672: This will have to be merged back in base UFE at some point in time. Will start the process.
       Unclear at this time if this will be a function or metadata.

        \param attribute The attribute to query.
        \return true if the attribute is authored.
    */
    [[nodiscard]] virtual bool isAuthoredAttribute(const Ufe::Attribute::Ptr& attribute) const = 0;
};

} // LOOKDEVXUFE_NS_DEF

#endif
