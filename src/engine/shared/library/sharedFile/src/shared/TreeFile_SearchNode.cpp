// ======================================================================
//
// TreeFile_SearchNode.cpp
// Portions copyright 1998 Bootprint Entertainment
// Portions copyright 2001-2002 Sony Online Entertainment
// All Rights Reserved.
//
// ======================================================================

#include "sharedFile/FirstSharedFile.h"
#include "sharedFile/TreeFile_SearchNode.h"
#include "sharedFile/TitanPakCrypto.h"

#include "sharedCompression/Compressor.h"
#include "sharedCompression/ZlibCompressor.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFile/FileStreamerFile.h"
#include "sharedFile/FileStreamer.h"
#include "sharedFile/MemoryFile.h"
#include "sharedFile/ZlibFile.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/Crc.h"
#include "sharedFoundation/MemoryBlockManager.h"
#include "sharedFoundation/Os.h"
#include "sharedFoundation/PersistentCrcString.h"
#include "sharedFoundation/PointerDeleter.h"
#include "sharedFoundation/Production.h"
#include "sharedFoundation/TemporaryCrcString.h"
#include "sharedSynchronization/Mutex.h"

#include <algorithm>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace
{
void titanSearchNodeOds(const char *const fmt, ...)
{
#ifdef _WIN32
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	(void)_vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
	va_end(ap);
	char line[600];
	_snprintf_s(line, sizeof(line), _TRUNCATE, "[Titan] SearchNode: %s\r\n", buf);
	OutputDebugStringA(line);
#else
	(void)fmt;
#endif
}

// MSVC: use %I64d + __int64 in varargs; %lld with long long can break _vsnprintf_s stack in some builds.
static size_t titanBoundedCStrLen(char const *p, size_t const cap)
{
	if (!p)
		return 0U;
	size_t n = 0U;
	while (n < cap && p[n] != '\0')
		++n;
	return n;
}

void titanOdsS32I64i(char const *const lead, int const a, long long const b)
{
#ifdef _WIN32
	char line[400];
	_snprintf_s(line, sizeof(line), _TRUNCATE, "[Titan] SearchNode: %s %d onDiskBytes=%I64d\r\n", lead, a, static_cast<__int64>(b));
	OutputDebugStringA(line);
#endif
}
} // namespace

// ======================================================================

namespace
{
	bool virtualPathMatchesPrefixAndSuffix(char const * const path, char const * const prefix, char const * const suffix)
	{
		if (!path || !prefix || !suffix)
			return false;

		size_t const prefixLen = strlen(prefix);
		if (_strnicmp(path, prefix, static_cast<int>(prefixLen)) != 0)
			return false;

		size_t const pathLen = strlen(path);
		size_t const suffixLen = strlen(suffix);
		if (pathLen < prefixLen + suffixLen)
			return false;

		return _stricmp(path + pathLen - suffixLen, suffix) == 0;
	}
}

// ======================================================================

const Tag TAG_NUNA = TAG(N, U, N, A);
const Tag TAG_TREE = TAG(T,R,E,E);
const Tag TAG_TOC  = TAG3(T,O,C);
const Tag TAG_NTOC = TAG(N,T,O,C);  // Encrypted titanlst format

// ======================================================================

TreeFile::SearchNode::SearchNode(int priority)
:
	m_priority(priority)
{
}

// ----------------------------------------------------------------------

TreeFile::SearchNode::~SearchNode(void)
{
}

// ----------------------------------------------------------------------

void TreeFile::SearchNode::collectVirtualPathNamesWithPrefixAndSuffix(char const *, char const *, std::vector<std::string> &, std::set<std::string> &) const
{
}

// ======================================================================

TreeFile::SearchPath::SearchPath(int priority, const char *path)
: SearchNode(priority),
	m_pathName(NULL),
	m_pathNameLength(0)
{
	NOT_NULL(path);
	DEBUG_FATAL(!path[0], ("empty path"));
	titanSearchNodeOds("SearchPath begin p=%d path=%s", priority, path);

	// convert from relative to absolute path
	char absolutePath[Os::MAX_PATH_LENGTH];
	const bool result = Os::getAbsolutePath(path, absolutePath, sizeof(absolutePath));
	FATAL(!result, ("Could not convert to absolute path.  Does it exist?  %s", path));

	// clean the path name up and remove any trailing slash
	// this function will remove leading slashes but we actually want them, so make sure we preserve it
	int const offset = (absolutePath[0] == '/') ? 1 : 0;
	TreeFile::fixUpFileName(absolutePath+offset, absolutePath+offset);
	m_pathNameLength = strlen(absolutePath);
	if (m_pathNameLength && absolutePath[m_pathNameLength-1] == '/')
		absolutePath[--m_pathNameLength] = '\0';

	// copy the path to a safe place
	m_pathName = DuplicateString(absolutePath);
	titanSearchNodeOds("SearchPath ok p=%d abs=%s", priority, m_pathName);
}

// ----------------------------------------------------------------------

TreeFile::SearchPath::~SearchPath(void)
{
	delete [] m_pathName;
}

// ----------------------------------------------------------------------

void TreeFile::SearchPath::debugPrint(void)
{
	DEBUG_REPORT_PRINT(true, ("  %d=priority %s=path\n", getPriority(), m_pathName));
	DEBUG_OUTPUT_STATIC_VIEW("Foundation\\Treefile", ("  %d=priority %s=path\n", getPriority(), m_pathName));
}

// ----------------------------------------------------------------------

void TreeFile::SearchPath::makeAbsolutePath(const char *fileName, char *buffer) const
{
	NOT_NULL(fileName);
	NOT_NULL(buffer);
	DEBUG_FATAL(strlen(m_pathName) + 1 + strlen(fileName) + 1 > Os::MAX_PATH_LENGTH, ("file name to long %d/%d", strlen(m_pathName) + strlen(fileName) + 1, Os::MAX_PATH_LENGTH));

	strcpy(buffer, m_pathName);
	buffer[m_pathNameLength] = '/';
	strcpy(buffer+m_pathNameLength+1, fileName);
}

// ----------------------------------------------------------------------

bool TreeFile::SearchPath::exists(const char *fileName, bool &) const 
{
	char buffer[Os::MAX_PATH_LENGTH];
	makeAbsolutePath(fileName, buffer);
	return FileStreamer::exists(buffer);
}

// ----------------------------------------------------------------------

int TreeFile::SearchPath::getFileSize(const char *fileName, bool &) const 
{
	char buffer[Os::MAX_PATH_LENGTH];
	makeAbsolutePath(fileName, buffer);
	return FileStreamer::getFileSize(buffer);
}

// ----------------------------------------------------------------------

void TreeFile::SearchPath::getPathName(const char *fileName, char *outPathName, int outPathNameLength) const
{
	UNREF(outPathNameLength);
	DEBUG_FATAL(istrlen(m_pathName) + 1 + istrlen(fileName) + 1 > outPathNameLength, ("file name too long %d/%d", strlen(m_pathName) + 1 + strlen(fileName) + 1, outPathNameLength));

#ifdef _DEBUG
	bool deleted = false;
	DEBUG_FATAL(!exists(fileName, deleted), ("file name doesn't exist"));
#endif

	makeAbsolutePath(fileName, outPathName);
}

// ----------------------------------------------------------------------

AbstractFile *TreeFile::SearchPath::open(const char *fileName, AbstractFile::PriorityType priority, bool &)
{
	char buffer[Os::MAX_PATH_LENGTH];
	makeAbsolutePath(fileName, buffer);
	FileStreamer::File *file = FileStreamer::open(buffer);
	if (!file)
		return NULL;
	return new FileStreamerFile(priority, *file);
}

// ----------------------------------------------------------------------

