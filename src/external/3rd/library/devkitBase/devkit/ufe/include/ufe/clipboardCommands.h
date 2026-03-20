#ifndef _clipboardCommands
#define _clipboardCommands

// ===========================================================================
// Copyright 2024 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
// ===========================================================================

#include "common/ufeExport.h"

#include "undoableCommand.h"
#include "sceneItemList.h"

#include <vector>

UFE_NS_DEF {

class Path;

//! \brief Command that handles paste from clipboard.
/*!
  This class provides the interface to retrieve all the pasted items and information
  about each one (paste succeeded or failed).
*/
class UFE_SDK_DECL PasteClipboardCommand : public UndoableCommand
{
public:
    typedef std::shared_ptr<PasteClipboardCommand> Ptr;

    //! \brief Struct that holds information about the paste command.
    struct PasteInfo
    {
        Ufe::Path pasteTarget;
        std::vector<Path> successfulPastes;
        std::vector<Path> failedPastes;
    };

    //! Constructor.
    PasteClipboardCommand();

    //! Destructor.
    ~PasteClipboardCommand() override;

    ///@{
    //! No copy or move constructor/assignment.
    PasteClipboardCommand(const PasteClipboardCommand&) = delete;
    PasteClipboardCommand& operator=(const PasteClipboardCommand&) = delete;
    PasteClipboardCommand(PasteClipboardCommand&&) = delete;
    PasteClipboardCommand& operator=(PasteClipboardCommand&&) = delete;
    ///@}

    //! Retrieve all the pasted items.
    virtual SceneItemList targetItems() const = 0;

    //! Get the paste infos.
    virtual std::vector<PasteInfo> getPasteInfos() const = 0;
};

}

#endif // _clipboardCommands
