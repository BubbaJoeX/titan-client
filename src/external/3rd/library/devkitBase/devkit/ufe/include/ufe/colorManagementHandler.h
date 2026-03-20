//*****************************************************************************
// Copyright (c) 2025 Autodesk, Inc.
// All rights reserved.
//
// These coded instructions, statements, and computer programs contain
// unpublished proprietary information written by Autodesk, Inc. and are
// protected by Federal copyright law. They may not be disclosed to third
// parties or copied or duplicated in any form, in whole or in part, without
// the prior written consent of Autodesk, Inc.
//*****************************************************************************
#ifndef UFE_COLOR_MANAGEMENT_HANDLER_H
#define UFE_COLOR_MANAGEMENT_HANDLER_H

#include "common/ufeExport.h"

#include "notification.h"
#include "rtid.h"
#include "subject.h"
#include "observer.h"

#include <memory>
#include <string>
#include <vector>

UFE_NS_DEF
{

//! \brief Provides a minimal color management interface that allows clients to query settings related to input color
//! spaces.

/*!
  This is intended as a wrapper around the DCC's global color management preferences.
*/

class UFE_SDK_DECL ColorManagementHandler
{
public:
    typedef std::shared_ptr<ColorManagementHandler> Ptr;

    //! Constructor.
    ColorManagementHandler() = default;
    //! Default copy constructor.
    ColorManagementHandler(const ColorManagementHandler&) = default;
    //! Destructor.
    virtual ~ColorManagementHandler();

    //! Convenience method to retrieve the Color Management Handler from the input runtime id.
    //! The handler interface will remain valid as long as the given runtime
    //! remains set in the runtime manager.
    static Ptr colorManagementHandler(Rtid);

    /**
     * \brief Check whether color management is enabled in the DCC preferences.
     *
     * \return True if enabled, otherwise false.
     */
    virtual bool isColorManagementEnabled() const = 0;

    /**
     * \brief Get a list of all input color spaces from the DCC preferences.
     *
     * \return An array of strings containing the input color spaces.
     */
    virtual std::vector<std::string> getInputSpaceNames() const = 0;

    /**
     * \brief Get a list of color space families for the given color space. This is mainly intended to fill a
     * hierarchical UI dropdown menu.
     *
     * \param inputColorSpace The name of an input color space.
     * \return An array of strings comprising the families for the given color space.
     */
    virtual std::vector<std::string> getInputSpaceFamilies(const std::string& inputColorSpace) const = 0;

    /**
     * \brief Uses the DCC's color management rules to determine the most appropriate input color space for the given
     * texture. E.g. a `.jpg` texture might return "sRGB", whereas an Arnold `.tx` would return "raw".
     *
     * \param filePath The path to a texture file.
     * \return The name of the input color space determined to be the most appropriate for the given texture file
     * according to the color management file rules. If an invalid path is specified, the default color space is
     * returned.
     */
    virtual std::string getColorSpaceFromFileRule(const std::string& filePath) const = 0;

    /** Returns the configuration file to be used, if color management is enabled.
     *
     * \return Path to the config file.
     */
    virtual std::string getConfigFilePath() const = 0;

    /** The color space to be used during rendering. This is the source color space to the viewing transform, for color
     * managed viewers and color managed UI controls, and the destination color space for color managed input pixels.
     *
     * \return Rendering color space name.
     */
    virtual std::string getRenderingSpaceName() const = 0;

    /** Returns the view from the (display, view) pair, to be applied by color managed viewers and color managed UI
     * controls.
     *
     * \return View name.
     */
    virtual std::string getViewName() const = 0;

    /** Returns the display from the (display, view) pair, to be applied by color managed viewers and color managed UI
     * controls.
     *
     * \return Display name.
     */
    virtual std::string getDisplayName() const = 0;

    /** Checks if a color space is known by the config. Includes checking for aliases.
     *
     * \return True is the name is recognized.
     */
    virtual bool isKnownColorSpace(const std::string& colorSpace) const = 0;

    /** Returns the compact name to use in the data streams.
     *
     * \return A compact name. Will return "colorSpace" if it is unknown to OCIO.
     */
    virtual std::string getCompactName(const std::string& colorSpace) const = 0;

    /** Returns the canonical name to use for UI.
     *
     * \return A canonical name. Will return "colorSpace" if it is unknown to OCIO.
     */
    virtual std::string getCanonicalName(const std::string& colorSpace) const = 0;

    /** Returns the description to use for UI tooltips.
     *
     * \return The color space description found in the OCIO file. Can be empty.
     */
    virtual std::string getDescription(const std::string& colorSpace) const = 0;

    // --------------------------------------------------------------------- //
    /// \name Metadata Keys:
    /// @{
    // --------------------------------------------------------------------- //

    /*!
        Metadata key: ignoreColorManagementFileRules, value type = bool\n
        SceneItem group metadata to not apply color management rules on this attributes when it changes.
    */
    static constexpr char kIgnoreColorManagementFileRules[] = "ignoreColorManagementFileRules";

    /*!
        Metadata key: colorSpace, value type = string\n
        The current color space value on an Attribute/SceneItem (Node/NodeDef/Document).
    */
    static constexpr char kColorSpaceMetadataKey[] = "colorSpace";

    // --------------------------------------------------------------------- //
    /// @}
    // --------------------------------------------------------------------- //

    /*!
        Add observation for global color management changes
        \param obs Observer to add.
        \return True if the observer is added. Add does nothing and returns 
         false if the observer is already present.
    */
    static bool addObserver(const Observer::Ptr& obs);

    /*!
        Remove observation for global color management changes
        \param obs Observer to remove.
        \return True if the observer is removed. False if the observer isn't
        found.
    */
    static bool removeObserver(const Observer::Ptr& obs);

    /*!
        Number of observers for color management changes
        \return Number of observers.
    */
    static std::size_t nbObservers();

    /*!
        Query observation for color management changes
        \param obs Observer to query.
        \return True if the observer observes Object3d changes on any item
        in the scene.
    */
    static bool hasObserver(const Observer::Ptr& obs);

    //! Notify all global observers.  The order in which observers are notified
    //! is unspecified.
    //! If no observer exists, does nothing.
    //! \param notif The color management notification to send.
    static void notify(const Notification& notif);
};

} // UFE_NS_DEF

#endif
