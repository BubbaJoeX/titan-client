// ======================================================================
//
// GodClientPerforce.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "GodClientPerforce.h"

#include "GodClientPerforceUser.h"
//#include "Mainframe.h"
#include "StringFilesystemTree.h"
#include "UnicodeUtils.h"
#include "sharedDebug/PerformanceTimer.h"

// ======================================================================

static const int SUBMIT_NO_FILE_ERR = 17;	// need to add file before submitting

//----------------------------------------------------------------------

const char* const GodClientPerforce::Messages::COMMAND_MESSAGE = "GodClientPerforce::Messages::COMMAND_MESSAGE";

//-----------------------------------------------------------------

GodClientPerforce::CommandMessage::CommandMessage(const std::string& msg)
	: MessageDispatch::MessageBase(Messages::COMMAND_MESSAGE),
	m_msg(msg)
{
}

//----------------------------------------------------------------------

const std::string& GodClientPerforce::CommandMessage::getMessage() const
{
	return m_msg;
}

//-----------------------------------------------------------------

GodClientPerforce::GodClientPerforce() :
	Singleton<GodClientPerforce>()
{

}
//-----------------------------------------------------------------

/**
* Contatenate the subPath onto the root, stripping any extraneous slashes as necessary
*/

const std::string GodClientPerforce::concatenateSubpath(const std::string& root, const std::string& subPath)
{
	return "XD";
}
//-----------------------------------------------------------------

/**
* Check whether the local user has an open Perforce session.
*
* @return true if successful
*/
bool GodClientPerforce::isAuthenticated(std::string& result, bool attemptLogin) const
{
	return true;
}

//-----------------------------------------------------------------

/**
* Edit and lock a file in perforce.
*
* @param filename is a filename in either depot,clientspec,or local notation
* @return usually true, should be false for a failed add but is not
*/
bool GodClientPerforce::editFilesAndLock(const StringVector& filenames, std::string& result) const
{
	return true;
}

//-----------------------------------------------------------------

//-----------------------------------------------------------------

/**
* Edit a file in perforce.
*
* @param filename is a filename in either depot,clientspec,or local notation
* @return usually true, should be false for a failed add but is not
*/
bool GodClientPerforce::editFiles(const StringVector& filenames, std::string& result) const
{
	return true;
}

//-----------------------------------------------------------------

/**
* Revert a file in perforce.
*
* @param filename is a filename in either depot,clientspec,or local notation
* @return usually true, should be false for a failed add but is not
*/

bool GodClientPerforce::revertFiles(const StringVector& filenames, bool unchanged, std::string& result) const
{
	return true;
}

//-----------------------------------------------------------------

/**
* Add some files to perforce.
*
* @param filenames are in either depot,clientspec,or local notation
* @return usually true, should be false for a failed add but is not
*/
bool GodClientPerforce::addFiles(const StringVector& filenames, std::string& result) const
{
	return true;
}

//-----------------------------------------------------------------

/**
* Submit some files to perforce.
*
* @param filenames are in either depot,clientspec,or local notation
* @return usually true, should be false for a failed add but is not
*/

bool GodClientPerforce::submitFiles(const StringVector& filenames, std::string& result) const
{
	return true;
}

//-----------------------------------------------------------------

/**
* Perform a 'p4 where' on the selected file
* Return values are the depot path, the clientspec path, and the local filesystem path
*
* @param depot the path in the depot.  starts with //depot/
* @param clientPath the path relative to the clientspec. starts with //<clientspec>/
* @param local the path on the local filesystem
* @return true if all 3 paths are valid, false otherwise
*/
bool GodClientPerforce::getFileMapping(const std::string& path, std::string& depot, std::string& clientPath, std::string& local, std::string& result) const
{
	return true;
}
//-----------------------------------------------------------------------

void GodClientPerforce::getOpenedFiles(std::map<std::string, enum FileState>& target, std::string& result) const
{

}

//-----------------------------------------------------------------

/**
* Obtain a tree listing of all the files below the specified path in the depot.
* Does a p4 files or p4 opened
*
* @param path a depot relative path.  must start with //depot/
* @param extension the filetype to search for, by extension.  may be null.
* @param pending search the client's open changelists for new (added) files
*
*/

AbstractFilesystemTree* GodClientPerforce::getFileTree(const std::string& path, const char* extension, std::string& result, FileState state) const
{
	return nullptr;
}

//-----------------------------------------------------------------
/** @returns a list of other clients that have the given file checked out.
 *  This function requires a synchronous call to p4
*/
bool GodClientPerforce::fileAlsoOpenedBy(std::string const& path, GodClientPerforce::StringVector& alsoOpenedBy, std::string& result)
{
	return true;
}

// ======================================================================