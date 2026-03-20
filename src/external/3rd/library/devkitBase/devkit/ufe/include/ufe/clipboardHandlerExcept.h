#ifndef _clipboardHandlerExcept
#define _clipboardHandlerExcept

// ===========================================================================
// Copyright 2024 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
// ===========================================================================

#include "ufe.h"

#include <stdexcept>
#include <string>

UFE_NS_DEF {

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

//! \brief Exception class to signal empty clipboard operation (cut/copy/paste).
//
// See \ref Ufe::InvalidRunTimeName for inline implementation details.

class EmptyClipboardOperation : public std::runtime_error
{
public:
    EmptyClipboardOperation(const std::string& operation) :
        std::runtime_error(std::string("Empty clipboard ") + operation + std::string(" operation.")) {}
    EmptyClipboardOperation(const EmptyClipboardOperation&) = default;
    ~EmptyClipboardOperation() override {}
};

//! \brief Exception class to signal no clipboard handler implemented for runtime.
//
// See \ref Ufe::InvalidRunTimeName for inline implementation details.

class NoClipboardHandler : public std::runtime_error
{
public:
    NoClipboardHandler(const std::string& rtName) :
        std::runtime_error(std::string("No clipboard handler for runtime '") + rtName + std::string("'.")) {}
    NoClipboardHandler(const NoClipboardHandler&) = default;
    ~NoClipboardHandler() override {}
};

//! \brief Exception class to signal empty clipboard Selection (cut/copy/paste).
//
// See \ref Ufe::InvalidRunTimeName for inline implementation details.

class EmptyClipboardSelection: public std::invalid_argument
{
public:
    EmptyClipboardSelection(const std::string& operation) :
        std::invalid_argument(std::string("Empty clipboard selection for ") + operation + std::string(" operation.")) {}
    EmptyClipboardSelection(const EmptyClipboardSelection&) = default;
    ~EmptyClipboardSelection() override {}
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif

}

#endif /* _clipboardHandlerExcept */