void TreeFile::SearchPath::collectVirtualPathNamesWithPrefixAndSuffix(char const * prefix, char const * suffix, std::vector<std::string> & out, std::set<std::string> & seen) const
{
#ifdef _WIN32
	if (!prefix || !suffix || _stricmp(suffix, ".trn") != 0)
		return;

	std::string terrainDir(m_pathName);
	terrainDir += "\\terrain";
	std::string pattern = terrainDir + "\\*.trn";

	WIN32_FIND_DATAA fd;
	HANDLE const h = FindFirstFileA(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		std::string virt("terrain/");
		virt += fd.cFileName;
		if (!virtualPathMatchesPrefixAndSuffix(virt.c_str(), prefix, suffix))
			continue;

		if (seen.insert(virt).second)
			out.push_back(virt);
	} while (FindNextFileA(h, &fd) != 0);

	FindClose(h);
#else
	UNREF(prefix);
	UNREF(suffix);
	UNREF(out);
	UNREF(seen);
#endif
}

// ======================================================================

TreeFile::SearchAbsolute::SearchAbsolute(int priority)
: SearchNode(priority)
{
}

// ----------------------------------------------------------------------

TreeFile::SearchAbsolute::~SearchAbsolute(void)
{
}

// ----------------------------------------------------------------------

void TreeFile::SearchAbsolute::debugPrint(void)
{
}

// ----------------------------------------------------------------------

bool TreeFile::SearchAbsolute::exists(const char *fileName, bool &) const
{
	DEBUG_FATAL(strlen(fileName) + 1 > Os::MAX_PATH_LENGTH, ("Filename too long %d/%d", strlen(fileName) + 1, Os::MAX_PATH_LENGTH));
	return FileStreamer::exists(fileName);
}

// ----------------------------------------------------------------------

int TreeFile::SearchAbsolute::getFileSize(const char *fileName, bool &) const
{
	DEBUG_FATAL(strlen(fileName) + 1 > Os::MAX_PATH_LENGTH, ("Filename too long %d/%d", strlen(fileName) + 1, Os::MAX_PATH_LENGTH));
	return FileStreamer::getFileSize(fileName);
}

// ----------------------------------------------------------------------

void TreeFile::SearchAbsolute::getPathName(const char *fileName, char *pathName, int pathNameLength) const
{
	NOT_NULL(fileName);
	NOT_NULL(pathName);
#ifdef _DEBUG
	bool deleted = false;
	DEBUG_FATAL(!exists(fileName, deleted), ("fileName does not exist"));
#endif

	const int fnLen = istrlen(fileName);
	FATAL(fnLen + 1 > pathNameLength, ("SearchAbsolute::getPathName: name too long %d/%d", fnLen + 1, pathNameLength));
	strcpy(pathName, fileName);
}

// ----------------------------------------------------------------------

AbstractFile *TreeFile::SearchAbsolute::open(const char *fileName, AbstractFile::PriorityType priority, bool &)
{
	NOT_NULL(fileName);
	DEBUG_FATAL(strlen(fileName) + 1 > Os::MAX_PATH_LENGTH, ("Filename too long %d/%d", strlen(fileName) + 1, Os::MAX_PATH_LENGTH));

	FileStreamer::File *file = FileStreamer::open(fileName);
	if (!file)
		return NULL;
	return new FileStreamerFile(priority, *file);
}


// ======================================================================

bool TreeFile::SearchTree::validate(const char *fileName)
{
	// open the file
	FileStreamer::File *file = FileStreamer::open(fileName);
	if (!file)
		return false;

	// read the header
	Header header;
	const int readPos = file->read(0, &header, sizeof(header), AbstractFile::PriorityData);

	// close the file
	delete file;

	// make sure all the bytes were read
	if (readPos != isizeof(header))
		return false;

	// validate the token - accept both TREE and NUNA (encrypted)
	if (header.token != TAG_TREE && header.token != TAG_NUNA)
		return false;

	// validate the version number
	if (header.version < TAG_0004 || header.version > TAG_0005)
		return false;

	return true;
}

// ----------------------------------------------------------------------

