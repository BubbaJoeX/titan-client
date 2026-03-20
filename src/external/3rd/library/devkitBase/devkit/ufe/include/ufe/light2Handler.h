#ifndef UFE_LIGHT2HANDLER_H
#define UFE_LIGHT2HANDLER_H
// ===========================================================================
// Copyright 2025 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
// ===========================================================================

#include "light2.h"
#include "sceneItem.h"

UFE_NS_DEF {

class Path;

//! \brief Factory base class for Light2 interface.
/*!

  This base class defines an interface for factory objects that runtimes
  can implement to create a Light2 interface object.
*/

class UFE_SDK_DECL Light2Handler
{
public:
    typedef std::shared_ptr<Light2Handler> Ptr;
    //! Constructor.
    Light2Handler();
    //! Default copy constructor.
    Light2Handler(const Light2Handler&) = default;
    //! Destructor.
    virtual ~Light2Handler();

    /*!
        Creates a Light interface on the given SceneItem.
        \param item SceneItem to use to retrieve its Light interface.
        \return Light interface of given SceneItem. Returns a null
        pointer if no Light interface can be created for the item.
    */
    virtual Light2::Ptr light(
        const SceneItem::Ptr& item) const = 0;

};

}

#endif /* UFE_LIGHT2HANDLER_H */
