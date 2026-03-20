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

#include <ufe/attribute.h>
#include <ufe/handlerInterface.h>
#include <ufe/runTimeMgr.h>
#include <ufe/undoableCommand.h>

#include <memory>

LOOKDEVXUFE_NS_DEF
{

//! \brief Factory base class for file handling interface.
/*!

  This base class defines an interface for factory objects that runtimes
  can implement to handle file specific operations.
*/
class LOOKDEVX_UFE_EXPORT FileHandler : public Ufe::HandlerInterface
{
public:
    using Ptr = std::shared_ptr<FileHandler>;

    static constexpr auto id = "LookdevXHandler_File";

    static Ptr get(const Ufe::Rtid& rtId);

    /*!
        Returns the resolved path of a filename attribute.
        \param fnAttr Filename attribute to resolve.
        \return The resolved path. Empty string if it could not be resolved.
    */
    [[nodiscard]] virtual std::string getResolvedPath(const Ufe::AttributeFilename::Ptr& fnAttr) const = 0;

    /*!
        Returns a command that will convert the path of a filename attribute to an absolute path.
        \param fnAttr Filename attribute to convert.
        \return The command, or nullptr if conversion is not required or impossible to do.
    */
    [[nodiscard]] virtual Ufe::UndoableCommand::Ptr convertPathToAbsoluteCmd(
        const Ufe::AttributeFilename::Ptr& fnAttr) const = 0;

    /*!
        Returns a command that will convert the path of a filename attribute to a relative path.
        \param fnAttr Filename attribute to convert.
        \return The command, or nullptr if conversion is not required or impossible to do.
    */
    [[nodiscard]] virtual Ufe::UndoableCommand::Ptr convertPathToRelativeCmd(
        const Ufe::AttributeFilename::Ptr& fnAttr) const = 0;

    /*!
        Returns a command that will set the path of a filename attribute using the current absolute/relative user
        preference.
        \param fnAttr Filename attribute to set.
        \param path New path value.
        \return The command, or nullptr in case of an error.
    */
    [[nodiscard]] virtual Ufe::UndoableCommand::Ptr setPreferredPathCmd(const Ufe::AttributeFilename::Ptr& fnAttr,
                                                                        const std::string& path) const = 0;

    /*!
        Opens a file picker dialog.
        \param fnAttr Destination attribute where the path could be stored. Impacts the resolution.
        \return The picked file, in the preferredPath format.
    */
    [[nodiscard]] virtual std::string openFileDialog(const Ufe::AttributeFilename::Ptr& fnAttr) const = 0;

    // File selector dialog modes. Used to indicate what the dialog is to return and display.
    enum FileMode
    {
        anyFile,            // Any file, whether it exists or not.
        existingFile,       // A single existing file.
        directory,          // The name of a directory. Both directories and files are displayed in the dialog.
        directoryHideFiles, // The name of a directory. Only directories are displayed in the dialog.
        existingFiles       // The names of one or more existing files.
    };

    // Options for the file selector dialog.
    struct FileSelectorOptions
    {
        FileMode mode = FileMode::anyFile;     // Indicate what the dialog is to return.
        std::string caption = "Select a File"; // The title
        std::string okCaption = "Ok";          // The text for the OK button.
        std::string description = "";          // A description for additional info of the specific operation.
        std::string fileFilter = "";           // A Qt-styled filtering string for the files.
        std::string baseDirectory = "";        // The directory used to initialize the interface.
    };

    /*!
        Interface to return a File or Directory on disk.
        Usage: If the host is a dcc, it might open an import/export dialog. If the host is a backend, it might
        programmatically return a desired path.
        \param options The configuration of the interface.
        \return The result file path.
    */
    [[nodiscard]] virtual std::string invokeFileSelector(const FileSelectorOptions& options) const;

    /*!
        Checks if saving the list of files (fileNames) to directoryPath would overwrite existing ones.
        A dcc might ask the user for confirmation.
        \param directoryPath The save directory that is being validated.
        \param options The configuration of the file picker interface.
        \param fileNames The list of the names of the files to save. Used to check if any file name already
         exists in the selected directory.
        \return The validated directory.
    */
    [[nodiscard]] virtual std::string validateSaveDirectory(const std::string& directoryPath,
                                                            const FileSelectorOptions& options,
                                                            const std::vector<std::string>& fileNames) const;

    /**
     * @brief Tries to find an icon file in an efficient way using the DCC mechanisms.
     * @param filename filename we are seeking in the icon path.
     * @return The full path if the file exists, an empty string otherwise.
     *
     * @note Works better if you ask the DCC gateway since the nested runtime might not know where the icons live.
     */
    ///
    [[nodiscard]] virtual std::string resolveIconFilePath(const std::string& filename) const;

    /**
     * @brief Opens a DCC specific file dialog to pick an image file.
     * @param startPath location in the filesystem where the dialog should open. Can be left empty to let the system
     * decide.
     * @param canBeRelative true if relative path options should be enabled in the file browser.
     * @param relativePathAnchor anchor location for relative path calculations in the file dialog if supported by the
     * DCC.
     * @return The picked file, in the preferredPath format if an anchor was specified.
     *
     * @note Works better if you ask the DCC gateway since the nested runtime might not know how to open a file dialog.
     */
    ///
    [[nodiscard]] virtual std::string openImageFileDialog(const std::string& startPath,
                                                          bool canBeRelative,
                                                          const std::string& relativePathAnchor) const;

    /*!
        Requests a valid writable directory. By default it is a system temp location, but can be overriden by
        DCC runtimes to return project-specific locations.
        \return The directory path.
    */
    [[nodiscard]] virtual std::string writableDirectory() const;
};

} // LOOKDEVXUFE_NS_DEF