TreeFile::SearchTree::SearchTree(int priority, const char *fileName)
: SearchNode(priority),
	m_treeFileName(NULL),
	m_treeFile(NULL),
	m_version(0),
	m_numberOfFiles(0),
	m_fileNames(NULL),
	m_tableOfContents(NULL),
	m_encrypted(false),
	m_encryptionContext(NULL)
{
	NOT_NULL(fileName);
	titanSearchNodeOds("SearchTree ctor begin p=%d file=%s", priority, fileName);

	// set to the the current name of the tree file that is fileName
	m_treeFileName = DuplicateString(fileName);

	m_treeFile = FileStreamer::open(m_treeFileName, true);
	DEBUG_FATAL(!m_treeFile, ("failed to open TreeFile %s", m_treeFileName));
	titanSearchNodeOds("SearchTree opened %s", m_treeFileName);

	// read the header (the first 36 bytes of the tree file) 
	Header header;
	m_treeFile->read(0, &header, sizeof(header), AbstractFile::PriorityData);
	DEBUG_FATAL(header.token != TAG_TREE && header.token != TAG_NUNA, ("file does not look like a tree file or titanpak"));

	// Check if this is an encrypted TitanPak archive
	m_encrypted = (header.token == TAG_NUNA);
	
	// set to the number of files that has been compressed within the tree file
	m_numberOfFiles = static_cast<int>(header.numberOfFiles);
	titanSearchNodeOds("SearchTree header ok file=%s files=%d tocOff=%u enc=%d tocComp=%u nameComp=%u", m_treeFileName, m_numberOfFiles, header.tocOffset, m_encrypted ? 1 : 0, header.tocCompressor, header.blockCompressor);
	
	if (m_encrypted)
	{
		// Read the encryption header
		TitanPakCrypto::EncryptionHeader encHeader;
		m_treeFile->read(sizeof(header), &encHeader, sizeof(encHeader), AbstractFile::PriorityData);
		
		// Use the hardcoded encryption password
		const char* password = TitanPakCrypto::getPassword();
		
		// Initialize encryption context
		m_encryptionContext = new TitanPakEncryptionContext();
		m_encryptionContext->initialize(password, encHeader);
		titanSearchNodeOds("SearchTree TitanPak crypto init done %s", m_treeFileName);
		
		DEBUG_REPORT_LOG(true, ("Loaded encrypted TitanPak: %s (files=%d, tocOffset=%u, tocComp=%u, nameComp=%u)\n", 
			m_treeFileName, m_numberOfFiles, header.tocOffset, header.tocCompressor, header.blockCompressor));
	}
	else
	{
		DEBUG_REPORT_LOG(true, ("Loaded standard TreeFile: %s (files=%d, tocOffset=%u)\n", m_treeFileName, m_numberOfFiles, header.tocOffset));
	}

	int readPosition = header.tocOffset;
	m_version = header.version;
	switch (m_version)
	{
		case TAG_0004:
		case TAG_0005:
			{
				m_tableOfContents = new TableOfContentsEntry[m_numberOfFiles];
				m_fileNames = new char [header.uncompSizeOfNameBlock];

				// prepare table of contents by zeroing out the total size of data to be stored
				const int tableOfContentsSize = isizeof(TableOfContentsEntry) * m_numberOfFiles;
				memset(m_tableOfContents, 0, tableOfContentsSize);

				if (isCompressed(header.tocCompressor))
				{
					titanSearchNodeOds("SearchTree read compressed TOC %s bytes=%u", m_treeFileName, static_cast<unsigned int>(header.sizeOfTOC));
					// create temp buffer to store the compressed TOC entry data
					byte *entryBuffer = new byte[static_cast<uint32>(header.sizeOfTOC)];
				
					// read the compressed table of contents data into buffer
					const int bytesRead = m_treeFile->read(readPosition, entryBuffer, header.sizeOfTOC, AbstractFile::PriorityData);					
					DEBUG_FATAL(bytesRead != static_cast<int>(header.sizeOfTOC), ("failed to read tree file TOC entries"));
					readPosition += bytesRead;

					// decrypt if encrypted TitanPak
					if (m_encrypted && m_encryptionContext)
					{
						m_encryptionContext->decryptAt(entryBuffer, header.sizeOfTOC, header.tocOffset);
					}

					// decompress data into toc 
					titanSearchNodeOds("SearchTree Zlib expand TOC %s", m_treeFileName);
					static_cast<void>(ZlibCompressor().expand(entryBuffer, header.sizeOfTOC, m_tableOfContents, tableOfContentsSize));
					titanSearchNodeOds("SearchTree TOC expand done %s", m_treeFileName);

					delete [] entryBuffer;
				}
				else
				{
					titanSearchNodeOds("SearchTree read uncompressed TOC %s bytes=%d", m_treeFileName, tableOfContentsSize);
					// read the uncompressed table of contents data
					const int bytesRead = m_treeFile->read(header.tocOffset, m_tableOfContents, tableOfContentsSize, AbstractFile::PriorityData);
					DEBUG_FATAL(bytesRead != tableOfContentsSize, ("failed to read tree file tableOfContents entries"));
					readPosition += bytesRead;
					
					// decrypt if encrypted TitanPak
					if (m_encrypted && m_encryptionContext)
					{
						m_encryptionContext->decryptAt(reinterpret_cast<uint8_t*>(m_tableOfContents), tableOfContentsSize, header.tocOffset);
					}
					titanSearchNodeOds("SearchTree uncompressed TOC read done %s", m_treeFileName);
				}

				if (header.blockCompressor)
				{
					titanSearchNodeOds("SearchTree read compressed name block %s bytes=%u", m_treeFileName, static_cast<unsigned int>(header.sizeOfNameBlock));
					// create temp buffer to store the compressed name block data
					byte *nameBuffer  = new byte[static_cast<uint32>(header.sizeOfNameBlock)];

					// read the compressed table of contents data into buffer
					const int bytesRead = m_treeFile->read(readPosition, nameBuffer, header.sizeOfNameBlock, AbstractFile::PriorityData);					
					UNREF(bytesRead);
					DEBUG_FATAL(bytesRead != static_cast<int>(header.sizeOfNameBlock), ("failed to read tree file name block"));

					// decrypt if encrypted TitanPak
					if (m_encrypted && m_encryptionContext)
					{
						m_encryptionContext->decryptAt(nameBuffer, header.sizeOfNameBlock, readPosition);
					}

					// decompress data into tocFileNames 
					titanSearchNodeOds("SearchTree Zlib expand name block %s", m_treeFileName);
					static_cast<void>(ZlibCompressor().expand(nameBuffer, header.sizeOfNameBlock, m_fileNames, header.uncompSizeOfNameBlock));
					titanSearchNodeOds("SearchTree name block expand done %s", m_treeFileName);
					
					delete [] nameBuffer;
				}
				else
				{
					titanSearchNodeOds("SearchTree read uncompressed name block %s bytes=%u", m_treeFileName, header.uncompSizeOfNameBlock);
					// read the uncompressed name block data 
					const int bytesRead = m_treeFile->read(readPosition, m_fileNames, header.uncompSizeOfNameBlock, AbstractFile::PriorityData);
					UNREF(bytesRead);
					DEBUG_FATAL(bytesRead != static_cast<int>(header.uncompSizeOfNameBlock), ("failed to read tree file name block"));
					
					
					// decrypt if encrypted TitanPak
					if (m_encrypted && m_encryptionContext)
					{
						m_encryptionContext->decryptAt(reinterpret_cast<uint8_t*>(m_fileNames), header.uncompSizeOfNameBlock, readPosition);
					}
					titanSearchNodeOds("SearchTree uncompressed name block read done %s", m_treeFileName);
				}

				// Debug: show first few files in the TOC to verify decryption worked
				if (m_numberOfFiles > 0)
				{
					int samplesToShow = (m_numberOfFiles > 10) ? 10 : m_numberOfFiles;
					WARNING(true, ("=== TitanPak/TreeFile '%s' loaded ===", m_treeFileName));
					WARNING(true, ("    Encrypted: %s", m_encrypted ? "YES" : "NO"));
					WARNING(true, ("    Version: 0x%08X", m_version));
					WARNING(true, ("    File count: %d", m_numberOfFiles));
					WARNING(true, ("    TOC offset: %u", header.tocOffset));
					WARNING(true, ("    TOC compressor: %u", header.tocCompressor));
					WARNING(true, ("    Name block compressor: %u", header.blockCompressor));
					WARNING(true, ("    First %d files in archive:", samplesToShow));
					for (int i = 0; i < samplesToShow; ++i)
					{
						const char* fileName = m_fileNames + m_tableOfContents[i].fileNameOffset;
						WARNING(true, ("      [%d] crc=0x%08X offset=%d len=%d compLen=%d name='%s'", 
							i, m_tableOfContents[i].crc, m_tableOfContents[i].offset, 
							m_tableOfContents[i].length, m_tableOfContents[i].compressedLength, fileName));
					}
					WARNING(true, ("=== End TitanPak/TreeFile '%s' ===", m_treeFileName));
				}
			}
			titanSearchNodeOds("SearchTree ctor end ok %s files=%d", m_treeFileName, m_numberOfFiles);
			break;

		default:
			{
				delete m_treeFile;
				delete m_encryptionContext;
				m_encryptionContext = NULL;

#if PRODUCTION
				FATAL(true, ("TreeFile corruption detected.  Please do a \"Full Scan\" from the launchpad. (%08x %s)", m_version, m_treeFileName));
#else
				FATAL(true, ("unsupported version %d in %s", m_version, m_treeFileName));
#endif

			}
			break;
	}
}

// ----------------------------------------------------------------------
/**
 * Look for a file in the SearchTree.
 * 
 * If index is passed, the table of contents index is assigned to it.
 * 
 * @return True if found, otherwise false.
 */

bool TreeFile::SearchTree::localExists(const char *fileName, int *index, bool &deleted) const
{
	DEBUG_FATAL(strlen(fileName) + 1 > Os::MAX_PATH_LENGTH,("file name too long %d/%d", strlen(fileName) + 1, Os::MAX_PATH_LENGTH));

	const uint32 crc = Crc::calculate(fileName);

	// try a binary search through the tree file to find the file
	bool found       = false;
	int  left        = 0;
	int  right       = m_numberOfFiles - 1;
	int  mid         = 0;
	while (!found && (left <= right))
	{
		mid = (left + right) / 2;

		if (m_tableOfContents[mid].crc < crc)
			left = mid + 1;
		else
			if (m_tableOfContents[mid].crc > crc)
				right = mid - 1;
			else
			{
				const int res = _stricmp(m_fileNames + m_tableOfContents[mid].fileNameOffset, fileName);

				if (res < 0)
					left = mid + 1;
				else
					if (res > 0)
						right = mid - 1;
					else
						found = true;
			}
	}

	// return the found index if desired
	if (found)
	{
		DEBUG_REPORT_LOG(true, ("SearchTree::localExists FOUND '%s' in '%s' (crc=0x%08X, idx=%d, len=%d, offset=%d)\n", 
			fileName, m_treeFileName, crc, mid, m_tableOfContents[mid].length, m_tableOfContents[mid].offset));
		
		if (m_tableOfContents[mid].length == 0)
		{
			deleted = true;
			return false;
		}

		if (index)
			*index = mid;
	}


	return found;
}

// ----------------------------------------------------------------------

TreeFile::SearchTree::~SearchTree(void)
{
	delete [] m_treeFileName;
	delete [] m_tableOfContents;
	delete [] m_fileNames;
	delete m_treeFile;
	delete m_encryptionContext;
}

// ----------------------------------------------------------------------

void TreeFile::SearchTree::debugPrint(void)
{
	DEBUG_REPORT_PRINT(true, ("  %d=priority %s=tree\n", getPriority(), m_treeFileName));
	DEBUG_OUTPUT_STATIC_VIEW("Foundation\\Treefile", ("  %d=priority %s=tree\n", getPriority(), m_treeFileName));
}

// ----------------------------------------------------------------------
/**
 * Look for a file in the SearchTree.
 * 
 * @return True if found, otherwise false.
 */

bool TreeFile::SearchTree::exists(const char *fileName, bool &deleted) const
{
	NOT_NULL(fileName);
	return localExists(fileName, NULL, deleted);
}

// ----------------------------------------------------------------------

