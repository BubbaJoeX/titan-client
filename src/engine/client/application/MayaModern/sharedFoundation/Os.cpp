// ======================================================================
//
// Os.cpp
//
// Portions copyright 1998 Bootprint Entertainment
// Portions copyright 2000-2002 Sony Online Entertainment
// All Rights Reserved
//
// ======================================================================

#include "Os.h"

#include "Misc.h"
#include "Globals.h"

#include <stack>
#include <string>

#include <windows.h>
#include <wtypes.h>

//-----------------------------------------------------------------
/**
* Create the specified directory and all of it's parents.
* @param directory the path to a directory
* @return always true currently
*/

bool Os::createDirectories(const char* directory)
{
    //-- construct list of subdirectories all the way down to root
    std::stack<std::string> directoryStack;

    std::string currentDirectory = directory;

    static const char path_seps[] = { '\\', '/', 0 };

    // build the stack
    while (!currentDirectory.empty())
    {
        // remove trailing backslash
        if (currentDirectory[currentDirectory.size() - 1] == '\\' || currentDirectory[currentDirectory.size() - 1] == '/')
            IGNORE_RETURN(currentDirectory.erase(currentDirectory.size() - 1));

        if (currentDirectory[currentDirectory.size() - 1] == ':')
        {
            // we've hit something like c:
            break;
        }

        if (!currentDirectory.empty())
            directoryStack.push(currentDirectory);

        // now strip off current directory
        const size_t previousDirIndex = currentDirectory.find_last_of(path_seps);
        if (static_cast<int>(previousDirIndex) == currentDirectory.npos)
            break;
        else
            IGNORE_RETURN(currentDirectory.erase(previousDirIndex));
    }

    //-- build all directories specified by the initial directory
    while (!directoryStack.empty())
    {
        // get the directory
        currentDirectory = directoryStack.top();
        directoryStack.pop();

        // try to create it (don't pass any security attributes)
        IGNORE_RETURN(CreateDirectory(currentDirectory.c_str(), NULL));
    }

    return true;
}

// ----------------------------------------------------------------------
/**
 * Write out a file.
 *
 * The file name and where the file is written is system-dependent.
 *
 * @param fileName  Name of the file to write
 * @param data  Data buffer to write to the file
 */

bool Os::writeFile(const char* fileName, const void* data, int length)     // Length of the data bufferto write
{
    BOOL   result;
    HANDLE handle;
    DWORD  written;

    // open the file for writing
    handle = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    // check if it was opened
    if (handle == INVALID_HANDLE_VALUE)
    {
        WARNING(true, ("Os::writeFile unable to create file [%s] for writing.", fileName));
        // aconite 8/19/22
        // add extra error information from the failed result of CreateFileA
        DWORD errorMessageID = ::GetLastError();
        LPSTR messageBuffer = nullptr;
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);
        WARNING(true, ("CreateFileA Error: %s", message.c_str()));
        return false;
    }

    // attempt to write the data
    result = WriteFile(handle, data, static_cast<DWORD>(length), &written, NULL);

    // make sure the data was written okay
    if (!result || written != static_cast<DWORD>(length))
    {
        WARNING(true, ("Os::writeFile error  writing file [%s].  Wrote %d, attempted to write %d.", fileName, written, length));
        static_cast<void>(CloseHandle(handle));
        return false;
    }

    // close the file
    result = CloseHandle(handle);

    // make sure the close was sucessful
    if (!result)
    {
        WARNING(true, ("Os::writeFile error closing file [%s].", fileName));
        return false;
    }

    return true;
}