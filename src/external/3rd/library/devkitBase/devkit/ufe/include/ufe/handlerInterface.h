#ifndef _handlerInterface
#define _handlerInterface
// =======================================================================
// Copyright 2023 Autodesk, Inc. All rights reserved.
//
// This computer source code and related instructions and comments are the
// unpublished confidential  and proprietary information of Autodesk, Inc.
// and are protected under applicable copyright and trade secret law. They
// may not be disclosed to, copied  or used by any third party without the
// prior written consent of Autodesk, Inc.
// =======================================================================

#include "common/ufeExport.h"

#include <memory>
#include <string>

UFE_NS_DEF{

    /*
     *  Superclass for all dynamic handler interface extensions.
     *
     *  When creating a new interface the following steps are required:
     *  - Subclass from HandlerInterface
     *  - Provide a statically accessible unique id string for that interface.
     *    It doesn't have to be human-readable, just unique, as it is only
     *    used for internal dispatching.
     *  - It will also be useful to define a Ptr alias for shared_ptr as well
     *    as a static creation function to hide the fetching from the manager.
     *    Can be kept in the header as the main intention is to avoid changing
     *    call sites if the new interface ever moves to ufe core.
     *
     *  For example:
     *
     * \code
     *  class ExampleHandler: public HandlerInterface
     *  {
     *  public:
     *      using Ptr = std::shared_ptr<ExampleHandler>;
     *
     *      static constexpr auto id = "ExampleHandler";
     *
     *      static Ptr create(const Rtid& rtId)
     *      {
     *          return Ufe::RunTimeMgr::instance().handler<ExampleHandler>(rtId);
     *      }
     *
     *      // Interface functions here...
     *  };
     *  \endcode
     */
    class UFE_SDK_DECL HandlerInterface
    {
    public:
        using Ptr = std::shared_ptr<HandlerInterface>;

        HandlerInterface() = default;
        HandlerInterface(const HandlerInterface&) = default;
        HandlerInterface& operator=(const HandlerInterface&) = default;
        HandlerInterface(HandlerInterface&&) = default;
        HandlerInterface& operator=(HandlerInterface&&) = default;

        virtual ~HandlerInterface();
    };
}

#endif /* _handlerInterface */