int TreeFile::SearchTree::getFileSize(const char *fileName, bool &deleted) const
{
	NOT_NULL(fileName);
	int tableOfContentsIndex = 0;
	if (!localExists(fileName, &tableOfContentsIndex, deleted))
		return -1;

	return m_tableOfContents[tableOfContentsIndex].length;
}

// ----------------------------------------------------------------------

void TreeFile::SearchTree::getPathName(const char *fileName, char *pathName, int pathNameLength)  const
{
	NOT_NULL(fileName);
	NOT_NULL(pathName);

#ifdef _DEBUG
	bool deleted = false;
	DEBUG_FATAL(!exists(fileName, deleted), ("fileName does not exist"));
#endif

	const int stringLength = istrlen(m_treeFileName) + 1 + istrlen(fileName) + 1 + 1;
	FATAL(stringLength > pathNameLength, ("SearchTree::getPathName: name too long %d/%d", stringLength, pathNameLength));

	// make a pseudo-path name for the tree file
	strcpy(pathName, m_treeFileName);
	strcat(pathName, "[");
	strcat(pathName, fileName);
	strcat(pathName, "]");
}

// ----------------------------------------------------------------------

AbstractFile *TreeFile::SearchTree::open(const char *fileName, AbstractFile::PriorityType priority, bool &deleted)
{
	NOT_NULL(fileName);
	DEBUG_FATAL(strlen(fileName) + 1 > Os::MAX_PATH_LENGTH,("file name too long %d/%d", strlen(fileName) + 1, Os::MAX_PATH_LENGTH));
	int tableOfContentsIndex = -1;
	if (localExists(fileName, &tableOfContentsIndex, deleted))
	{
		const TableOfContentsEntry &entry = m_tableOfContents[tableOfContentsIndex];

		DEBUG_REPORT_LOG(true, ("TreeFile::open '%s' from '%s' (size=%d, compressed=%d, encrypted=%s)\n",
			fileName, m_treeFileName, entry.length, entry.compressedLength, m_encrypted ? "yes" : "no"));

		if (!TreeFile::SearchTree::isCompressed(entry.compressor))
		{
			// Uncompressed file
			if (m_encrypted && m_encryptionContext)
			{
				// For encrypted archives, we need to read and decrypt the data
				byte *buffer = new byte[entry.length];
				const int bytesRead = m_treeFile->read(entry.offset, buffer, entry.length, priority);
				DEBUG_FATAL(bytesRead != entry.length, ("error reading data into buffer"));
				UNREF(bytesRead);
				
				// Decrypt the data
				m_encryptionContext->decryptAt(buffer, entry.length, entry.offset);
				
				// Return a memory file with the decrypted data
				// Note: MemoryFile takes ownership of the buffer
				return new MemoryFile(buffer, entry.length);
			}
			else
			{
				// Standard unencrypted file - use direct file streaming
				return new FileStreamerFile(priority, *m_treeFile, entry.offset, entry.length);
			}
		}

		// Compressed file
		byte * compressedBuffer = new byte[entry.compressedLength];

		const int bytesRead = m_treeFile->read(entry.offset, compressedBuffer, entry.compressedLength, priority);
		DEBUG_FATAL(bytesRead != entry.compressedLength, ("error reading compressed data into buffer"));
		UNREF(bytesRead);

		// Decrypt if encrypted TitanPak
		if (m_encrypted && m_encryptionContext)
		{
			m_encryptionContext->decryptAt(compressedBuffer, entry.compressedLength, entry.offset);
		}

		return new ZlibFile(entry.length, compressedBuffer, entry.compressedLength, true);
	}

	return NULL;
}

// ----------------------------------------------------------------------

void TreeFile::SearchTree::collectVirtualPathNamesWithPrefixAndSuffix(char const * prefix, char const * suffix, std::vector<std::string> & out, std::set<std::string> & seen) const
{
	for (int i = 0; i < m_numberOfFiles; ++i)
	{
		TableOfContentsEntry const & entry = m_tableOfContents[i];
		if (entry.length == 0 || entry.offset == 0)
			continue;

		char const * const name = m_fileNames + entry.fileNameOffset;
		if (!virtualPathMatchesPrefixAndSuffix(name, prefix, suffix))
			continue;

		if (seen.insert(name).second)
			out.push_back(std::string(name));
	}
}

// ======================================================================

bool TreeFile::SearchTOC::validate(const char *fileName)
{
	// open the file
	FileStreamer::File *file = FileStreamer::open(fileName);
	if (!file)
		return false;

	// read the header
	Header header;
	const int readPos = file->read(0, &header, sizeof(header), AbstractFile::PriorityData);

	// close the file
	delete file;

	// make sure all the bytes were read
	if (readPos != isizeof(header))
		return false;

	// validate the token - accept both TOC (unencrypted) and NTOC (encrypted)
	if (header.token != TAG_TOC && header.token != TAG_NTOC)
		return false;

	// validate the version number
	if (header.version != TAG_0001)
		return false;

	return true;
}

// ----------------------------------------------------------------------

