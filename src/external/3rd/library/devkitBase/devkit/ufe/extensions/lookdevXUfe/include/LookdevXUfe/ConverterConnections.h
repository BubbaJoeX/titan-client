// =======================================================================
// Copyright 2024 Autodesk, Inc. All rights reserved.
//
// This computer source code and related instructions and comments are the
// unpublished confidential  and proprietary information of Autodesk, Inc.
// and are protected under applicable copyright and trade secret law. They
// may not be disclosed to, copied  or used by any third party without the
// prior written consent of Autodesk, Inc.
// =======================================================================

#ifndef CONVERTER_CONNECTIONS_H
#define CONVERTER_CONNECTIONS_H

#include "ExtendedConnection.h"

LOOKDEVXUFE_NS_DEF
{

/*!
  \class ConverterConnections
  \brief The class represents a list of existing converter connections.
*/
class ConverterConnections
{
public:
    using Ptr = std::shared_ptr<ConverterConnections>;

    //! Constructor.
    LOOKDEVX_UFE_EXPORT explicit ConverterConnections(const Ufe::SceneItem::Ptr& sceneItem);
    //! Destructor.
    LOOKDEVX_UFE_EXPORT virtual ~ConverterConnections();

    //@{
    //! No copy or move constructor/assignment.
    ConverterConnections(const ConverterConnections&) = delete;
    ConverterConnections& operator=(const ConverterConnections&) = delete;
    ConverterConnections(ConverterConnections&&) = delete;
    ConverterConnections& operator=(ConverterConnections&&) = delete;
    //@}

    //! \return Return all the converter connections.
    [[nodiscard]] LOOKDEVX_UFE_EXPORT std::vector<ExtendedConnection::Ptr> allConnections() const;

    //! \return Return all converter connections with the specified destination attribute
    [[nodiscard]] LOOKDEVX_UFE_EXPORT std::vector<ExtendedConnection::Ptr> connections(
        const Ufe::Attribute::Ptr& dstAttr) const;

    LOOKDEVX_UFE_EXPORT static ConverterConnections::Ptr create(const Ufe::SceneItem::Ptr& item);

private:
    Ufe::SceneItem::Ptr m_sceneItem;
};

} // LOOKDEVXUFE_NS_DEF

#endif
