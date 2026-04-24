// ======================================================================
//
// AbstractFile.cpp
// copyright (c) 2001,2002 Sony Online Entertainment
//
// ======================================================================

#include "fileInterface/FirstFileInterface.h"
#include "fileInterface/AbstractFile.h"

#include <assert.h>
#include <cstddef>
#include <cstdio>

// ======================================================================

namespace AbstractFileNamespace
{
	AbstractFile::AudioServeFunction s_audioServeFunction = NULL;
}

using namespace AbstractFileNamespace;

// ----------------------------------------------------------------------

void AbstractFile::setAudioServe(AudioServeFunction audioServeFunction)
{
	s_audioServeFunction = audioServeFunction;
}

// ======================================================================

AbstractFile::AbstractFile(PriorityType priority)
:
	m_priority(priority)
{
}

// ----------------------------------------------------------------------

AbstractFile::~AbstractFile()
{
}

// ----------------------------------------------------------------------

void AbstractFile::flush()
{
}

// ----------------------------------------------------------------------

byte *AbstractFile::readEntireFileAndClose()
{
	if (s_audioServeFunction != NULL)
		(*s_audioServeFunction)();

	seek(SeekBegin, 0);

	const int fileLength = length();
	// length() < 0 (e.g. FileStreamer in Release on null OsFile) must not pass to
	// new[] — on x64 a negative int becomes a huge size_t and corrupts the heap.
	if (fileLength < 0)
	{
		close();
		return NULL;
	}
	byte *buffer = new byte[static_cast<size_t>(fileLength)];
	const int bytesRead = read(buffer, fileLength);
	static_cast<void>(bytesRead);
	assert(bytesRead == fileLength);
	close();

	return buffer;
}

// ----------------------------------------------------------------------

bool AbstractFile::isZlibCompressed() const
{
	return false;
}

// ----------------------------------------------------------------------

int AbstractFile::getZlibCompressedLength() const
{
	return -1;
}

// ----------------------------------------------------------------------

void AbstractFile::getZlibCompressedDataAndClose(byte *& compressedBuffer, int & compressedBufferLength)
{
	compressedBuffer = NULL;
	compressedBufferLength = -1;
}

// ======================================================================

