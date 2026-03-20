// =======================================================================
// Copyright 2024 Autodesk, Inc. All rights reserved.
//
// This computer source code and related instructions and comments are the
// unpublished confidential  and proprietary information of Autodesk, Inc.
// and are protected under applicable copyright and trade secret law. They
// may not be disclosed to, copied  or used by any third party without the
// prior written consent of Autodesk, Inc.
// =======================================================================

#ifndef COMPONENT_CONNECTIONS_H
#define COMPONENT_CONNECTIONS_H

#include "ExtendedConnection.h"

#include <ufe/connections.h>

LOOKDEVXUFE_NS_DEF
{

/*!
  \class ComponentConnections
  \brief The class represents a list of existing component connections.
*/
class LOOKDEVX_UFE_EXPORT ComponentConnections
{
public:
    using Ptr = std::shared_ptr<ComponentConnections>;

    //! Constructor.
    explicit ComponentConnections(const Ufe::SceneItem::Ptr& sceneItem);
    //! Destructor.
    virtual ~ComponentConnections();

    //@{
    //! No copy or move constructor/assignment.
    ComponentConnections(const ComponentConnections&) = delete;
    ComponentConnections& operator=(const ComponentConnections&) = delete;
    ComponentConnections(ComponentConnections&&) = delete;
    ComponentConnections& operator=(ComponentConnections&&) = delete;
    //@}

    //! \return Return all the component connections.
    [[nodiscard]] virtual std::vector<ExtendedConnection::Ptr> allConnections() const;

    [[nodiscard]] virtual std::vector<std::string> componentNames(const Ufe::Attribute::Ptr& attr) const = 0;

    //! \return Return all component connections with the specified destination attribute
    [[nodiscard]] std::vector<ExtendedConnection::Ptr> connections(const Ufe::Attribute::Ptr& dstAttr) const;

protected:
    [[nodiscard]] virtual Ufe::Connections::Ptr connections(const Ufe::SceneItem::Ptr& sceneItem) const = 0;

private:
    Ufe::SceneItem::Ptr m_sceneItem;
};

} // LOOKDEVXUFE_NS_DEF

#endif