TreeFile::SearchTOC::SearchTOC(int priority, const char *fileName)
: SearchNode(priority),
	m_TOCFileName(NULL),
	m_TOCFile(NULL),
	m_treeFiles(NULL),
	m_encryptionContexts(NULL),
	m_tocEncryptionContext(NULL),
	m_numberOfTreeFiles(0),
	m_numberOfFiles(0),
	m_treeFileNames(NULL),
	m_treeFileNamePointers(NULL),
	m_tableOfContents(NULL),
	m_fileNames(NULL)
{
	NOT_NULL(fileName);
	titanSearchNodeOds("SearchTOC ctor begin file=%s", fileName);

	// set to the the current name of the TOC file to the fileName
	m_TOCFileName = DuplicateString(fileName);

	m_TOCFile = FileStreamer::open(m_TOCFileName, true);
	DEBUG_FATAL(!m_TOCFile, ("failed to open TOCFile %s", m_TOCFileName));
	long long const tocBytesOnDisk = m_TOCFile->length64();
	FATAL(tocBytesOnDisk < 0, ("SearchTOC: GetFileSizeEx/length64 failed for %s", m_TOCFileName));
	titanSearchNodeOds("SearchTOC open ok %s bytes=%I64d", m_TOCFileName, static_cast<__int64>(tocBytesOnDisk));

	// read the header
	Header header;
	m_TOCFile->read(0, &header, sizeof(header), AbstractFile::PriorityData);
	DEBUG_FATAL(header.token != TAG_TOC && header.token != TAG_NTOC, ("file does not look like a table of contents file (token=0x%08X)", header.token));

	// Check if this is an encrypted titanlst file
	bool tocEncrypted = (header.token == TAG_NTOC);

	// grab the number of files
	m_numberOfFiles = header.numberOfFiles;

	// grab the number of tree files
	m_numberOfTreeFiles = header.numberOfTreeFiles;
	titanSearchNodeOds("SearchTOC header nFiles=%u nTree=%u compTOC=%u compNames=%u", header.numberOfFiles, header.numberOfTreeFiles, header.tocCompressor, header.fileNameBlockCompressor);

	// Reject pathological or corrupt field combinations before we allocate 100MB+ in one shot.
	{
		int64_t const tocFileLen = static_cast<int64_t>(tocBytesOnDisk);
		// 512 MiB cap on single name block; adjust if a future SKU justifies it.
		uint32 const kMaxNameBlock = 512U * 1024U * 1024U;
		FATAL(header.uncompSizeOfNameBlock > kMaxNameBlock, ("SearchTOC: uncomp name block %u too large (corrupt header?) in %s", header.uncompSizeOfNameBlock, m_TOCFileName));
		// Uncompressed: read() must not write past our allocation. Compressed: expanded size is uncomp.
		if (!SearchTOC::isCompressed(static_cast<int>(header.fileNameBlockCompressor)))
		{
			FATAL(
				header.sizeOfNameBlock > header.uncompSizeOfNameBlock,
				("SearchTOC: on-disk name block %u > uncompSize %u (heap overflow) in %s", header.sizeOfNameBlock, header.uncompSizeOfNameBlock, m_TOCFileName));
		}
		int64_t const tocTableBytes = static_cast<int64_t>(sizeof(TableOfContentsEntry)) * static_cast<int64_t>(m_numberOfFiles);
		// 256 MiB cap on one TOC table (nFiles * record size).
		int64_t const kMaxTocTable = static_cast<int64_t>(256) * 1024 * 1024;
		FATAL(tocTableBytes > kMaxTocTable, ("SearchTOC: TOC table too large (%u files, %I64d bytes) in %s", m_numberOfFiles, static_cast<__int64>(tocTableBytes), m_TOCFileName));
		FATAL(tocTableBytes > static_cast<int64_t>(INT_MAX), ("SearchTOC: TOC table int overflow; %u files in %s", m_numberOfFiles, m_TOCFileName));
		// Coarse: file must be large enough for tree name block, TOC region, and on-disk name block.
		int const readPosAfterHeader = tocEncrypted ? (isizeof(Header) + static_cast<int>(sizeof(TitanPakCrypto::EncryptionHeader))) : isizeof(Header);
		int64_t const tocPayloadBytes = SearchTOC::isCompressed(static_cast<int>(header.tocCompressor)) ? static_cast<int64_t>(header.sizeOfTOC) : tocTableBytes;
		int64_t const needLo =
			static_cast<int64_t>(readPosAfterHeader) + static_cast<int64_t>(header.sizeOfTreeFileNameBlock) + tocPayloadBytes
			+ static_cast<int64_t>(header.sizeOfNameBlock);
		FATAL(needLo > tocFileLen, ("SearchTOC: file too small (len=%I64d, need>=%I64d) %s", static_cast<__int64>(tocFileLen), static_cast<__int64>(needLo), m_TOCFileName));
	}

	// set the read position to after the header (beginning of patch tree names)
	int readPosition = isizeof(Header);
	
	// If encrypted, read the encryption header and initialize context
	if (tocEncrypted)
	{
		TitanPakCrypto::EncryptionHeader encHeader;
		m_TOCFile->read(readPosition, &encHeader, sizeof(encHeader), AbstractFile::PriorityData);
		readPosition += sizeof(encHeader);
		
		const char* password = TitanPakCrypto::getPassword();
		m_tocEncryptionContext = new TitanPakEncryptionContext();
		m_tocEncryptionContext->initialize(password, encHeader);
		
		DEBUG_REPORT_LOG(true, ("SearchTOC: Loading encrypted titanlst file: %s\n", m_TOCFileName));
	}
	
	uint32 version = header.version;
	switch (version)
	{
		case TAG_0001:
		{
		m_tableOfContents = new TableOfContentsEntry [m_numberOfFiles];
		m_fileNames = new char [header.uncompSizeOfNameBlock];
		m_treeFileNames = new char [header.sizeOfTreeFileNameBlock];
		m_treeFiles = new FileStreamer::File* [header.numberOfTreeFiles];
		m_treeFileNamePointers = new char* [header.numberOfTreeFiles];
		m_encryptionContexts = new TitanPakEncryptionContext* [header.numberOfTreeFiles];

		{
					// get any paths we need to check to open the tree files
					// Fixed storage (no std::vector): in x64/Release we saw a failure mode after
					// "tree open done" and before the next ODS, consistent with heap corruption
					// or unsafe teardown when the path vector is destroyed.
					enum { kMaxTocTreePathExtraKeys = 64 };
					char const* treePathEntries[2 + kMaxTocTreePathExtraKeys];
					int numTreePathEntries = 0;
					treePathEntries[numTreePathEntries++] = "";
					treePathEntries[numTreePathEntries++] = "tres/";

					// add on all paths in config file
					for (int index = 0;; ++index)
					{
						char const* const result = ConfigFile::getKeyString("SharedFile", "TOCTreePath", index, NULL);
						if (result == NULL)
							break;
						FATAL(
							numTreePathEntries >= static_cast<int>(sizeof(treePathEntries) / sizeof(treePathEntries[0])),
							("SearchTOC: too many TOCTreePath keys in SharedFile (max %d) %s", kMaxTocTreePathExtraKeys, m_TOCFileName));
						treePathEntries[numTreePathEntries++] = result;
					}

					char * const treePathBuffer = new char[Os::MAX_PATH_LENGTH];

					// read in the tree file names and open the files
					// Track the offset where tree names start for decryption
					const int treeNamesOffset = readPosition;
					const int bytesRead = m_TOCFile->read(readPosition, m_treeFileNames, header.sizeOfTreeFileNameBlock, AbstractFile::PriorityData);
					FATAL(bytesRead != static_cast<int>(header.sizeOfTreeFileNameBlock), ("SearchTOC: failed to read tree file name block (got %d need %u) %s", bytesRead, header.sizeOfTreeFileNameBlock, m_TOCFileName));
					readPosition += bytesRead;
					
					// Decrypt tree file names if the titanlst is encrypted
					if (m_tocEncryptionContext && m_tocEncryptionContext->isInitialized())
					{
						m_tocEncryptionContext->decryptAt(reinterpret_cast<uint8_t*>(m_treeFileNames), header.sizeOfTreeFileNameBlock, treeNamesOffset);
					}

					for (int treeFileNameIndex = 0, treeFileNameReadPosition = 0; treeFileNameIndex < static_cast<int>(header.numberOfTreeFiles); treeFileNameIndex++)
					{
					m_treeFileNamePointers[treeFileNameIndex] = (m_treeFileNames + treeFileNameReadPosition);
					m_treeFiles[treeFileNameIndex] = NULL;
					m_encryptionContexts[treeFileNameIndex] = NULL;

					int const nameBytesRemaining = static_cast<int>(header.sizeOfTreeFileNameBlock) - treeFileNameReadPosition;
					if (nameBytesRemaining <= 0)
					{
						FATAL(true, ("SearchTOC: tree name block exhausted before tree index %d (pos %d, size %u)", treeFileNameIndex, treeFileNameReadPosition, header.sizeOfTreeFileNameBlock));
					}
					// try to open the tree file in each of the relative paths
					for (int pathIdx = 0; pathIdx < numTreePathEntries; ++pathIdx)
					{
					char const* const pathPrefix = treePathEntries[pathIdx];
					// Use %.*s: tree-name bytes are not proven null-terminated here; a bare %s can read past
					// the name block and access-violate (0xC0000005) during snprintf.
					(void)snprintf(
						treePathBuffer,
						static_cast<size_t>(Os::MAX_PATH_LENGTH),
						"%s%.*s",
						pathPrefix,
						nameBytesRemaining,
						m_treeFileNames + treeFileNameReadPosition);

					if (FileStreamer::exists (treePathBuffer))
					{
					m_treeFiles[treeFileNameIndex] = FileStreamer::open(treePathBuffer, true);
					// Only stop searching paths after a successful open. If exists() was true
					// but open() failed, keep trying other prefixes (e.g. tres/ vs current dir).
					if (!m_treeFiles[treeFileNameIndex])
					{
					continue;
					}
					// Check if this tree file is an encrypted TitanPak
					// Read just the magic token to check if encrypted (NUNA = encrypted TitanPak)
					{
						Tag treeToken;
						m_treeFiles[treeFileNameIndex]->read(0, &treeToken, sizeof(treeToken), AbstractFile::PriorityData);
						if (treeToken == TAG_NUNA)
						{
						// This is an encrypted TitanPak - read encryption header and initialize context
						// Encryption header follows immediately after the standard 36-byte tree header
						const int encryptionHeaderOffset = 36;
						TitanPakCrypto::EncryptionHeader encHeader;
						m_treeFiles[treeFileNameIndex]->read(encryptionHeaderOffset, &encHeader, sizeof(encHeader), AbstractFile::PriorityData);
						const char* password = TitanPakCrypto::getPassword();
						m_encryptionContexts[treeFileNameIndex] = new TitanPakEncryptionContext();
						m_encryptionContexts[treeFileNameIndex]->initialize(password, encHeader);
						DEBUG_REPORT_LOG(true, ("SearchTOC: Loaded encrypted TitanPak tree file: %s\n", treePathBuffer));
						}
					}
					break;
					}
					}

					{
					char const * const nptr = m_treeFileNames + treeFileNameReadPosition;
					size_t const nameLen = titanBoundedCStrLen(nptr, static_cast<size_t>(nameBytesRemaining));
					if (nameLen == static_cast<size_t>(nameBytesRemaining))
					{
						FATAL(true, ("SearchTOC: tree file name in TOC not null-terminated (index %d, pos %d, rem %d)", treeFileNameIndex, treeFileNameReadPosition, nameBytesRemaining));
					}
					char namePreview[256] = { 0 };
					::strncpy(namePreview, nptr, sizeof(namePreview) - 1U);
					namePreview[sizeof(namePreview) - 1U] = '\0';
					FATAL(!m_treeFiles[treeFileNameIndex], ("failed to open tree file index %d, offset %d, name '%s' (tried full tre paths)", treeFileNameIndex, treeFileNameReadPosition, namePreview));
					treeFileNameReadPosition += static_cast<int>(nameLen + 1U);
					}
					}

					delete [] treePathBuffer;
					titanSearchNodeOds("SearchTOC tree .tre(s) open done, read main TOC (zlib may follow)");
				}

#ifdef _WIN32
				OutputDebugStringA("[Titan] SearchNode: after tree inner block, before main TOC\r\n");
#endif
				titanOdsS32I64i("SearchTOC step2 main TOC readPos", readPosition, tocBytesOnDisk);

				// prepare table of contents by zeroing out the total size of data to be stored
				// TOC table byte size: use int64 multiply (early header check already bounded; int must hold result)
				int const tableOfContentsSize = static_cast<int>(static_cast<int64_t>(sizeof(TableOfContentsEntry)) * static_cast<int64_t>(m_numberOfFiles));
				{
					if (SearchTOC::isCompressed(static_cast<int>(header.tocCompressor)))
					{
						int64_t const needToc = static_cast<int64_t>(readPosition) + static_cast<int64_t>(header.sizeOfTOC);
						FATAL(needToc > static_cast<int64_t>(tocBytesOnDisk) || needToc < 0, ("SearchTOC: truncated before compressed TOC (pos=%d comp=%u len64=%I64d) %s", readPosition, header.sizeOfTOC, static_cast<__int64>(tocBytesOnDisk), m_TOCFileName));
					}
					else
					{
						int64_t const needToc = static_cast<int64_t>(readPosition) + static_cast<int64_t>(tableOfContentsSize);
						FATAL(needToc > static_cast<int64_t>(tocBytesOnDisk) || needToc < 0, ("SearchTOC: truncated before raw TOC (pos=%d need=%d len64=%I64d) %s", readPosition, tableOfContentsSize, static_cast<__int64>(tocBytesOnDisk), m_TOCFileName));
					}
				}
				titanSearchNodeOds("SearchTOC: read main TOC n=%u needRaw=%d pos=%d zlib=%d", m_numberOfFiles, tableOfContentsSize, readPosition, SearchTOC::isCompressed(static_cast<int>(header.tocCompressor)) ? 1 : 0);
				if (SearchTOC::isCompressed(static_cast<int>(header.tocCompressor)))
				{
					// create temp buffer to store the compressed TOC entry data
					byte *entryBuffer = new byte[header.sizeOfTOC];

					// read the compressed table of contents data into buffer
					const int tocOffset = readPosition;
					const int bytesRead = m_TOCFile->read(readPosition, entryBuffer, header.sizeOfTOC, AbstractFile::PriorityData);
					FATAL(bytesRead != static_cast<int>(header.sizeOfTOC), ("SearchTOC: failed to read compressed TOC (got %d need %u) %s", bytesRead, header.sizeOfTOC, m_TOCFileName));
					readPosition += bytesRead;

					// Decrypt if the titanlst is encrypted (before decompression)
					if (m_tocEncryptionContext && m_tocEncryptionContext->isInitialized())
					{
						m_tocEncryptionContext->decryptAt(entryBuffer, header.sizeOfTOC, tocOffset);
					}

					// decompress data into toc
					static_cast<void>(ZlibCompressor().expand(entryBuffer, header.sizeOfTOC, m_tableOfContents, tableOfContentsSize));

					delete [] entryBuffer;
				}
				else
				{
					// read the uncompressed table of contents data
					const int tocOffset = readPosition;
					const int bytesRead = m_TOCFile->read(readPosition, m_tableOfContents, tableOfContentsSize, AbstractFile::PriorityData);
					FATAL(bytesRead != tableOfContentsSize, ("SearchTOC: failed to read raw TOC (got %d need %d) %s", bytesRead, tableOfContentsSize, m_TOCFileName));
					readPosition += bytesRead;
					
					// Decrypt if the titanlst is encrypted
					if (m_tocEncryptionContext && m_tocEncryptionContext->isInitialized())
					{
						m_tocEncryptionContext->decryptAt(reinterpret_cast<uint8_t*>(m_tableOfContents), tableOfContentsSize, tocOffset);
					}
				}
				titanSearchNodeOds("SearchTOC: main TOC in memory; mapping name lengths to offsets");

				// After the TableOfContents is read into memory, the fileNameLengths must be changed to fileNameOffsets
				{
					int64_t run = 0;
					int64_t const nameCap = static_cast<int64_t>(header.uncompSizeOfNameBlock);
					for (uint32 i = 0; i < m_numberOfFiles; ++i)
					{
						uint32 const nameField = m_tableOfContents[i].fileNameOffset; // still length, not yet offset
						int64_t const add = static_cast<int64_t>(nameField) + 1;
						if (add < 1 || add > (nameCap - run))
						{
							FATAL(true, ("SearchTOC: bad per-file name length (idx=%u len=%u run=%I64d cap=%u) in %s", i, nameField, static_cast<__int64>(run), header.uncompSizeOfNameBlock, m_TOCFileName));
						}
						m_tableOfContents[i].fileNameOffset = static_cast<uint32_t>(run);
						run += add;
					}
					if (run > nameCap)
					{
						FATAL(true, ("SearchTOC: name lengths past end of name block (run=%I64d cap=%u) in %s", static_cast<__int64>(run), header.uncompSizeOfNameBlock, m_TOCFileName));
					}
				}

				{
					int64_t const needName = static_cast<int64_t>(readPosition) + static_cast<int64_t>(header.sizeOfNameBlock);
					FATAL(needName > static_cast<int64_t>(tocBytesOnDisk) || needName < 0, ("SearchTOC: truncated before name block (pos=%d onDisk=%u fileLen64=%I64d) %s", readPosition, header.sizeOfNameBlock, static_cast<__int64>(tocBytesOnDisk), m_TOCFileName));
				}
				titanSearchNodeOds("SearchTOC: name block read pos=%d onDisk=%u uncomp=%u compressed=%d", readPosition, header.sizeOfNameBlock, header.uncompSizeOfNameBlock, SearchTOC::isCompressed(static_cast<int>(header.fileNameBlockCompressor)) ? 1 : 0);
				if (SearchTOC::isCompressed(static_cast<int>(header.fileNameBlockCompressor)))
				{
					// create temp buffer to store the compressed name block data
					byte *nameBuffer  = new byte[header.sizeOfNameBlock];

					// read the compressed table of contents data into buffer
					const int nameBlockOffset = readPosition;
					const int bytesRead = m_TOCFile->read(readPosition, nameBuffer, header.sizeOfNameBlock, AbstractFile::PriorityData);
					FATAL(bytesRead != static_cast<int>(header.sizeOfNameBlock), ("SearchTOC: failed to read compressed name block (got %d need %u) %s", bytesRead, header.sizeOfNameBlock, m_TOCFileName));

					// Decrypt if the titanlst is encrypted (before decompression)
					if (m_tocEncryptionContext && m_tocEncryptionContext->isInitialized())
					{
						m_tocEncryptionContext->decryptAt(nameBuffer, header.sizeOfNameBlock, nameBlockOffset);
					}

					// decompress data into tocFileNames
					static_cast<void>(ZlibCompressor().expand(nameBuffer, header.sizeOfNameBlock, m_fileNames, header.uncompSizeOfNameBlock));

					delete [] nameBuffer;
				}
				else
				{
					// read the uncompressed name block data
					const int nameBlockOffset = readPosition;
					const int bytesRead = m_TOCFile->read(readPosition, m_fileNames, header.sizeOfNameBlock, AbstractFile::PriorityData);
					FATAL(bytesRead != static_cast<int>(header.sizeOfNameBlock), ("SearchTOC: failed to read raw name block (got %d need %u) %s", bytesRead, header.sizeOfNameBlock, m_TOCFileName));
					
					// Decrypt if the titanlst is encrypted
					if (m_tocEncryptionContext && m_tocEncryptionContext->isInitialized())
					{
						m_tocEncryptionContext->decryptAt(reinterpret_cast<uint8_t*>(m_fileNames), header.sizeOfNameBlock, nameBlockOffset);
					}
				}

			}
			break;

		default:
			{
				delete m_TOCFile;

#if PRODUCTION
				FATAL(true, ("Table of Contents File corruption detected.  Please do a \"Full Scan\" from the launchpad. (%08x %s)", version, m_TOCFileName));
#else
				FATAL(true, ("unsupported version %d in %s", version, m_TOCFileName));
#endif

			}
			break;
	}

}

