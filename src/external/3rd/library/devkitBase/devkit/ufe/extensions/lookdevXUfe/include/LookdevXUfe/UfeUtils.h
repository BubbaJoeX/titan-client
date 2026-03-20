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

#ifndef LOOKDEVX_UFE_UTILS_H
#define LOOKDEVX_UFE_UTILS_H

#include "ConverterConnections.h"
#include "Export.h"
#include "ExtendedConnection.h"

#include <ufe/attribute.h>
#include <ufe/connections.h>
#include <ufe/hierarchy.h>
#include <ufe/nodeDef.h>
#include <ufe/sceneItem.h>
#include <ufe/sceneItemList.h>

#include <string>
#include <unordered_set>

LOOKDEVXUFE_NS_DEF
{

enum class UfeLdxType
{
    eInvalid,
    eBool,
    eInt,
    eFloat,
    eDouble,
    eString,
    eColorFloat3,
    eColorFloat4,
    eFilename,
    eEnumString,
    eInt3,
    eFloat2,
    eFloat3,
    eFloat4,
    eDouble3,
    eMatrix3d,
    eMatrix4d,
    eGeneric,
    eUInt
};

struct UfeRuntimeInfo
{
    std::string m_ufeRuntimeName;
    std::string m_niceName;
    std::string m_iconPath;
};

/**
 * Namespace for UFE utility functions.
 */
namespace UfeUtils
{

/**
 * @brief Returns UFE LookdevX Type for a given UFE attribute.
 * @param ufeAttr The provided UFE attribute.
 * @return The type of the provided UFE attribute.
 */
LOOKDEVX_UFE_EXPORT
UfeLdxType getUfeLdxType(const Ufe::Attribute& ufeAttr);

/**
 * @brief Checks if \p item is of Scope type.
 * @param item A SceneItem to inspect.
 * @return True if the given SceneItem is of Scope type.
 */
LOOKDEVX_UFE_EXPORT
bool isScopeItem(const Ufe::SceneItem::Ptr& item);

/**
 * @brief Checks if \p item is of Material type.
 * @param item A SceneItem to inspect.
 * @return True if the given SceneItem is of Material type.
 */
LOOKDEVX_UFE_EXPORT
bool isMaterialItem(const Ufe::SceneItem::Ptr& item);

/**
 * @brief Checks that the given \p item is of NodeGraph type.
 * @param item A SceneItem to inspect.
 * @return True if the given SceneItem is of NodeGraph type.
 */
LOOKDEVX_UFE_EXPORT
bool isNodeGraphItem(const Ufe::SceneItem::Ptr& item);

/**
 * @brief Checks if \p item is of a shading-related type, that can be handled by LookdevX.
 * E.g. Material, Shader, Nodegraph.
 * @param item A SceneItem to inspect.
 * @return True if the given SceneItem is shading-related.
 */
LOOKDEVX_UFE_EXPORT
bool isLookdevSceneItem(const Ufe::SceneItem::Ptr& item);

/**
 * @brief This utility function is missing in core UFE.
 * @return True if the runtime with the given name exists.
 */
LOOKDEVX_UFE_EXPORT
bool hasRuntimeName(const std::string& runtimeName);

/**
 * @brief Get the Runtime Id, starting from the Runtime name of a Data Model.
 *
 * @param runtimeName used to fetch the corresponding id.
 * @return the target Ufe::Rtid
 */
LOOKDEVX_UFE_EXPORT
Ufe::Rtid getRuntimeId(const std::string& runtimeName);

/**
 * @brief Get the Runtime name for a given Runtime Id
 *
 * @param runtimeId used to fetch the corresponding runtime name.
 * @return the name of the runtime for a given runtime id
 */
LOOKDEVX_UFE_EXPORT
std::string getUfeRuntimeName(Ufe::Rtid runtimeId);

/**
 * @exception Ufe::InvalidRunTimeName Thrown if the Maya runtime does not exist.
 * @return The runtime ID of the Maya runtime.
 */
LOOKDEVX_UFE_EXPORT
Ufe::Rtid getMayaRuntimeId();

/**
 * @exception Ufe::InvalidRunTimeName Thrown if the USD runtime does not exist.
 * @return The runtime ID of the USD runtime.
 */
LOOKDEVX_UFE_EXPORT
Ufe::Rtid getUsdRuntimeId();

/**
 * @exception Ufe::InvalidRunTimeName Thrown if the MaterialX runtime does not exist.
 * @return The runtime ID of the MaterialX runtime.
 */
LOOKDEVX_UFE_EXPORT
Ufe::Rtid getMaterialXRuntimeId();

/**
 * @brief Get information about the supported UFE runtimes.
 *
 * @return A list of structs containing information about the supported UFE runtimes.
 */
LOOKDEVX_UFE_EXPORT
const std::vector<UfeRuntimeInfo>& getRuntimeInfos();

/**
 * @brief Get information about the specified UFE runtime.
 *
 * @param runtimeId UFE runtime to retrieve the UfeRuntimeInfo struct for.
 * @return A struct containing information about the specified runtime. If the runtime does not exist, an empty struct
 * is returned, i.e. all contained strings will be empty.
 */
LOOKDEVX_UFE_EXPORT
UfeRuntimeInfo getRuntimeInfo(Ufe::Rtid runtimeId);

/**
 * @brief Get information about the specified UFE runtime.
 *
 * @param runtimeName UFE runtime to retrieve the UfeRuntimeInfo struct for.
 * @return A struct containing information about the specified runtime. If the runtime does not exist, an empty struct
 * is returned, i.e. all contained strings will be empty.
 */
LOOKDEVX_UFE_EXPORT
UfeRuntimeInfo getRuntimeInfo(const std::string& runtimeName);

/**
 * @brief Checks if \p item has valid Runtime Id, meaning it belongs to the correct Data Model.
 *
 * @param item A SceneItem to inspect.
 * @param runtimeName identifying the runtime to which the item is desired to belong to.
 * @return true if the runtime matching criterion is valid.
 */
LOOKDEVX_UFE_EXPORT
bool itemBelongsToRuntime(const Ufe::SceneItem::Ptr& item, const std::string& runtimeName);

//! overload
LOOKDEVX_UFE_EXPORT
bool itemBelongsToRuntime(const Ufe::SceneItem::Ptr& item, Ufe::Rtid runtimeId);

/**
 * @brief Filter \p sceneItemList scene items by Runtime (Data Model), identified by its \p runtimeId.
 *
 * @param sceneItemList The queried input list.
 * @param runtimeId identifying the runtime to which the items are desired to belong to.
 * @param itemsInRuntime output list of valid items.
 * @param itemsNotInRuntime output list of invalid items.
 * @return true if all queried items belong to the desired runtime.
 */
LOOKDEVX_UFE_EXPORT
bool filterItemsByRuntime(const Ufe::SceneItemList& sceneItemList,
                          Ufe::Rtid runtimeId,
                          Ufe::SceneItemList& itemsInRuntime,
                          Ufe::SceneItemList& itemsNotInRuntime);

/**
 * @brief Group the items by their Runtime.
 * @param items The item to group.
 * @return The grouped items by their runtimeId.
 */
[[nodiscard]] LOOKDEVX_UFE_EXPORT std::unordered_map<Ufe::Rtid, Ufe::Selection> groupItemsByRuntime(
    const Ufe::Selection& items);

/**
 * @brief Group the items by their Runtime. If they are gateway items,use their nested runtimeId.
 * @param items The item to group.
 * @return The grouped items by their runtimeId.
 */
[[nodiscard]] LOOKDEVX_UFE_EXPORT std::unordered_map<Ufe::Rtid, Ufe::Selection> groupItemsByNestedRuntime(
    const Ufe::Selection& items);

/// The unique location of the input prefix string:
LOOKDEVX_UFE_EXPORT
const std::string& getInputPrefix();

/// The unique location of the output prefix string:
LOOKDEVX_UFE_EXPORT
const std::string& getOutputPrefix();

enum class AttributeIOType
{
    INPUT,  // NOLINT
    OUTPUT, // NOLINT
    UNKNOWN // NOLINT
};

/// Get the direction and vnn name of a Ufe attribute:
LOOKDEVX_UFE_EXPORT
AttributeIOType getPortInfoFromUfeName(const std::string& ufeName);
LOOKDEVX_UFE_EXPORT
AttributeIOType getPortInfoFromUfeName(const std::string& ufeName, std::string* vnnName);

/**
 * Searches the list of children of the SceneItem `item` for a child named `childName`.
 *
 * @param item      SceneItem whose children will be searched for `childName`.
 * @param childName Name of child that will searched for.
 * @return A pointer to the child if found. A nullptr otherwise.
 */
LOOKDEVX_UFE_EXPORT
Ufe::SceneItem::Ptr findChild(const Ufe::SceneItem::Ptr& item, const std::string& childName);

/**
 * @brief Finds or creates a lookdev environment i.e., an item that can serve as the parent of a lookdev container in
          the specified runtime.
 * @param hostRuntimeId ID of the Host DCC runtime.
 * @param dataRuntimeId ID of the Data Model runtime.
 * @param useSelectionForMultipleGateways When set it will use a gateway among multiple ones from the global selection.
 * @return An item that can serve as the parent of a lookdev container in the specified runtime. Can be nullptr.
 * @throws std::runtime_error if the lookdev environment creation fails when selection is used for multiple gateways.
 */
LOOKDEVX_UFE_EXPORT
Ufe::SceneItem::Ptr createLookdevEnvironment(Ufe::Rtid hostRuntimeId,
                                             Ufe::Rtid dataRuntimeId,
                                             bool useSelectionForMultipleGateways = true);

//! scene getters

/**
 * @brief Filter the Lookdev items (materials, shaders, sceneGraphs) in the selection.
 * @param selection The selection to filter.
 * @return The filtered selection.
 */
LOOKDEVX_UFE_EXPORT
Ufe::Selection getLookdevXItems(const Ufe::Selection& selection);

/**
 * @brief Filter the Lookdev items (materials, shaders, sceneGraphs) from the Global Selection.
 * @return The filtered selection.
 */
LOOKDEVX_UFE_EXPORT
Ufe::Selection getGlobalSelectionLookdevXItems();

/**
 * @brief Filter the Scope items in the selection.
 * @param selection The selection to filter.
 * @return The filtered selection.
 */
LOOKDEVX_UFE_EXPORT
Ufe::Selection getScopeItems(const Ufe::Selection& selection);

/**
 * @brief Filter the DocumentStack items in the selection.
 * @param selection The selection to filter.
 * @return The filtered selection.
 */
LOOKDEVX_UFE_EXPORT
Ufe::Selection getGatewayItems(const Ufe::Selection& selection);

/**
 * @brief Check if the specified path is a gateway into the specified runtime.
 * @param path The path to check.
 * @param rtid The rtid to check.
 * @return True if it is a gateway.
 */
LOOKDEVX_UFE_EXPORT
bool isGateway(const Ufe::Path& path, Ufe::Rtid rtid);

/**
 * @brief If \p gatewayPath is a gateway, returns the ID of the runtime it is a gateway to.
 * @param gatewayPath The path to check.
 * @return The ID of the runtime it serves as a gateway to.
 */
LOOKDEVX_UFE_EXPORT
Ufe::Rtid getNestedRuntimeId(const Ufe::Path& gatewayPath);

/**
 * @brief Get Lookdev items (materials, shaders, sceneGraphs) related to the given \p sceneItem
 * @param sceneItem The SceneItem to inspect.
 * @return Ufe::SceneItemList, the list of Lookdev items.
 */
LOOKDEVX_UFE_EXPORT
Ufe::SceneItemList getLookdevItems(const Ufe::SceneItem::Ptr& sceneItem);

/**
 * @brief Get materials assigned to the given \p sceneItem. The UFE material interface is used to return all the
 * materials assigned to the queried scene item. A scene item may not have a material interface associated to.
 * @param sceneItem queried SceneItem.
 * @return Ufe::SceneItemList, the list of materials.
 */
LOOKDEVX_UFE_EXPORT
Ufe::SceneItemList getAssignedMaterials(const Ufe::SceneItem::Ptr& sceneItem);

/**
 * @brief Get materials related to the given \p sceneItem. Any found will be appended
 * to the \p sceneItemList.
 * NOTE: This is different than UfeUtils::getAssignedMaterials.
 * @param sceneItem The SceneItem to inspect.
 * @return Ufe::SceneItemList, the list of materials.
 */
LOOKDEVX_UFE_EXPORT
Ufe::SceneItemList getMaterialsFromSceneItem(const Ufe::SceneItem::Ptr& sceneItem);

/**
 * @brief Get materials related to the given \p sceneItem and its children. Any found will be appended
 * to the \p sceneItemList.
 * Recursive version of the previous method.
 * @param sceneItem The SceneItem to inspect.
 * @param sceneItemList Result list items are appended to.
 */
LOOKDEVX_UFE_EXPORT
void getMaterialsFromSceneItemRecursive(const Ufe::SceneItem::Ptr& currentSceneItem, Ufe::SceneItemList& sceneItemList);

//! scene queries

/**
 * @brief Check if \p sceneItem has any Lookdev items (materials, shaders, sceneGraphs) related to.
 *
 * @param sceneItem queried SceneItem
 */
LOOKDEVX_UFE_EXPORT
bool hasLookdevItems(const Ufe::SceneItem::Ptr& sceneItem);

/**
 * @brief Check if \p sceneItem has any materials assigned to.
 * @param sceneItem queried SceneItem.
 * @return The result may be true only for any prim type to which a material may be assigned to.
 */
LOOKDEVX_UFE_EXPORT
bool hasAssignedMaterials(const Ufe::SceneItem::Ptr& sceneItem);

/**
 * @brief Check if \p currentSceneItem has any children with any materials assigned to. Recursive version.
 * @param currentSceneItem The root SceneItem to inspect.
 */
LOOKDEVX_UFE_EXPORT
bool hasAssignedMaterialsRecursive(const Ufe::SceneItem::Ptr& currentSceneItem);

/**
 * @brief Check if \p sceneItem has any children materials.
 * @param sceneItem The SceneItem to inspect.
 */
LOOKDEVX_UFE_EXPORT
bool hasChildrenMaterials(const Ufe::SceneItem::Ptr& sceneItem);

/**
 * @brief Check if \p currentSceneItem has any children materials. Recursive version.
 * @param currentSceneItem The root SceneItem to inspect.
 */
LOOKDEVX_UFE_EXPORT
bool hasChildrenMaterialsRecursive(const Ufe::SceneItem::Ptr& currentSceneItem);

/**
 * @brief Check if \p sceneItem support material assignment. The check is performed using the runtime-specific
 * implementation of the corresponding UFE utility.
 * @param sceneItem queried SceneItem.
 */
// MAYA-128782/MAYA-128784
// bool canAssignMaterialToSceneItem(const Ufe::SceneItem::Ptr& sceneItem);

/**
 * Set metadata attribute.
 *
 * @param attr The attribute.
 * @param metadataType The metadata type we want to update.
 * @param metadataValue The new metadata value to set.
 * @throws std::runtime_error if setting the metadata fails.
 */
LOOKDEVX_UFE_EXPORT
void setMetadata(Ufe::Attribute::Ptr& attr, const std::string& metadataType, const std::string& metadataValue);

/**
 * @brief Test the attribute metadata as a flag, checking whether it holds a value of 0 or 1.
 * @param attr The attribute.
 * @param metadataString The metadata string to test.
 * @return True if the metadata is present and holds a value of 1, false otherwise.
 */
LOOKDEVX_UFE_EXPORT
bool testMetadataFlag(Ufe::Attribute::Ptr& attr, const char* metadataString);

/**
 * @brief Get the node definition of the given scene item.
 * @param sceneItem SceneItem corresponding to the node.
 * @return A valid node definition if it exists, or nullptr.
 */
LOOKDEVX_UFE_EXPORT
Ufe::NodeDef::Ptr getNodeDef(const Ufe::SceneItem::Ptr& sceneItem);

/**
 * @brief Get a Ufe::CompositeUndoableCommand consisting of the union of commands that translate the scene items (via
 * their Ufe::UINodeGraphNode) by \p ufeTranslation. The returned Ufe::CompositeUndoableCommand is not executed.
 * @param items The items to translate.
 * @param ufeTranslation The ufe translation vector to add to the current scene item Ufe::UINodeGraphNode position.
 * @return Ufe::CompositeUndoableCommand::Ptr made by the union of the translate cmds.
 */
LOOKDEVX_UFE_EXPORT
Ufe::CompositeUndoableCommand::Ptr translateSceneItemsCmd(const Ufe::SceneItemList& items,
                                                          const Ufe::Vector2f& ufeTranslation);

/**
 * @brief Get the position of the first scene item that has an Ufe::UINodeGraphNode with a valid position.
 * @param items The scene items to check.
 * @return The first valid position.
 */
LOOKDEVX_UFE_EXPORT
Ufe::Vector2f getFirstValidPosition(const Ufe::SceneItemList& items);

/**
 * @brief Return the hidden state of the object that correponds to the Ufe path.
 * @return true if the object is hidden.
 */
LOOKDEVX_UFE_EXPORT
bool isPathHidden(const Ufe::Path& path);

/**
 * @brief Return the hidden state of the object (input Ufe SceneItem).
 * @return true if the object is hidden.
 */
LOOKDEVX_UFE_EXPORT
bool isItemHidden(const Ufe::SceneItem::Ptr& item);

/**
 * @brief Returns whether the connection is hidden. Hidden connections have source and/or destination
 * attributes belonging to hidden scene items.
 * @param connection The connection to query for being hidden
 * @return True if the connection is hidden.
 */
LOOKDEVX_UFE_EXPORT
bool isConnectionHidden(const Ufe::Connection::Ptr& connection);

/**
 * @brief Returns whether the extended connection is hidden. Hidden connections have source and/or destination
 * attributes belonging to hidden scene items.
 * @param connection The connection to query for being hidden
 * @return True if the connection is hidden.
 */
LOOKDEVX_UFE_EXPORT
bool isExtendedConnectionHidden(const LookdevXUfe::ExtendedConnection::Ptr& connection);

/**
 * @brief Returns whether the extended connection is internal. An internal connection is a connection
 * from a compound to one of its children.
 * @param connection The connection to query for being internal
 * @return True if the connection is internal.
 */
LOOKDEVX_UFE_EXPORT
bool isInternalConnection(const LookdevXUfe::ExtendedConnection::Ptr& connection);

/**
 * @brief Get the separate and combine (sub-components) items that could be found in between the items in the
 * selection.
 * @param selection The selection to check.
 * @return The in-between sub-components items.
 */
LOOKDEVX_UFE_EXPORT
std::unordered_set<Ufe::SceneItem::Ptr> getInBetweenSeparateAndCombineItems(const Ufe::Selection& selection);

/**
 * @brief Serializes the component types for a UFE attribute to a string vector.
 * @param attr attribute we want the component types for as strings.
 * @return vector of strings containing the component type names.
 */
LOOKDEVX_UFE_EXPORT
std::vector<std::string> attributeComponentsAsStrings(const Ufe::Attribute::Ptr& attr);

LOOKDEVX_UFE_EXPORT
Ufe::Attribute::Ptr getFirstOutput(const Ufe::SceneItem::Ptr& item);

// Recursively search through compound connections for the source of the given attribute.
// Stops until either a non compound attribute is found or no connection at all.
LOOKDEVX_UFE_EXPORT
Ufe::Attribute::Ptr getConnectedSource(const Ufe::Attribute::Ptr& attr);

/**
 * @brief Get the list of classifications for the given scene item.
 * @param sceneItem The scene item to get the classifications for.
 * @return The list of classifications.
 */
LOOKDEVX_UFE_EXPORT
std::vector<std::string> getNodeClassifications(const Ufe::SceneItem::Ptr& sceneItem);

/**
 * @brief Get the library name for the given scene item. The library name is the last item in the list of
 * classifications.
 * @param sceneItem The scene item to get the library name for.
 * @return The library name.
 */
LOOKDEVX_UFE_EXPORT
std::string getNodeLibraryName(const Ufe::SceneItem::Ptr& sceneItem);

/*
 * @brief If found, replace the item with ufePath \p itemPathToReplace in the global selection with \p itemToAdd.
 * @note TODO(LOOKDEVX-2335): This function should be removed once the global selection (e.g. for renaming and
 * reparenting) is properly updated by UFE.
 * @param itemToAdd The item to add to the global selection.
 * @param itemPathToReplace The path of the item to remove in the global selection.
 */
LOOKDEVX_UFE_EXPORT
void replaceItemInGlobalSelection(const Ufe::SceneItem::Ptr& itemToAdd, const Ufe::Path& itemPathToReplace);

// Returns the parent lookdev container that owns the shader item. Returns item if item is a lookdev container,
// or nullptr if item is not descendant of a container in scene hierarchy.
LOOKDEVX_UFE_EXPORT
Ufe::SceneItem::Ptr getLookdevContainer(const Ufe::SceneItem::Ptr& item);

// Returns the position where the UDIM tile index begins, if found. Will return std::string::npos if there is no UDIM
// pattern in the filename.
LOOKDEVX_UFE_EXPORT
size_t findUdimPositionInFilename(const std::string& filename);

// Finds a UDIM index in the file name and replace it with the <UDIM> tag if applicable. Returns the original filename
// if no UDIM index found.
LOOKDEVX_UFE_EXPORT
std::string insertUdimTagInFilename(const std::string& filename);

/**
 * @brief Create a composite cmd which deletes all the composite connections from and to \p sceneItem.
 * @note The cmd is not executed.
 * @param sceneItem The item to delete the connections from and to.
 * @return A composite cmd, it's nullptr if there are no component connections to delete.
 */
[[nodiscard]] LOOKDEVX_UFE_EXPORT std::shared_ptr<Ufe::CompositeUndoableCommand> deleteComponentConnections(
    const Ufe::SceneItem::Ptr& sceneItem);

/**
 * @brief Create a composite cmd which deletes all the adsk converter connections from and to \p sceneItem.
 * @note The cmd is not executed.
 * @param sceneItem The item to delete the connections from and to.
 * @return A composite cmd, it's nullptr if there are no adsk converter connections to delete.
 */
[[nodiscard]] LOOKDEVX_UFE_EXPORT std::shared_ptr<Ufe::CompositeUndoableCommand> deleteAdskConverterConnections(
    const Ufe::SceneItem::Ptr& sceneItem);

/**
 * Determines if the scene item represents a Materialx node in the MaterialX or USD
 * contexts.
 */
LOOKDEVX_UFE_EXPORT bool isMaterialXNode(const Ufe::SceneItemPtr& sceneItem);

//! Supported conversion for attribute types: <srcAttrType, dstAttrType>
using SupportedConversionTypes = std::vector<std::pair<std::string, std::string>>;
/**
 * @brief Returns the supported conversion for attribute types.
 * @return The supported conversions .
 */
[[nodiscard]] LOOKDEVX_UFE_EXPORT const SupportedConversionTypes& getSupportedConverterTypes();

/**
 * @brief Returns the converter Node Def for the \p srcAttr and the \p dstAttr.
 * @param srcAttr The source attribute.
 * @param dstAttr The destination attribute.
 * @return The converter NodeDef as string.
 */
[[nodiscard]] LOOKDEVX_UFE_EXPORT std::string getConverterNodeDef(const Ufe::Attribute::Type& srcAttrType,
                                                                  const Ufe::Attribute::Type& dstAttrType);
/**
 * @brief Checks if the conversion between \p srcAttrType and \p dstAttrType is supported.
 * @param srcAttrType The source attribute type.
 * @param dstAttrType The destination attribute type.
 * @return True if the conversion is supported, false otherwise.
 */
[[nodiscard]] LOOKDEVX_UFE_EXPORT bool isConversionSupported(const Ufe::Attribute::Type& srcAttrType,
                                                             const Ufe::Attribute::Type& dstAttrType);

/**
 * Utility function to get the source connections from \p attr.
 *
 * @param attr The connection attribute.
 * @param type The type of the attribute (destination or source).
 * @return The connections.
 */
[[nodiscard]] LOOKDEVX_UFE_EXPORT std::vector<Ufe::Connection::Ptr> getSourceConnections(
    const Ufe::Attribute::Ptr& attr, Ufe::Connections::AttributeType type);

/**
 * Utility function to get the all \p item connections.
 *
 * @param item The item.
 * @return All the source connections.
 */
[[nodiscard]] LOOKDEVX_UFE_EXPORT std::vector<Ufe::Connection::Ptr> getAllSourceConnections(
    const Ufe::SceneItem::Ptr& item);

/**
 * Utility function to get all the adsk converter connections from and to \p sceneItem.
 *
 * @param item The item.
 * @return All the adsk converter connections.
 */
[[nodiscard]] LOOKDEVX_UFE_EXPORT std::vector<LookdevXUfe::ExtendedConnection::Ptr> getAdskConnections(
    const Ufe::SceneItem::Ptr& sceneItem);

/**
 * Utility function to get all the adsk converter connections to \p sceneItem.
 *
 * @param item The item.
 * @return All the adsk converter connections.
 */
[[nodiscard]] LOOKDEVX_UFE_EXPORT std::vector<LookdevXUfe::ExtendedConnection::Ptr> getAllSourceAdskConnections(
    const Ufe::SceneItem::Ptr& sceneItem);

constexpr unsigned int kExtendedRegularConnections = 1;
constexpr unsigned int kExtendedComponentConnections = 2;
constexpr unsigned int kExtendedConverterConnections = 4;

/**
 * Get all the source extended connections of a specified set of types (regular, component and converter connections)
 * for \p sceneItem.
 *
 * @param sceneItem The scene item.
 * @param connectionTypes The connection types to include.
 * @return All the extended connections that were requested.
 */
[[nodiscard]] LOOKDEVX_UFE_EXPORT std::vector<LookdevXUfe::ExtendedConnection::Ptr> getSourceExtendedConnections(
    const Ufe::SceneItem::Ptr& sceneItem,
    unsigned int connectionTypes = kExtendedRegularConnections | kExtendedConverterConnections |
                                   kExtendedComponentConnections);

/**
 * Get all the extended connections of a specified set of types (regular, component and converter connections)
 * for \p sceneItem.
 *
 * @param sceneItem The scene item.
 * @param connectionTypes The connection types to include.
 * @return All the extended connections that were requested.
 */
[[nodiscard]] LOOKDEVX_UFE_EXPORT std::vector<LookdevXUfe::ExtendedConnection::Ptr> getAllExtendedConnections(
    const Ufe::SceneItem::Ptr& sceneItem,
    unsigned int connectionTypes = kExtendedRegularConnections | kExtendedConverterConnections |
                                   kExtendedComponentConnections);

/**
 * Return all the extended connections (e.g. component, converter connections) with the specified destination \p
 * attribute.
 *
 * @param attr The attribute.
 * @return All the extended converter connections for \p attr.
 */
[[nodiscard]] LOOKDEVX_UFE_EXPORT std::vector<LookdevXUfe::ExtendedConnection::Ptr> extendedConnections(
    const Ufe::Attribute::Ptr& attr);

/**
 * Return the attribute with the specified name from the \p item.
 *
 * @param item The scene item.
 * @param attrName The attribute name.
 * @return The attribute with the specified name if found, nullptr otherwise.
 */
[[nodiscard]] LOOKDEVX_UFE_EXPORT Ufe::Attribute::Ptr getAttribute(const Ufe::SceneItem::Ptr& item,
                                                                   const std::string& attrName);
} // namespace UfeUtils

} // LOOKDEVXUFE_NS_DEF

#endif
