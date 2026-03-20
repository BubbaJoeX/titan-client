// =======================================================================
// Copyright 2024 Autodesk, Inc. All rights reserved.
//
// This computer source code and related instructions and comments are the
// unpublished confidential  and proprietary information of Autodesk, Inc.
// and are protected under applicable copyright and trade secret law. They
// may not be disclosed to, copied  or used by any third party without the
// prior written consent of Autodesk, Inc.
// =======================================================================

#ifndef EXTENDED_CONNECTION_HANDLER_H
#define EXTENDED_CONNECTION_HANDLER_H

#include "ComponentConnections.h"
#include "ConverterConnections.h"
#include "UndoableCommand.h"

#include <ufe/attribute.h>
#include <ufe/handlerInterface.h>
#include <ufe/runTimeMgr.h>

LOOKDEVXUFE_NS_DEF
{

/*!
  \class ExtendedConnectionHandler
  \brief Factory base class for extended connection interface.
*/

class LOOKDEVX_UFE_EXPORT ExtendedConnectionHandler : public Ufe::HandlerInterface
{
public:
    using Ptr = std::shared_ptr<ExtendedConnectionHandler>;

    //! Constructor.
    ExtendedConnectionHandler() = default;
    //! Default copy constructor.
    ExtendedConnectionHandler(const ExtendedConnectionHandler&) = default;
    //! Destructor.
    ~ExtendedConnectionHandler() override = default;

    //@{
    ExtendedConnectionHandler& operator=(const ExtendedConnectionHandler&) = delete;
    ExtendedConnectionHandler(ExtendedConnectionHandler&&) = delete;
    ExtendedConnectionHandler& operator=(ExtendedConnectionHandler&&) = delete;
    //@}

    static constexpr auto id = "ExtendedConnectionHandler";

    static Ptr get(const Ufe::Rtid& rtId);

    /*!
        \brief Get the source component connections on the given SceneItem.

        \param item SceneItem to use to retrieve its source component connections.
        \return all the component connections of given SceneItem and a null pointer
        if no component connections interface can be created for the item.
    */
    [[nodiscard]] virtual ComponentConnections::Ptr sourceComponentConnections(
        const Ufe::SceneItem::Ptr& item) const = 0;

    /*!
        \brief Get the source adsk converter connections on the given SceneItem.

        \param item SceneItem to use to retrieve its source adsk converter connections.
        \return all the adsk converter connections of given SceneItem.
    */
    [[nodiscard]] static ConverterConnections::Ptr sourceAdskConverterConnections(const Ufe::SceneItem::Ptr& item);

    /*!
      \brief Get the command to create a connection. The command handles all the supported types of connections,
      e.g. "regular" connections, components connections, etc. The returned command is not executed.

      \param srcAttr source attribute to be connected.
      \param srcComponent component to connect (empty string means use the attribute directly)
      \param dstAttr destination attribute to be connected.
      \param dstComponent component to connect (empty string means use the attribute directly)
      \returns a command that, when executed, will connect the attributes/components and return the
      resulting component connection.
    */
    [[nodiscard]] virtual std::shared_ptr<CreateConnectionResultCommand> createConnectionCmd(
        const Ufe::Attribute::Ptr& srcAttr,
        const std::string& srcComponent,
        const Ufe::Attribute::Ptr& dstAttr,
        const std::string& dstComponent) const = 0;

    /*!
      \brief Get the command to delete a connection. The command handles all the supported types of connections,
      e.g. "regular" connections, components connections, etc. The returned command is not executed.

      \param srcAttr source attribute to be disconnected.
      \param srcComponent component to disconnect (empty string means use the attribute directly)
      \param dstAttr destination attribute to be disconnected.
      \param dstComponent component to disconnect (empty string means use the attribute directly)
      \returns a command that, when executed, will disconnect the attributes/components
    */
    [[nodiscard]] virtual std::shared_ptr<DeleteConnectionCommand> deleteConnectionCmd(
        const Ufe::Attribute::Ptr& srcAttr,
        const std::string& srcComponent,
        const Ufe::Attribute::Ptr& dstAttr,
        const std::string& dstComponent) const = 0;

    /*!
      \brief Checks if a scene item can have external connections.

      "External connection" means connection to a sibling item. If this method returns false, the item can only be
      connected to child items. This is the case for USD materials and MaterialX documents for example: They can have
      inputs and outputs that are connected to attributes on child-items, but they cannot be connected to any siblings.

      \param item The scene item to query.
      \return true if the item can have external connections i.e., if it can be connected to sibling items.
    */
    [[nodiscard]] virtual bool canHaveExternalConnections(const Ufe::SceneItem::Ptr& /*item*/) const
    {
        return true;
    }
};

} // LOOKDEVXUFE_NS_DEF

#endif // EXTENDED_CONNECTION_HANDLER_H
