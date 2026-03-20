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

#include "AttributeComponentInfo.h"

#include <ufe/attribute.h>
#include <ufe/connection.h>
#include <ufe/handlerInterface.h>
#include <ufe/path.h>
#include <ufe/runTimeMgr.h>
#include <ufe/sceneItem.h>

#include <list>
#include <memory>
#include <variant>

LOOKDEVXUFE_NS_DEF
{

namespace Log
{
// Ufe::Path has full copy/move semantics declared and implemented, Ufe::AttributeComponentInfo has full copy/move
// semantics implicitly implemented, but Connection has deleted its copy/move semantics forcing us to reimplement
// it here.
// TODO(Ufe): Enable default copy/move semantics on Ufe::Connection.
class ConnectionInfo
{
public:
    //! Constructor.
    LOOKDEVX_UFE_EXPORT ConnectionInfo(const AttributeComponentInfo& srcAttr, const AttributeComponentInfo& dstAttr);
    //! Destructor.
    LOOKDEVX_UFE_EXPORT ~ConnectionInfo();

    //@{
    //! Default implementation of copy/move semantics:
    ConnectionInfo(const ConnectionInfo&) = default;
    ConnectionInfo& operator=(const ConnectionInfo&) = delete;
    ConnectionInfo(ConnectionInfo&&) = default;
    ConnectionInfo& operator=(ConnectionInfo&&) = delete;
    //@}

    //! Get the source attribute.
    LOOKDEVX_UFE_EXPORT const AttributeComponentInfo& src() const
    {
        return m_src;
    }
    //! Get the destination attribute.
    LOOKDEVX_UFE_EXPORT const AttributeComponentInfo& dst() const
    {
        return m_dst;
    }

private:
    AttributeComponentInfo m_src;
    AttributeComponentInfo m_dst;
};

class Location
{
public:
    enum PayloadType
    {
        ePath,
        eAttributeComponentInfo,
        eConnectionInfo
    };

    // We do want implicit construction for this simple container class.
    LOOKDEVX_UFE_EXPORT Location(Ufe::Path&& path);                           // NOLINT(google-explicit-constructor)
    LOOKDEVX_UFE_EXPORT Location(LookdevXUfe::AttributeComponentInfo&& attr); // NOLINT(google-explicit-constructor)
    LOOKDEVX_UFE_EXPORT Location(LookdevXUfe::Log::ConnectionInfo&& cnx);     // NOLINT(google-explicit-constructor)

    LOOKDEVX_UFE_EXPORT PayloadType type() const;

    LOOKDEVX_UFE_EXPORT const Ufe::Path& path() const;
    LOOKDEVX_UFE_EXPORT const LookdevXUfe::AttributeComponentInfo& attribute() const;
    LOOKDEVX_UFE_EXPORT const LookdevXUfe::Log::ConnectionInfo& connection() const;

private:
    std::variant<Ufe::Path, LookdevXUfe::AttributeComponentInfo, LookdevXUfe::Log::ConnectionInfo> m_payload;
};
using Locations = std::vector<Location>;

enum class Severity
{
    kInfo,
    kWarning,
    kError,
};

class Entry
{
public:
    LOOKDEVX_UFE_EXPORT Entry(Severity severity, std::string message, Locations locations);

    [[nodiscard]] LOOKDEVX_UFE_EXPORT Severity severity() const
    {
        return m_severity;
    }

    [[nodiscard]] LOOKDEVX_UFE_EXPORT const std::string& message() const
    {
        return m_message;
    }

    [[nodiscard]] LOOKDEVX_UFE_EXPORT const Locations& locations() const
    {
        return m_locations;
    }

private:
    Severity m_severity;
    std::string m_message;
    Locations m_locations;
};

// Allow printing Log components:
LOOKDEVX_UFE_EXPORT
std::ostream& operator<<(std::ostream& out, const Ufe::Path& path);
LOOKDEVX_UFE_EXPORT
std::ostream& operator<<(std::ostream& out, const LookdevXUfe::AttributeComponentInfo& attr);
LOOKDEVX_UFE_EXPORT
std::ostream& operator<<(std::ostream& out, const ConnectionInfo& cnx);
LOOKDEVX_UFE_EXPORT
std::ostream& operator<<(std::ostream& out, const Location& location);
LOOKDEVX_UFE_EXPORT
std::ostream& operator<<(std::ostream& out, const Entry& entry);

} // namespace Log
//! \brief Validation log.
class ValidationLog
{
public:
    using Ptr = std::shared_ptr<ValidationLog>;
    using Entries = std::vector<Log::Entry>;
    using EntriesIndex = std::vector<size_t>;

    LOOKDEVX_UFE_EXPORT static Ptr create();
    LOOKDEVX_UFE_EXPORT ValidationLog();

    ValidationLog(const ValidationLog&) = delete;
    ValidationLog& operator=(const ValidationLog&) = delete;
    ValidationLog(ValidationLog&&) = delete;
    ValidationLog& operator=(ValidationLog&&) = delete;

    LOOKDEVX_UFE_EXPORT void addEntry(Log::Entry);

    [[nodiscard]] LOOKDEVX_UFE_EXPORT const Entries& entries() const;
    [[nodiscard]] LOOKDEVX_UFE_EXPORT const Log::Entry& entry(size_t index) const;

    [[nodiscard]] LOOKDEVX_UFE_EXPORT const EntriesIndex& getIndexForLocation(const Ufe::Path& path,
                                                                              bool includeChildren);
    [[nodiscard]] LOOKDEVX_UFE_EXPORT const EntriesIndex& getIndexForLocation(
        const LookdevXUfe::AttributeComponentInfo& attrCompInfo);
    /*
    Improved UX:
    - In compound editor, useless prefixing should be removed so we end up with node names and
      attributes that are visible in the view: "add.out -> standardSurface1.diffuse" beats
      "world|bob|bobShape,/mtl/old_leather/add.out -> world|bob|bobShape,/mtl/old_leather/standardSurface1.diffuse"
      for simplicity.
    */

private:
    struct Imp;
    std::unique_ptr<Imp> m_impl;
};

// Allow printing the log:
LOOKDEVX_UFE_EXPORT
std::ostream& operator<<(std::ostream& out, const ValidationLog& log);

} // LOOKDEVXUFE_NS_DEF
