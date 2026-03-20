// =======================================================================
// Copyright 2024 Autodesk, Inc. All rights reserved.
//
// This computer source code and related instructions and comments are the
// unpublished confidential  and proprietary information of Autodesk, Inc.
// and are protected under applicable copyright and trade secret law. They
// may not be disclosed to, copied  or used by any third party without the
// prior written consent of Autodesk, Inc.
// =======================================================================

#ifndef EXTENDED_CONNECTION_H
#define EXTENDED_CONNECTION_H

#include "AttributeComponentInfo.h"

LOOKDEVXUFE_NS_DEF
{

/*!
  \class ExtendedConnection
  \brief This class represents an extended connection, e.g. a "regular" connection, a component connection, etc.
*/
class LOOKDEVX_UFE_EXPORT ExtendedConnection
{
public:
    using Ptr = std::shared_ptr<ExtendedConnection>;

    //! Constructor.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    ExtendedConnection(AttributeComponentInfo srcAttr, AttributeComponentInfo dstAttr)
        : m_src(std::move(srcAttr)), m_dst(std::move(dstAttr))
    {
    }

    //! Destructor.
    virtual ~ExtendedConnection();

    //@{
    //! No copy or move constructor/assignment.
    ExtendedConnection(const ExtendedConnection&) = delete;
    ExtendedConnection& operator=(const ExtendedConnection&) = delete;
    ExtendedConnection(ExtendedConnection&&) = delete;
    ExtendedConnection& operator=(ExtendedConnection&&) = delete;

    //@}

    static Ptr create(const AttributeComponentInfo& srcAttr, const AttributeComponentInfo& dstAttr)
    {
        return std::make_shared<ExtendedConnection>(srcAttr, dstAttr);
    }

    //! Get the source attribute and component.
    const AttributeComponentInfo& src() const
    {
        return m_src;
    }

    //! Get the destination attribute and component.
    const AttributeComponentInfo& dst() const
    {
        return m_dst;
    }

private:
    const AttributeComponentInfo m_src, m_dst;
};

} // LOOKDEVXUFE_NS_DEF

#endif // EXTENDED_CONNECTION_H
