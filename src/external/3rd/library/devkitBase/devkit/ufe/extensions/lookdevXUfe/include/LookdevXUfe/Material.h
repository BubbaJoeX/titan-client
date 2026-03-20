//*****************************************************************************
// Copyright (c) 2023 Autodesk, Inc.
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

#include <ufe/sceneItem.h>

#include <memory>

LOOKDEVXUFE_NS_DEF
{
//! \brief Abstract base class for Material interface.
/*!

  This base class defines the interface that runtimes can implement to
  provide basic material behavior, e.g. querying the materials assigned
  to a SceneItem.

  To avoid the memory-consuming "one proxy object per scene object" approach,
  Material interface objects should be obtained and used within a local
  scope, and not stored.  Material interfaces should be considered
  stateless, and can be bound to new selection items.
*/
class LOOKDEVX_UFE_EXPORT Material
{
public:
    typedef std::shared_ptr<Material> Ptr;

    /*!
        Convenience method that calls the material method on the
        Material handler for the item.  Returns a null pointer if the
        argument is null, or has an empty path.
        \param item SceneItem for which Material interface is desired.
        \return Material interface of the given SceneItem.
    */
    static Ptr material(const Ufe::SceneItem::Ptr& item);

    //! Constructor.
    Material();
    //! Default copy constructor.
    Material(const Material&) = default;
    //! Destructor.
    virtual ~Material();

    //! Returns all the materials assigned to this scene item.
    virtual std::vector<Ufe::SceneItem::Ptr> getMaterials() const = 0;

    //! Returns true if any material is assigned to this scene item.
    virtual bool hasMaterial() const = 0;
};

} // LOOKDEVXUFE_NS_DEF