// ----------------------------------------------------------------------

TreeFile::SearchTOC::~SearchTOC(void)
{
	delete [] m_TOCFileName;
	delete [] m_treeFileNames;
	delete [] m_tableOfContents;
	delete [] m_fileNames;
	delete [] m_treeFileNamePointers;

	// clear out FileStreamer::File pointers and encryption contexts
	for (uint32 i = 0; i < m_numberOfTreeFiles; i++)
	{
		delete m_treeFiles[i];
		delete m_encryptionContexts[i];
	}
	delete [] m_treeFiles;
	delete [] m_encryptionContexts;
	
	// Delete the TOC encryption context if present
	delete m_tocEncryptionContext;

	delete m_TOCFile;
}

// ----------------------------------------------------------------------

void TreeFile::SearchTOC::debugPrint(void)
{
	DEBUG_REPORT_PRINT(true, ("  %d=priority %s=tree\n", getPriority(), m_TOCFileName));
	DEBUG_OUTPUT_STATIC_VIEW("Foundation\\TOCfile", ("  %d=priority %s=tree\n", getPriority(), m_TOCFileName));
}

// ----------------------------------------------------------------------

bool TreeFile::SearchTOC::localExists(const char *fileName, int *index) const
{
	NOT_NULL(fileName);
	DEBUG_FATAL(strlen(fileName) + 1 > Os::MAX_PATH_LENGTH,("file name too long %d/%d", strlen(fileName) + 1, Os::MAX_PATH_LENGTH));

	const uint32 crc = Crc::calculate(fileName);

	// try a binary search through the tree file to find the file
	bool found       = false;
	int  left        = 0;
	int  right       = m_numberOfFiles - 1;
	int  mid         = 0;
	while (!found && (left <= right))
	{
		mid = (left + right) / 2;

		if (m_tableOfContents[mid].crc < crc)
			left = mid + 1;
		else
			if (m_tableOfContents[mid].crc > crc)
				right = mid - 1;
			else
			{
				const int res = _stricmp(m_fileNames + m_tableOfContents[mid].fileNameOffset, fileName);

				if (res < 0)
					left = mid + 1;
				else
					if (res > 0)
						right = mid - 1;
					else
						found = true;
			}
	}

	// return the found index if desired
	if (found)
	{
		if (m_tableOfContents[mid].length == 0)
		{
			return false;
		}
		else if (m_tableOfContents[mid].offset == 0)
		{
			// sanity check - if the file has a length, but the offset is zero, the pointer in the TOC is invalid
			return false;
		}

		if (index)
			*index = mid;
	}

	return found;
}

