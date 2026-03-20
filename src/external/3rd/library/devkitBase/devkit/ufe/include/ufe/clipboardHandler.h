#ifndef _clipboardHandler
#define _clipboardHandler

// ===========================================================================
// Copyright 2024 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
// ===========================================================================

#include "common/ufeExport.h"

#include "sceneItem.h"
#include "selection.h"
#include "undoableCommand.h"
#include "clipboardCommands.h"

UFE_NS_DEF {

/*!
  This base class defines an interface that runtimes can implement to provide
  cut/copy and paste operations for sceneItem and Selection.
*/

class UFE_SDK_DECL ClipboardHandler
{
public:
    typedef std::shared_ptr<ClipboardHandler> Ptr;

    //! Constructor.
    ClipboardHandler() = default;
    //! Default copy constructor.
    ClipboardHandler(const ClipboardHandler&) = default;
    //! Destructor.
    virtual ~ClipboardHandler() = default;

    //! Convenience method to retrieve the Clipboard Handler from the input runtime id.
    //! The handler interface will remain valid as long as the given runtime
    //! remains set in the runtime manager.
    static Ptr clipboardHandler(Rtid);

    //! Creates a command which will cut the input item.  The command is not executed.
    //! The default implementation provided puts the input item in a Selection
    //! and then calls the corresponding method which takes a Selection argument.
    //! \param[in] item The item to cut.
    //! \exception std::invalid_argument if the argument is a null pointer.
    //! \return Command whose execution will cut the input items.
    virtual UndoableCommand::Ptr cutCmd_(const SceneItem::Ptr& item);

    //! Creates a command which will copy the input item.  The command is not executed.
    //! The default implementation provided puts the input item in a Selection
    //! and then calls the corresponding method which takes a Selection argument.
    //! \param[in] item The item to copy.
    //! \exception std::invalid_argument if the argument is a null pointer.
    //! \return Command whose execution will copy the input items.
    virtual UndoableCommand::Ptr copyCmd_(const SceneItem::Ptr& item);

    //! Convenience method that groups the items from the input selection into their runtime ID and
    //! calls the corresponding virtual \ref cutCmd_ on each clipboard handler. Creates a command to
    //! perform the cut operation. The command is not executed.
    //! 
    //! \exception EmptyClipboardSelection if the input selection is empty.
    //! \exception NoClipboardHandler if no clipboard handler found for runtime in selection.
    //! \exception EmptyClipboardOperation if there is/are no command(s) generated (empty composite cmd).
    //! 
    //! \param[in] selection The input selection to cut.
    //! \return Command whose execution will cut the input items.
    static UndoableCommand::Ptr cutCmd(const Selection& selection);

    //! Convenience method that groups the items from the input selection into their runtime ID and
    //! calls the corresponding virtual \ref copyCmd_ on each clipboard handler. Creates a command
    //! to perform the copy operation. The command is not executed.
    //! 
    //! \exception EmptyClipboardSelection if the input selection is empty.
    //! \exception NoClipboardHandler if no clipboard handler found for runtime in selection.
    //! \exception EmptyClipboardOperation if there is/are no command(s) generated (empty composite cmd).
    //! 
    //! \param[in] selection The input selection to copy.
    //! \return Command whose execution will copy the input items.
    static UndoableCommand::Ptr copyCmd(const Selection& selection);

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
    static UndoableCommand::Ptr pasteCmd(const Selection& parentItems);

    //! Convenience method that gets the clipboard handler corresponding to the runtime ID
    //! of the input item and then calls the virtual method \ref pasteCmd_.
    //! \exception std::invalid_argument if the input item is a null pointer.
    //! \exception NoClipboardHandler if no clipboard handler found for runtime of item.
    //! \param[in] parentItem The target parent item into which the clipboard is pasted.
    //! \return Command whose execution will paste the input items.
    static PasteClipboardCommand::Ptr pasteCmd(const SceneItem::Ptr& parentItem);

    //! Creates a command which will cut the items in the input selection.
    //! The command is not executed.
    //! The items are copied to the clipboard, then they are deleted.
    //! \param[in] selection Selection of items to cut.
    virtual UndoableCommand::Ptr cutCmd_(const Selection& selection) = 0;

    //! Creates a command which will copy the items in the input selection.
    //! The command is not executed.
    //! The items are copied to the clipboard.
    //! \param[in] selection Selection of items to copy.
    virtual UndoableCommand::Ptr copyCmd_(const Selection& selection) = 0;

    //! Creates a command which will paste the item(s) from clipboard to the input
    //! parent destination item. The command is not executed.
    //! \param[in] parentItem The parent where to try and paste the the clipboard item(s).
    //! \return Command whose execution will paste the input items.
    virtual PasteClipboardCommand::Ptr pasteCmd_(const SceneItem::Ptr& parentItem) = 0;

    //! Creates a command which will be paste the item(s) from the clipboard to the most
    //! suitable parent destination item.
    //! The command is not executed.
    //! \param[in] parentItems The parent items where to try and paste the clipboard item(s).
    //! \return Command whose execution will paste the input items.
    virtual UndoableCommand::Ptr pasteCmd_(const Selection& parentItems) = 0;

    //! Convenience method that calls \ref hasItemsToPaste_ on the appropriate clipboard handler.
    //! \param[in] rtid Runtime ID for the clipboard handler to call.
    //! \note Returns false if no clipboard handler exists for given runtime ID.
    static bool hasItemsToPaste(const Rtid& rtid);

    //! Utility method to check if the clipboard handler has any items to paste.
    //! \return true if there are items to paste, otherwise false.
    virtual bool hasItemsToPaste_() = 0;
 
    //! Convenience method that calls \ref canBeCut_ on the appropriate clipboard handler.
    //! \param[in] item SceneItem to be tested for cut.
    //! \note Returns false if input item is nullptr or no clipboard handler exists for runtime of item.
    static bool canBeCut(const SceneItem::Ptr& item);

    //! Utility method to test if an item meets specific conditions for the cut action.
    //! \param[in] item SceneItem to be tested for cut.
    //! \return true if the item can be cut, otherwise false.
    virtual bool canBeCut_(const SceneItem::Ptr& item) = 0;

    //! Convenience method that calls \ref preCopy_ for each clipboard handler that exists for
    //! for all the registered runtimes.
    static void preCopy();

    //! Utility method to perform any cleanup before the copy operation is executed.
    virtual void preCopy_() = 0;

    //! Convenience method that calls \ref preCut_ for each clipboard handler that exists for
    //! for all the registered runtimes.
    static void preCut();

    //! Utility method to perform any cleanup before the cut operation is executed.
    virtual void preCut_() = 0;
};

}

#endif /* _clipboardHandler */
