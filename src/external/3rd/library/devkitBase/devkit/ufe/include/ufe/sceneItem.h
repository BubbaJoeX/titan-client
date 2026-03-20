#ifndef _ufeSceneItem
#define _ufeSceneItem
// ===========================================================================
// Copyright 2018 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
// ===========================================================================

#include "path.h"
#include "ufeFwd.h"
#include "value.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

UFE_NS_DEF{

    class UndoableCommand;

/**
 * \class SceneItem
 *
 * \brief Universal Front End abstract scene item. 
 *
 * A scene item identifies an object or 3D path in the scene, independent
 * of its underlying runtime.  It consists of a path that identifies an object
 * in the scene.
 *
 * A scene item may optionally represent a property associated with a given
 * scene object.  A property represents data associated with a single node or
 * object.  In different runtimes, it is variously known as an attribute, a
 * component, or an underworld path, but is always associated with a single
 * node or object.
 */

class UFE_SDK_DECL SceneItem
{
public:
    typedef std::shared_ptr<SceneItem> Ptr;
    
    //! Constructor.
    //! \param path Path of the scene item to build.
    SceneItem(const Path& path);

    //! Default copy constructor.
    SceneItem(const SceneItem&) = default;

    //! Destructor.
    virtual ~SceneItem();

    //! \return Path of the SceneItem.
    const Path& path() const;

    //! \return Convenience to obtain the runtime ID of the Path.
    Rtid runTimeId() const;

    //@{
    //! Unfortunately no compiler-generated default equality operators.  Items
    //! are equal if their path is equal.
    bool operator==(const SceneItem& rhs) const;
    bool operator!=(const SceneItem& rhs) const;
    //@}

    //! \return Convenience to return the name of the node at the tail of the Path.
    std::string nodeName() const;

    //! Pure virtual method to return the type of the last node
    //! \return type of node at the tail of the Path.
    virtual std::string nodeType() const = 0;

    //! Return a list of all ancestor types (including the type of
    //! the scene item itself) in order from closest ancestor to farthest.
    //! The starting type is itself included, as the first element of the results vector.
    //! The implementation in this class returns a vector containing a single
    //! item, the nodeType of this scene item.
    //! \return List of all ancestor types (including this one).
    virtual std::vector<std::string> ancestorNodeTypes() const;

    //! Return whether this scene item represents a property.  The
    //! implementation in this class returns false.
    //! \return true if this scene item represents a property.
    virtual bool isProperty() const;

    //! Pure virtual method to return meta data for a given key
    //! \param key Key to look for in meta data 
    //! \return Ufe::Value of the found key, otherwise the default Ufe::Value()
    virtual Ufe::Value getMetadata(const std::string& key) const = 0;
    
    //! Pure virtual method to set a meta data and return an UndoableCommand pointer
    //! The returned command is not executed; it is up to the caller to call execute().
    //! \param key The key to set the value on
    //! \param value The value for the given key
    //! \return UndoableCommandPtr of the set key action
    virtual UndoableCommandPtr setMetadataCmd(const std::string& key, const Ufe::Value& value) = 0;

    //! Virtual method to set a meta data and execute its UndoableCommand
    //! \param key The key to set the value on
    //! \param value The value for the given key
    virtual void setMetadata(const std::string& key, const Ufe::Value& value);

    //! Pure virtual method to clear a meta data
    //! The returned command is not executed; it is up to the caller to call execute().
    //! \param key The key to clear
    //! \return UndoableCommandPtr of the clear action
    virtual UndoableCommandPtr clearMetadataCmd(const std::string& key = "") = 0;

    //! Virtual method to clear a meta data and execute its UndoableCommand
    //! \param key The key to clear
    virtual void clearMetadata(const std::string& key = "");

    //! Pure virtual method to return meta data for a given key in a given group
    //! \param group The group that contains the given key
    //! \param key Key to look for in meta data 
    //! \return Ufe::Value of the found key, otherwise the default Ufe::Value()
    virtual Ufe::Value getGroupMetadata(const std::string& group, const std::string& key) const = 0;
    
    //! Pure virtual method to set a meta data and return an UndoableCommand pointer
    //! The returned command is not executed; it is up to the caller to call execute().
    //! \param group The group this key is going to be set on
    //! \param key The key to set the value on
    //! \param value The value for the given key
    //! \return UndoableCommandPtr of the set key action
    virtual UndoableCommandPtr setGroupMetadataCmd(const std::string& group, const std::string& key, const Ufe::Value& value) = 0;

    //! Virtual method to set a meta data and execute its UndoableCommand
    //! \param group The group this key is going to be set on
    //! \param key The key to set the value on
    //! \param value The value for the given key
    virtual void setGroupMetadata(const std::string& group, const std::string& key, const Ufe::Value& value);
    
    //! Pure virtual method to clear a meta data
    //! The returned command is not executed; it is up to the caller to call execute().
    //! \param group The group this key is going to be cleared from
    //! \param key The key to be cleared
    //! \return UndoableCommandPtr of the clear action. 
    virtual UndoableCommandPtr clearGroupMetadataCmd(const std::string& group, const std::string& key = "") = 0;

    //! Virtual method to clear a meta data and execute its UndoableCommand
    //! \param group The group this key is going to be cleared from
    //! \param key The key to be cleared
    virtual void clearGroupMetadata(const std::string& group, const std::string& key = "");

private:

    Path fPath;
};

}

#endif  /* _ufeSceneItem */