// ----------------------------------------------------------------------

bool TreeFile::SearchTOC::exists(const char *fileName, bool &deleted) const
{
	NOT_NULL(fileName);
	deleted = false;
	return localExists(fileName, NULL);
}

// ----------------------------------------------------------------------

int TreeFile::SearchTOC::getFileSize(const char *fileName, bool &deleted) const
{
	NOT_NULL(fileName);
	deleted = false;
	int tableOfContentsIndex = 0;
	if (!localExists(fileName, &tableOfContentsIndex))
		return -1;

	return m_tableOfContents[tableOfContentsIndex].length;
}

// ----------------------------------------------------------------------

void TreeFile::SearchTOC::getPathName(const char *fileName, char *pathName, int pathNameLength)  const
{
	NOT_NULL(fileName);
	NOT_NULL(pathName);

	// make a pseudo-path name for the tree file
	int tableOfContentsIndex = 0;
	if(!localExists(fileName, &tableOfContentsIndex))
		return;

	char* treeFileName = m_treeFileNamePointers[m_tableOfContents[tableOfContentsIndex].treeFileIndex];

#ifdef _DEBUG
	bool deleted;
	DEBUG_FATAL(!exists(fileName, deleted), ("fileName does not exist"));
#endif

	const int stringLength = istrlen(treeFileName) + 1 + istrlen(fileName) + 1 + 1;
	FATAL(stringLength > pathNameLength, ("SearchTOC::getPathName: name too long %d/%d", stringLength, pathNameLength));

	strcpy(pathName, treeFileName);
	strcat(pathName, "[");
	strcat(pathName, fileName);
	strcat(pathName, "]");
}

// ----------------------------------------------------------------------

AbstractFile *TreeFile::SearchTOC::open(const char *fileName, AbstractFile::PriorityType priority, bool &deleted)
{
	NOT_NULL(fileName);
	DEBUG_FATAL(strlen(fileName) + 1 > Os::MAX_PATH_LENGTH,("file name too long %d/%d", strlen(fileName) + 1, Os::MAX_PATH_LENGTH));
	deleted = false;

	int tableOfContentsIndex = -1;
	if (localExists(fileName, &tableOfContentsIndex))
	{
	const TableOfContentsEntry &entry = m_tableOfContents[tableOfContentsIndex];
	const uint16 treeFileIndex = entry.treeFileIndex;
	TitanPakEncryptionContext* encContext = m_encryptionContexts ? m_encryptionContexts[treeFileIndex] : NULL;

	DEBUG_REPORT_LOG(true, ("SearchTOC::open '%s' from tree[%d] '%s' (offset=%u, len=%d, comp=%d, encrypted=%s)\n",
		fileName, treeFileIndex, m_treeFileNamePointers[treeFileIndex], 
		entry.offset, entry.length, entry.compressor,
		(encContext && encContext->isInitialized()) ? "yes" : "no"));

	if (!isCompressed(entry.compressor))
	{
	// Uncompressed file
	if (encContext && encContext->isInitialized())
	{
		// For encrypted archives, we need to read and decrypt the data
		byte *buffer = new byte[entry.length];
		const uint32 bytesRead = m_treeFiles[treeFileIndex]->read(entry.offset, buffer, entry.length, priority);
		DEBUG_FATAL(bytesRead != entry.length, ("error reading data into buffer"));
		UNREF(bytesRead);
				
		// Decrypt the data
		encContext->decryptAt(buffer, entry.length, entry.offset);
				
		// Return a memory file with the decrypted data
		return new MemoryFile(buffer, entry.length);
	}
	else
	{
		// Standard unencrypted file - use direct file streaming
		return new FileStreamerFile(priority, *m_treeFiles[treeFileIndex], entry.offset, entry.length);
	}
	}

	// Compressed file
	byte * compressedBuffer = new byte[entry.compressedLength];

	const uint32 bytesRead = m_treeFiles[treeFileIndex]->read(entry.offset, compressedBuffer, entry.compressedLength, priority);
	DEBUG_FATAL(bytesRead != entry.compressedLength, ("error reading compressed data into buffer"));
	UNREF(bytesRead);

	// Decrypt if encrypted TitanPak
	if (encContext && encContext->isInitialized())
	{
	encContext->decryptAt(compressedBuffer, entry.compressedLength, entry.offset);
	}

	return new ZlibFile(entry.length, compressedBuffer, entry.compressedLength, true);
	}

	return NULL;
	}

