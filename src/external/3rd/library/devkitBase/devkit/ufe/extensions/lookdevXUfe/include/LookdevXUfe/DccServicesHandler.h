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

#include "ufe/handlerInterface.h"
#include "ufe/rtid.h"
#include "ufe/value.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

LOOKDEVXUFE_NS_DEF
{

//! \brief Factory base class for dcc services handling.
/*!
  This base class defines an interface that any host dcc runtime
  can implement to handle common utilities that any dcc might require,
  using its specific implementation.

  Operations include:
  - Get the dcc global selection.
  - Assign a material to a selected object in the editor.
  - Graph material in LookdevX panel.
  - etc.
*/
class LOOKDEVX_UFE_EXPORT DccServicesHandler : public Ufe::HandlerInterface
{
public:
    using Ptr = std::shared_ptr<DccServicesHandler>;

    static constexpr auto id = "LookdevXHandler_DccServices";

    static Ptr get(const Ufe::Rtid& rtId);

    /**
     * @brief Returns the Global Selection in the dcc scene graph.
     *
     * @param excludeMaterials option to exclude materials from the selection.
     * @param includeDescendants option to include descendants of the selected objects.
     * @return std::unordered_set<std::string> the list of selected objects.
     */
    [[nodiscard]] virtual std::unordered_set<std::string> getGlobalSelection(bool excludeMaterials = false,
                                                                             bool includeDescendants = false) const;

    /**
     * @brief Says if there are selected objects in the dcc scene graph.
     */
    [[nodiscard]] virtual bool hasSelectedItems() const;

    /**
     * @brief Assign a MaterialX material to a geometry object.
     * If 'targetGeom' is not specified, the assignment is done to all selected objects.
     *
     * @param materialPath identifier of the material to assign.
     * @param targetGeom the target geometry to assign the material to. If empty, assign to Global Selection.
     * @return true if assign operation succeeded.
     * @return false if assign operation failed.
     */
    [[nodiscard]] virtual bool materialXAssignCommand(const std::string& materialPath,
                                                      const std::string& targetGeom = "") const;

    /**
     * @brief Get the value of variable stored in the DCC preferences.
     *
     * @param variableName The name of the variable to query.
     */
    [[nodiscard]] virtual Ufe::Value getPreferenceVariable(const std::string& variableName) const;

    /**
     * @brief Get a list of the loaded plugins information.
     *
     * @return std::vector<std::string> the list of plugin info. String format: "pluginName version".
     */
    [[nodiscard]] virtual std::vector<std::string> getPluginsInfoList() const;

    /**
     * @brief Get the Web Documentation URL. Each DCC directs to its documentation page.
     *
     * @return std::string the URL of the documentation.
     */
    [[nodiscard]] virtual std::string getWebDocumentationURL() const;

    /**
     * @brief Opens the LookdevX window.
     *
     */
    virtual void openLookdevXWindow();

    /**
     * @brief Closes the LookdevX window.
     *
     */
    virtual void closeLookdevXWindow();

    /**
     * @brief Check if the LookdevX window is opened.
     *
     * @return true if the LookdevX window is opened.
     */
    [[nodiscard]] virtual bool isLookdevXWindowOpened() const;

    /**
     * @brief Display the dcc preferences window, possibly focusing on the LookdevX section.
     *
     */
    virtual void displayPreferencesWindow();

    /**
     * @brief Set the current container in LookdevX.
     *
     * @param containerName the name of the container (tab).
     * @return true if successful, false if tab not found.
     */
    [[nodiscard]] virtual bool setCurrentContainer(const std::string& containerName);

    /**
     * @brief Get the name of the current container in LookdevX.
     *
     * @return std::string the name of the current container.
     */
    [[nodiscard]] virtual std::string getCurrentContainerName() const;

    /**
     * @brief Set the global libraries in the DCC.
     *
     * @param globalLibraries the global libraries value to use.
     */
    virtual void setGlobalLibraries(const std::string& globalLibraries) const;
};

} // LOOKDEVXUFE_NS_DEF
