#ifndef UFE_LIGHT2_H
#define UFE_LIGHT2_H
// ===========================================================================
// Copyright 2025 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
// ===========================================================================

#include "common/ufeExport.h"
#include "sceneItem.h"
#include "observer.h"
#include "baseUndoableCommands.h"
#include "types.h"
#include "value.h"
#include "lightNotification.h"

#include <memory>
#include <array>
#include <vector>

UFE_NS_DEF
{

class UndoableCommand;

class UFE_SDK_DECL LightInterface
{
    public:
    typedef std::shared_ptr<LightInterface> Ptr;
    //! Constructor.
    LightInterface();
    //! Default copy constructor.
    LightInterface(const LightInterface&) = default;
    //! Destructor.
    virtual ~LightInterface();
};

class UFE_SDK_DECL AreaLightInterface : public LightInterface
{
public:
    typedef std::shared_ptr<AreaLightInterface> Ptr;
    //! Constructor.
    AreaLightInterface();
    //! Default copy constructor.
    AreaLightInterface(const AreaLightInterface&) = default;
    //! Destructor.
    virtual ~AreaLightInterface() override;

    using NormalizeUndoableCommand = SetBoolUndoableCommand;
    using WidthUndoableCommand = SetFloatUndoableCommand;
    using HeightUndoableCommand = SetFloatUndoableCommand;

    //! Normalize attribute. Normalizes power by the surface area of the light.
    //! Create an undoable command to set the normalize flag. The command is not executed.
    //! \param nl the new normalize flag value.
    //! \return Undoable command to set the normalize flag
    virtual NormalizeUndoableCommand::Ptr normalizeCmd(bool nl) = 0;

    //! Normalize attribute. Normalizes power by the surface area of the light.
    //! Set the normalize flag. Default implementation uses NormalizeUndoableCommand.
    //! \param nl the new normalize flag value.
    virtual void normalize(bool nl) {
        if (auto cmd = normalizeCmd(nl)) {
            cmd->execute();
        }
    }

    //! Retrieve the normalize flag for the light
    //! \return the normalize flag value
    virtual bool normalize() const = 0;

    //! Create an undoable command to set the width. The command is not executed.
    //! \param w the new width  value.
    //! \return Undoable command to set the width
    virtual WidthUndoableCommand::Ptr widthCmd(float w) = 0;

    //! Set the width . Default implementation uses WidthUndoableCommand.
    //! \param w the new width  value.
    virtual void width(float w) {
        if (auto cmd = widthCmd(w)) {
            cmd->execute();
        }
    }

    //! Retrieve the width for the light
    //! \return the width value
    virtual float width() const = 0;

    //! Create an undoable command to set the height. The command is not executed.
    //! \param h the new height  value.
    //! \return Undoable command to set the height
    virtual HeightUndoableCommand::Ptr heightCmd(float h) = 0;

    //! Set the height . Default implementation uses HeightUndoableCommand.
    //! \param h the new height  value.
    virtual void height(float h) {
        if (auto cmd = heightCmd(h)) {
            cmd->execute();
        }
    }

    //! Retrieve the height for the light
    //! \return the height value
    virtual float height() const = 0;
};

class UFE_SDK_DECL Light2 : public std::enable_shared_from_this<Light2>
{
public:
    typedef std::shared_ptr<Light2> Ptr;

    enum Type
    {
        Invalid,
        Area,
    };

    using IntensityUndoableCommand = SetFloatUndoableCommand;
    using ColorUndoableCommand = SetColor3fUndoableCommand;
    using ShadowEnableUndoableCommand = SetBoolUndoableCommand;
    using ShadowColorUndoableCommand = SetColor3fUndoableCommand;
    using DiffuseUndoableCommand = SetFloatUndoableCommand;
    using SpecularUndoableCommand = SetFloatUndoableCommand;

    /*!
        Convenience method that calls the \ref Ufe::Light2Handler::light()
        method on the Light handler for the item. Returns a null pointer
        if the argument is null, is an empty path or if the item does not
        support the Light interface.
        \param item SceneItem's Light to retrieve
        \return Light of the given SceneItem
    */
    static Ptr light(const SceneItem::Ptr& item);

    /*!
        Add observation on the argument item for Light changes.
        \param item SceneItem to observe.
        \param obs Observer to add.
        \return True if the observer is added. Add does nothing and returns
        false if the observer is already present.
    */
    static bool addObserver(
        const SceneItem::Ptr& item, const Observer::Ptr& obs);
    /*!
        Remove observation on the argument item for Light changes.
        \param item SceneItem to remove observation on.
        \param obs Observer to remove.
        \return True if the observer is removed. False if the observer isn't
        found.
    */
    static bool removeObserver(
        const SceneItem::Ptr& item, const Observer::Ptr& obs);

    /*!
        Number of observers on the given SceneItem.
        \param item SceneItem for which to count observers.
        \return Number of observers on SceneItem.
    */
    static std::size_t nbObservers(const SceneItem::Ptr& item);

    /*!
        Query observation on argument item for light changes.
        \param item SceneItem to check if has observation.
        \param obs Observer to query.
        \return True if there is observation on argument item
        for light changes.
    */
    static bool hasObserver(
        const SceneItem::Ptr& item, const Observer::Ptr& obs);

    //! \param path Path to verify if being observed.
    //! \return True if the given path is being observed.
    static bool hasObservers(const Path& path);

    //! Helper query for runtimes, to determine if any path they are
    //! responsible for is being observed.
    //! \param runTimeId runtime Id to find observers on.
    //! \return True if any path of given runtime Id are being observed.
    static bool hasObservers(Rtid runTimeId);

    //! Notify all observers of the item with this path.  If no observer exists,
    //! does nothing.
    //! \param notification Notification to use on the notify action.
    static void notify(const Ufe::LightChanged& notification);

    // --------------------------------------------------------------------- //
     /// \name Metadata Access:
     /// @{
     // --------------------------------------------------------------------- //

     /*!
         Get the value of the metadata named key.
         \param[in] key The metadata key to query.
         \return The value of the metadata key. If the key does not exist an empty Value is returned.
     */
    virtual Value getMetadata(const std::string& key) const = 0;

    /*!
        Set the metadata key's value to value.
        \param[in] key The metadata key to set.
        \param[in] value The value to set.
        \return True if the metadata was set successfully, otherwise false.
    */
    virtual bool setMetadata(const std::string& key, const Value& value) = 0;

    //! Return a command for undo / redo that sets the metadata key's value to value.
    //! The returned command is not executed; it is up to the caller to call execute().
    virtual UndoableCommand::Ptr setMetadataCmd(const std::string& key, const Value& value);

    /*!
        Clear the metadata key's value.
        \param[in] key The metadata key to clear.
        \return True if the metadata was cleared successfully, otherwise false.
    */
    virtual bool clearMetadata(const std::string& key) = 0;

    //! Return a command for undo / redo that clear the metadata.
    //! The returned command is not executed; it is up to the caller to call
    //! execute().
    virtual UndoableCommand::Ptr clearMetadataCmd(const std::string& key);

    //! Returns true if metadata key has a non-empty value.
    virtual bool hasMetadata(const std::string& key) const = 0;

    // --------------------------------------------------------------------- //
    /// @}
    // --------------------------------------------------------------------- //

    //! Constructor.
    Light2();
    //! Default copy constructor.
    Light2(const Light2&) = default;
    //! Destructor.
    virtual ~Light2();

    //! \return the object's Path.
    virtual const Path& path() const = 0;

    //! \return the object's SceneItem.
    virtual SceneItem::Ptr sceneItem() const = 0;

    //! \return the light type
    virtual Type type() const = 0;

    //! A collection of LightInterface pointers that this light supports.
    std::vector<LightInterface::Ptr> interfaces;

    /*************************************************
        Light intensity attribute.
        Valid for the following light types: [all]
    *************************************************/

    //! Create an undoable command to set the intensity. The command is not executed.
    //! \param li the new intensity value.
    //! \return Undoable command to set the intensity
    virtual IntensityUndoableCommand::Ptr intensityCmd(float li) = 0;

    //! Set the intensity. Default implementation uses IntensityUndoableCommand.
    //! \param li the new intensity value.
    virtual void intensity(float li) {
        if (auto cmd = intensityCmd(li)) {
            cmd->execute();
        }
    }

    //! Retrieve the intensity for the light
    //! \return the intensity value
    virtual float intensity() const = 0;

    /*************************************************
        Light color attribute, defined in energy-linear terms
        Valid for the following light types: [all]
    *************************************************/

    //! Create an undoable command to set the color. The command is not executed.
    //! \param r Red component of the new color value.
    //! \param g Green component of the new color value.
    //! \param b Blue component of the new color value.
    //! \return Undoable command to set the color
    virtual ColorUndoableCommand::Ptr colorCmd(float r, float g, float b) = 0;

    //! Set the color. Default implementation uses ColorUndoableCommand.
    //! \param r red component \param g green component \param b blue component
    virtual void color(float r, float g, float b) {
        if (auto cmd = colorCmd(r, g, b)) {
            cmd->execute();
        }
    }

    //! Retrieve the color for the light
    //! \return the color value
    virtual Color3f color() const = 0;

    /*************************************************
        Light shadow enable attribute.
        Valid for the following light types: [all]
    *************************************************/

    //! Create an undoable command to set the shadow enable flag. The command is not executed.
    //! \param se the new shadow enable flag value.
    //! \return Undoable command to set the shadow enable flag
    virtual ShadowEnableUndoableCommand::Ptr shadowEnableCmd(bool se) = 0;

    //! Set the shadow enable flag. Default implementation uses ShadowEnableUndoableCommand.
    //! \param se the new shadow enable flag value.
    virtual void shadowEnable(bool se) {
        if (auto cmd = shadowEnableCmd(se)) {
            cmd->execute();
        }
    }

    //! Retrieve the shadow enable flag
    //! \return the shadow enable flag value
    virtual bool shadowEnable() const = 0;

    /*************************************************
        Shadow color attribute.
        Valid for the following light types: [all]
    *************************************************/

    //! Create an undoable command to set the shadow color. The command is not executed.
    //! \param r Red component of the new shadow color value.
    //! \param g Green component of the new shadow color value.
    //! \param b Blue component of the new shadow color value.
    //! \return Undoable command to set the shadow color
    virtual ShadowColorUndoableCommand::Ptr shadowColorCmd(float r, float g, float b) = 0;

    //! Set the shadow color. Default implementation uses ShadowColorUndoableCommand.
    //! \param r red component \param g green component \param b blue component
    virtual void shadowColor(float r, float g, float b) {
        if (auto cmd = shadowColorCmd(r, g, b)) {
            cmd->execute();
        }
    }

    //! Retrieve the shadow color for the light
    //! \return the shadow color value
    virtual Color3f shadowColor() const = 0;

    /*************************************************
        Light diffuse attribute, a multiplier for the effect
        of this light on the diffuse response of materials.
        Valid for the following light types: [all]
    *************************************************/

    //! Create an undoable command to set the diffuse. The command is not executed.
    //! \param ld the new diffuse value.
    //! \return Undoable command to set the diffuse
    virtual DiffuseUndoableCommand::Ptr diffuseCmd(float ld) = 0;

    //! Set the diffuse. Default implementation uses DiffuseUndoableCommand.
    //! \param ld the new diffuse value.
    virtual void diffuse(float ld) {
        if (auto cmd = diffuseCmd(ld)) {
            cmd->execute();
        }
    }

    //! Retrieve the diffuse for the light
    //! \return the diffuse value
    virtual float diffuse() const = 0;

    /*************************************************
        Light specular attribute, a multiplier for the effect
        of this light on the specular response of materials.
        Valid for the following light types: [all]
    *************************************************/

    //! Create an undoable command to set the specular. The command is not executed.
    //! \param ls the new specular value.
    //! \return Undoable command to set the specular
    virtual SpecularUndoableCommand::Ptr specularCmd(float ls) = 0;

    //! Set the specular. Default implementation uses SpecularUndoableCommand.
    //! \param ls the new specular value.
    virtual void specular(float ls) {
        if (auto cmd = specularCmd(ls)) {
            cmd->execute();
        }
    }

    //! Retrieve the specular for the light
    //! \return the specular value
    virtual float specular() const = 0;
};
}

#endif /* UFE_LIGHT2_H */
