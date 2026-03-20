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

#include <ufe/clipboardHandler.h>

#include <memory>

LOOKDEVXUFE_NS_DEF
{
using PasteInfo = Ufe::PasteClipboardCommand::PasteInfo;

//! \brief Provides an interface for scene items cut/copy and paste operations.

class LOOKDEVX_UFE_EXPORT ClipboardHandler : public Ufe::ClipboardHandler
{
public:
    // Clipboard file name
    static constexpr auto clipboardFileName = "LookdevXClipboard";

    using Ptr = std::shared_ptr<ClipboardHandler>;

    static Ptr get(const Ufe::Rtid& rtId);

    /*!
        Convenience method that calls \ref hasMaterialToPasteImpl on the appropriate clipboard handler.
    */
    [[nodiscard]] static bool hasMaterialToPaste(const Ufe::Rtid& rtid);

    /*!
        Convenience method that calls \ref hasNonMaterialToPasteImpl on the appropriate clipboard handler.
    */
    [[nodiscard]] static bool hasNonMaterialToPaste(const Ufe::Rtid& rtid);

    /*!
        Convenience method that calls \ref hasNodeGraphsToPasteImpl on the appropriate clipboard handler.
    */
    [[nodiscard]] static bool hasNodeGraphsToPaste(const Ufe::Rtid& rtid);

    /*!
       Set the clipboard path, i.e. where it should be saved.
       \param clipboardPath The clipboard path.
    */
    virtual void setClipboardPath(const std::string& clipboardPath) = 0;

    //! Convenience method that groups the items from the input selection into their runtime ID and
    //! calls the corresponding virtual \ref pasteCmd_ on each clipboard handler. Creates a command
    //! to perform the paste operation. The command is not executed.
    //!
    //! \exception EmptyClipboardSelection if the input selection is empty.
    //! \exception NoClipboardHandler if no clipboard handler found for runtime in selection.
    //! \exception EmptyClipboardOperation if there is/are no command(s) generated (empty composite cmd).
    //!
    //! \param[in] parentItems The target parent items into which the clipboard is pasted.
    //! \return Command whose execution will paste the input items.
    static Ufe::UndoableCommand::Ptr pasteCmd(const Ufe::Selection& parentItems);

    //! Convenience method that gets the clipboard handler corresponding to the runtime ID
    //! of the input item and then calls the virtual method \ref pasteCmd_.
    //! \exception std::invalid_argument if the input item is a null pointer.
    //! \exception NoClipboardHandler if no clipboard handler found for runtime of item.
    //! \param[in] parentItem The target parent item into which the clipboard is pasted.
    //! \return Command whose execution will paste the input items.
    static Ufe::PasteClipboardCommand::Ptr pasteCmd(const Ufe::SceneItem::Ptr& parentItem);

private:
    /*!
        Utility function: Check if the handler has a material to paste.
    */
    [[nodiscard]] virtual bool hasMaterialToPasteImpl() = 0;

    /*!
        Utility function: Check if the handler has a non material to paste.
    */
    [[nodiscard]] virtual bool hasNonMaterialToPasteImpl() = 0;

    /*!
        Utility function: Check if the handler has NodeGraphs to paste.
    */
    [[nodiscard]] virtual bool hasNodeGraphsToPasteImpl() = 0;
};

} // LOOKDEVXUFE_NS_DEF
