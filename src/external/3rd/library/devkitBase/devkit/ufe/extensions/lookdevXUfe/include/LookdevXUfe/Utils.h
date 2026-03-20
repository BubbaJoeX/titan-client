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
#ifndef LOOKDEVXUFE_UTILS_H
#define LOOKDEVXUFE_UTILS_H

#include "Export.h"

#include <ufe/attribute.h>
#include <ufe/contextOps.h>
#include <ufe/nodeDef.h>
#include <ufe/sceneItem.h>
#include <ufe/selection.h>
#include <ufe/value.h>

#include <memory>
#include <string>
#include <unordered_set>

LOOKDEVXUFE_NS_DEF
{
// Converter metadatas.
constexpr std::string_view kInternalConverter = "internalConverter";           // NOLINT
constexpr std::string_view kConverterUnconnectable = "converterUnconnectable"; // NOLINT

const std::string kAutodeskGroup = "Autodesk"; // NOLINT

//! Helper method to get the meta data for a given key in Autodesk group
//! \param sceneItem The scene item containing the meta data
//! \param key The key to retrieve the value for
//! @return The Ufe Value of the meta data.
LOOKDEVX_UFE_EXPORT
Ufe::Value getAutodeskMetadata(const Ufe::SceneItem::Ptr& sceneItem, const std::string& key);

//! Helper method to set the meta data for a given key in Autodesk group
//! \param sceneItem The scene item containing the meta data
//! \param key The key to retrieve the value for
//! \param key The key to set the value on
//! \return UndoableCommandPtr of the set key action
LOOKDEVX_UFE_EXPORT
Ufe::UndoableCommandPtr setAutodeskMetadataCmd(const Ufe::SceneItem::Ptr& sceneItem,
                                               const std::string& key,
                                               const Ufe::Value& value);

//! Helper method to clear the meta data for a given key in Autodesk group
//! \param sceneItem The scene item containing the meta data
//! \param key The key to retrieve the value for
//! \return UndoableCommandPtr of the clear key action
LOOKDEVX_UFE_EXPORT
Ufe::UndoableCommandPtr clearAutodeskMetadataCmd(const Ufe::SceneItem::Ptr& sceneItem, const std::string& key);

//! Returns an undoable command to set the value of \p attribute to \p value. If the types of \p attribute and \p value
//! don't match, a null pointer is returned.
template <typename T>
LOOKDEVX_UFE_EXPORT Ufe::UndoableCommand::Ptr setAttributeValueCmd(const Ufe::Attribute::Ptr& attribute,
                                                                   const T& value);

//! Returns an undoable command to set the value of a float vector attribute to \p value. If the number of components in
//! \p value doesn't match the number of components of \p attribute, a null pointer is returned.
//!
//! Supported attribute types: float, Vector2f, Vector3f, Vector4f, Color3f, Color4f. If \p attribute is of a different
//! type, a null pointer is returned.
template <>
LOOKDEVX_UFE_EXPORT Ufe::UndoableCommand::Ptr setAttributeValueCmd(const Ufe::Attribute::Ptr& attribute,
                                                                   const std::vector<float>& value);
LOOKDEVX_UFE_EXPORT bool isDefaultValue(const Ufe::Attribute::Ptr& attribute);

//! Returns a sanitized list of node definitions based on LookdevX requirements.
LOOKDEVX_UFE_EXPORT Ufe::NodeDefs sanitizeDefinitions(const Ufe::NodeDefs& nodeDefs, Ufe::Rtid runtimeId);

//! Sorts the menu items alphabetically.
LOOKDEVX_UFE_EXPORT void sortMenuItemsAlphabetically(Ufe::ContextOps::Items& items);

//! Fast identification of component connection nodes will return one of these:
enum class ComponentNodeType
{
    eNone,
    eSeparateColor3,
    eSeparateColor4,
    eSeparateVector2,
    eSeparateVector3,
    eSeparateVector4,
    eCombineColor3,
    eCombineColor4,
    eCombineVector2,
    eCombineVector3,
    eCombineVector4
};

//! Check if the ComponentNodeType is a component combine node
LOOKDEVX_UFE_EXPORT bool isComponentCombineNode(ComponentNodeType componentType);

//! Check if the ComponentNodeType is a component separate node
LOOKDEVX_UFE_EXPORT bool isComponentSeparateNode(ComponentNodeType componentType);

//! Returns the number of channels handled by this component node
LOOKDEVX_UFE_EXPORT int numComponentChannels(ComponentNodeType componentType);

//! Parse a NodeDef identifier and return the corresponding component node:
LOOKDEVX_UFE_EXPORT ComponentNodeType identifyComponentNode(const std::string& nodeDefString);

//! Parse a NodeDef identifier and return if the id corresponds with a component combine node:
LOOKDEVX_UFE_EXPORT bool isComponentCombineNode(const std::string& nodeDefString);

//! Parse a NodeDef identifier and return if the id corresponds with a component separate node:
LOOKDEVX_UFE_EXPORT bool isComponentSeparateNode(const std::string& nodeDefString);

//! Given an attribute on a combine or separate node, return the component it handles:
LOOKDEVX_UFE_EXPORT std::string componentFromAttr(const Ufe::Attribute::Ptr& attribute);

//! Given a component node type and an attribute name, return the component it handles:
LOOKDEVX_UFE_EXPORT std::string componentFromTypeAndName(ComponentNodeType componentType,
                                                         const std::string& attributeName);

//! Check if the attribute is a adsk converter attribute.
LOOKDEVX_UFE_EXPORT bool isAdskConverterAttr(const Ufe::Attribute::Ptr& attr);

//! Check if the scene item is a adsk converter.
LOOKDEVX_UFE_EXPORT bool isAdskConverter(const Ufe::SceneItem::Ptr& item);

//! Set the Autodesk group meta data for the source connection adsk converter if found.
//! \param sceneItem The scene item to verify whether it is an adsk converter.
//! \return UndoableCommandPtr of the set key action, the cmd is not executed.
LOOKDEVX_UFE_EXPORT Ufe::UndoableCommand::Ptr setAdskUnconnectableConverterMetadataCmd(const Ufe::SceneItem::Ptr& item);

//! Clear the Autodesk group meta data for the source connection adsk converter if found.
//! \param sceneItem The scene item to verify whether it has the metadata.
//! \param attr The attribute to determine if it is metadata-related.
//! \return UndoableCommandPtr of the clear key action, the cmd is not executed.
LOOKDEVX_UFE_EXPORT Ufe::UndoableCommand::Ptr clearAdskUnconnectableConverterMetadataCmd(
    const Ufe::SceneItem::Ptr& item, const Ufe::Attribute::Ptr& attr);

//! Get the Autodesk converter, if any, which connects \p srcAttr and \p dstAttr.
//! \param srcAttr The source attribute.
//! \param dstAttr The destination attribute.
//! \return The Autodesk converter connecting the attributes.
LOOKDEVX_UFE_EXPORT Ufe::SceneItem::Ptr getAdskConnectedConverter(const Ufe::Attribute::Ptr& srcAttr,
                                                                  const Ufe::Attribute::Ptr& dstAttr);

/**
 * @brief Get the converter items that could be found in between the items in the selection.
 * @param selection The selection to check.
 * @return The in-between converter items.
 */
LOOKDEVX_UFE_EXPORT
std::unordered_set<Ufe::SceneItem::Ptr> getInBetweenAdskConverter(const Ufe::Selection& selection);

/**
 * @brief Add items to the selection that are extended connection items between those currently selected.
 * @param selection The selection to update.
 */
LOOKDEVX_UFE_EXPORT
void addInBetweenExtendedConnections(Ufe::Selection& selection);

/**
 * @brief Add items to the selection that are extended connection items, considering that we want to duplicate the
 * connections as well.
 * @param selection The selection to update.
 */
LOOKDEVX_UFE_EXPORT
void addExtendedDuplicateConnections(Ufe::Selection& selection);

} // LOOKDEVXUFE_NS_DEF
#endif