// ----------------------------------------------------------------------

void TreeFile::SearchTOC::collectVirtualPathNamesWithPrefixAndSuffix(char const * prefix, char const * suffix, std::vector<std::string> & out, std::set<std::string> & seen) const
{
	for (uint32 i = 0; i < m_numberOfFiles; ++i)
	{
		TableOfContentsEntry const & entry = m_tableOfContents[i];
		if (entry.length == 0 || entry.offset == 0)
			continue;

		char const * const name = m_fileNames + entry.fileNameOffset;
		if (!virtualPathMatchesPrefixAndSuffix(name, prefix, suffix))
			continue;

		if (seen.insert(name).second)
			out.push_back(std::string(name));
	}
}

// ======================================================================

class TreeFile::SearchCache::CachedFile
{
public:

	explicit CachedFile(char const * fileName, AbstractFile * abstractFile);
	~CachedFile();

	CrcString const * getCrcString () const;
	bool              getCompressed() const;
	int               getUncompressedLength() const;
	int               getCompressedLength() const;
	AbstractFile *    createAbstractFile() const;

private:

	CachedFile();
	CachedFile(CachedFile const &);
	CachedFile & operator= (CachedFile const &);

private:

	PersistentCrcString const m_name;

	byte * m_buffer;
	int    m_length;
	bool   m_compressed;
	int    m_uncompressedLength;
};

// ----------------------------------------------------------------------

TreeFile::SearchCache::CachedFile::CachedFile(char const * fileName, AbstractFile * abstractFile) :
	m_name(fileName, true),
	m_buffer(0),
	m_length(0),
	m_compressed(false),
	m_uncompressedLength(abstractFile->length())
{
	if (abstractFile->isZlibCompressed())
	{
		abstractFile->getZlibCompressedDataAndClose(m_buffer, m_length);
		m_compressed = true;
	}
	else
	{
		m_buffer = abstractFile->readEntireFileAndClose();
		m_length = m_uncompressedLength;
	}
}

// ----------------------------------------------------------------------

TreeFile::SearchCache::CachedFile::~CachedFile()
{
	delete [] m_buffer;
	m_buffer = 0;
}

// ----------------------------------------------------------------------

CrcString const * TreeFile::SearchCache::CachedFile::getCrcString () const
{
	return &m_name;
}

// ----------------------------------------------------------------------

bool TreeFile::SearchCache::CachedFile::getCompressed() const
{
	return m_compressed;
}

// ----------------------------------------------------------------------

int TreeFile::SearchCache::CachedFile::getUncompressedLength() const
{
	return m_compressed ? m_uncompressedLength : m_length;
}

// ----------------------------------------------------------------------

int TreeFile::SearchCache::CachedFile::getCompressedLength() const
{
	return m_length;
}

// ----------------------------------------------------------------------

AbstractFile * TreeFile::SearchCache::CachedFile::createAbstractFile() const
{
	if (m_compressed)
	{
		return new ZlibFile(m_uncompressedLength, m_buffer, m_length, false);
	}
	else
	{
		byte * const uncompressedData = new byte[m_length];
		memcpy(uncompressedData, m_buffer, m_length);
		return new MemoryFile(uncompressedData, m_length);
	}
}

// ======================================================================

namespace TreeFileSearchCacheNamespace
{
	bool ms_logSearchCache;
}

using namespace TreeFileSearchCacheNamespace;

// ----------------------------------------------------------------------

TreeFile::SearchCache::SearchCache(int const priority) : 
	SearchNode(priority),
	m_cachedFileMap(new CachedFileMap)
{
#if PRODUCTION == 0
	DebugFlags::registerFlag(ms_logSearchCache, "SharedFile", "logSearchCache");
#endif
}

// ----------------------------------------------------------------------

TreeFile::SearchCache::~SearchCache()
{
	std::for_each (m_cachedFileMap->begin(), m_cachedFileMap->end(), PointerDeleterPairSecond());
	m_cachedFileMap->clear();
	delete m_cachedFileMap;

#if PRODUCTION == 0
	DebugFlags::unregisterFlag(ms_logSearchCache);
#endif
}

// ----------------------------------------------------------------------

int TreeFile::SearchCache::addCachedFile(char const * const fileName)
{
	bool deleted = false;
	int fileSize = 0;
	if (exists(fileName, deleted))
		WARNING(true, ("Skipping existing cached file %s", fileName));
	else
	{
		AbstractFile * const abstractFile = TreeFile::open(fileName, AbstractFile::PriorityData, false);
		CachedFile * const cachedFile = new CachedFile(fileName, abstractFile);
		fileSize = cachedFile->getCompressedLength();
		delete abstractFile;

		bool const result = (m_cachedFileMap->insert(CachedFileMap::value_type (cachedFile->getCrcString(), cachedFile))).second;
		if (result)
		{
#if PRODUCTION == 0
			if (ms_logSearchCache)
			{
				if (cachedFile->getCompressed())
					REPORT_LOG(true, ("Adding cached file %s, compressed, %i/%i bytes\n", fileName, cachedFile->getCompressedLength(), cachedFile->getUncompressedLength()));
				else
					REPORT_LOG(true, ("Adding cached file %s, uncompressed, %i bytes\n", fileName, cachedFile->getUncompressedLength()));
			}
#endif
		}
		else
		{
			DEBUG_FATAL(!result, ("TreeFile::SearchCache::addCachedFile: insert failed for %s (possibly a duplicate entry)", fileName));
			delete cachedFile;
		}
	}

	return fileSize;
}

// ----------------------------------------------------------------------

void TreeFile::SearchCache::debugPrint()
{
	DEBUG_REPORT_PRINT(true, ("  %d=priority SearchCache [%i]\n", getPriority(), m_cachedFileMap->size()));
	DEBUG_OUTPUT_STATIC_VIEW("Foundation\\Treefile", ("  %d=priority SearchCache [%i]\n", getPriority(), m_cachedFileMap->size()));
}

// ----------------------------------------------------------------------

bool TreeFile::SearchCache::exists(char const * const fileName, bool & deleted) const
{
	deleted = false;

	TemporaryCrcString const crcString(fileName, true);
	return m_cachedFileMap->find(&crcString) != m_cachedFileMap->end();
}

// ----------------------------------------------------------------------

int TreeFile::SearchCache::getFileSize(char const * const fileName, bool & deleted) const
{
	deleted = false;

	TemporaryCrcString const crcString(fileName, true);
	CachedFileMap::iterator iter = m_cachedFileMap->find(&crcString);
	if (iter != m_cachedFileMap->end())
		return iter->second->getUncompressedLength();

	return -1;
}

// ----------------------------------------------------------------------

void TreeFile::SearchCache::getPathName(char const * const fileName, char * const pathName, int const pathNameLength) const
{
	NOT_NULL(fileName);
	NOT_NULL(pathName);

#ifdef _DEBUG
	bool deleted = false;
	DEBUG_FATAL(!exists(fileName, deleted), ("fileName does not exist"));
#endif

	const int stringLength = 13 + istrlen(fileName) + 1 + 1;
	FATAL(stringLength > pathNameLength, ("SearchCache::getPathName: name too long %d/%d", stringLength, pathNameLength));

	// make a pseudo-path name for the tree file
	strcpy(pathName, "SearchCache[");
	strcat(pathName, fileName);
	strcat(pathName, "]");
}

// ----------------------------------------------------------------------

AbstractFile * TreeFile::SearchCache::open(char const * const fileName, AbstractFile::PriorityType const, bool & deleted)
{
	deleted = false;

	TemporaryCrcString const crcString(fileName, true);
	CachedFileMap::iterator iter = m_cachedFileMap->find(&crcString);
	if (iter != m_cachedFileMap->end())
		return iter->second->createAbstractFile();

	return 0;
}

// ======================================================================

