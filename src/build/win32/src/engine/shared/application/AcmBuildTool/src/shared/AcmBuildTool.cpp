// ======================================================================
//
// AcmBuildTool.cpp
//
// Asset Customization Manager Build Tool
// Cross-platform implementation for Linux and Windows
// With configurable paths support
//
// ======================================================================

#include "AcmBuildTool.h"
#include "AcmBuildConfig.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <cstring>
#include <cstdlib>
#include <cerrno>

#ifdef _WIN32
	#include <windows.h>
	#include <direct.h>
	#define popen _popen
	#define pclose _pclose
	#define PATH_SEPARATOR "\\"
#else
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/wait.h>
	#include <sys/stat.h>
	#include <dirent.h>
	#define PATH_SEPARATOR "/"
#endif

// ======================================================================

bool AcmBuildTool::ms_verbose = false;

// ======================================================================

AcmBuildConfig::AcmBuildConfig()
: miffPath(ACM_DEFAULT_MIFF_PATH)
, collectScript(ACM_DEFAULT_COLLECT_SCRIPT)
, buildScript(ACM_DEFAULT_BUILD_SCRIPT)
, basePath(ACM_DEFAULT_BASE_PATH)
, clientPath(ACM_DEFAULT_CLIENT_PATH)
, forceAddFile(ACM_DEFAULT_FORCE_ADD_FILE)
, perlInterpreter(ACM_DEFAULT_PERL_INTERPRETER)
, verbose(false)
{
}

// ======================================================================

int AcmBuildTool::run(int argc, char **argv)
{
	std::cout << "Asset Customization Manager Build Tool" << std::endl;
	std::cout << "=======================================" << std::endl;
	std::cout << std::endl;

	AcmBuildConfig config;
	
	if (!parseCommandLine(argc, argv, config))
	{
		return 1;
	}

	ms_verbose = config.verbose;

	if (ms_verbose)
	{
		std::cout << "Configuration:" << std::endl;
		std::cout << "  Miff Path: " << config.miffPath << std::endl;
		std::cout << "  Collect Script: " << config.collectScript << std::endl;
		std::cout << "  Build Script: " << config.buildScript << std::endl;
		std::cout << "  Base Path: " << config.basePath << std::endl;
		std::cout << "  Client Path: " << config.clientPath << std::endl;
		std::cout << "  Perl: " << config.perlInterpreter << std::endl;
		std::cout << std::endl;
	}

	if (!buildAssetCustomizationManager(config))
	{
		std::cerr << "ERROR: Asset Customization Manager build failed!" << std::endl;
		return 1;
	}

	return 0;
}

// ----------------------------------------------------------------------

bool AcmBuildTool::parseCommandLine(int argc, char **argv, AcmBuildConfig &config)
{
	bool hasBasePath = false;
	bool hasRootPath = false;

	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];

		if (arg == "-h" || arg == "--help")
		{
			printUsage();
			return false;
		}
		else if (arg == "-v" || arg == "--version")
		{
			printVersion();
			return false;
		}
		else if (arg == "--verbose")
		{
			config.verbose = true;
		}
		else if (arg == "--miff" && i + 1 < argc)
		{
			config.miffPath = argv[++i];
		}
		else if (arg == "--root" && i + 1 < argc)
		{
			if (hasBasePath)
			{
				std::cerr << "ERROR: --root cannot be combined with explicit base path." << std::endl;
				return false;
			}
			if (!configureFromRootPath(argv[++i], config))
				return false;
			hasRootPath = true;
		}
		else if (arg == "--collect-script" && i + 1 < argc)
		{
			config.collectScript = argv[++i];
		}
		else if (arg == "--build-script" && i + 1 < argc)
		{
			config.buildScript = argv[++i];
		}
		else if (arg == "--client-path" && i + 1 < argc)
		{
			config.clientPath = argv[++i];
		}
		else if (arg == "--perl" && i + 1 < argc)
		{
			config.perlInterpreter = argv[++i];
		}
		else if (arg[0] != '-')
		{
			// First non-option argument is the base path
			if (!hasBasePath)
			{
				if (hasRootPath)
				{
					std::cerr << "ERROR: Explicit base path cannot be combined with --root." << std::endl;
					return false;
				}
				config.basePath = arg;
				hasBasePath = true;
			}
		}
		else
		{
			std::cerr << "ERROR: Unknown option: " << arg << std::endl;
			printUsage();
			return false;
		}
	}

	// Validate that we have a base path
	if (!hasBasePath && !hasRootPath)
	{
		std::cerr << "ERROR: Base path or --root is required!" << std::endl;
		std::cout << std::endl;
		printUsage();
		return false;
	}

	config.basePath = normalizePath(config.basePath);
	config.clientPath = normalizePath(config.clientPath);
	config.collectScript = normalizePath(config.collectScript);
	config.buildScript = normalizePath(config.buildScript);
	config.miffPath = normalizePath(config.miffPath);

	if (!directoryExists(config.basePath))
	{
		std::cerr << "ERROR: Base path does not exist: " << config.basePath << std::endl;
		return false;
	}
	if (!fileExists(config.collectScript))
	{
		std::cerr << "ERROR: collect script not found: " << config.collectScript << std::endl;
		return false;
	}
	if (!fileExists(config.buildScript))
	{
		std::cerr << "ERROR: build script not found: " << config.buildScript << std::endl;
		return false;
	}
	if (!fileExists(config.miffPath))
	{
		std::cerr << "ERROR: Miff compiler not found: " << config.miffPath << std::endl;
		return false;
	}

	if (!ensureDirectory(config.clientPath))
	{
		std::cerr << "ERROR: Failed to create/access client path: " << config.clientPath << std::endl;
		return false;
	}

	const std::string clientCustomizationPath = joinPath(config.clientPath, "customization");
	if (!ensureDirectory(clientCustomizationPath))
	{
		std::cerr << "ERROR: Failed to create/access customization output path: " << clientCustomizationPath << std::endl;
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------

void AcmBuildTool::printUsage()
{
	std::cout << "Usage: AcmBuildTool [OPTIONS] <base_path>" << std::endl;
	std::cout << "   or: AcmBuildTool [OPTIONS] --root <repo_root>" << std::endl;
	std::cout << std::endl;
	std::cout << "Required:" << std::endl;
	std::cout << "  <base_path>              Path to dsrc/sku.0/sys.shared/compiled/game/" << std::endl;
	std::cout << "  --root <repo_root>       Repository root (e.g. D:/titan)" << std::endl;
	std::cout << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  -h, --help               Show this help message" << std::endl;
	std::cout << "  -v, --version            Show version information" << std::endl;
	std::cout << "  --verbose                Enable verbose output" << std::endl;
	std::cout << "  --miff <path>            Path to Miff compiler" << std::endl;
	std::cout << "  --collect-script <path>  Path to collectAssetCustomizationInfo.pl" << std::endl;
	std::cout << "  --build-script <path>    Path to buildAssetCustomizationManagerData.pl" << std::endl;
	std::cout << "  --client-path <path>     Output path for client files" << std::endl;
	std::cout << "  --perl <cmd>             Perl interpreter command" << std::endl;
	std::cout << std::endl;
	std::cout << "Examples:" << std::endl;
	std::cout << "  AcmBuildTool --root D:/titan --verbose" << std::endl;
	std::cout << "  AcmBuildTool --root D:/titan --miff D:/titan/exe/win32/Miff.exe" << std::endl;
	std::cout << "  AcmBuildTool D:/titan/dsrc/sku.0/sys.shared/compiled/game --client-path D:/titan/data/sku.0/sys.client/compiled/game" << std::endl;
}

// ----------------------------------------------------------------------

void AcmBuildTool::printVersion()
{
	std::cout << "AcmBuildTool version " << ACM_VERSION_STRING << std::endl;
	std::cout << "Asset Customization Manager Build Tool" << std::endl;
	std::cout << "Cross-platform C++ implementation" << std::endl;
}

// ----------------------------------------------------------------------

void AcmBuildTool::printStep(int step, int total, const std::string &message)
{
	std::cout << "ACM: " << message << " [Step " << step << "/" << total << "]" << std::endl;
}

// ----------------------------------------------------------------------

bool AcmBuildTool::fileExists(const std::string &path)
{
#ifdef _WIN32
	DWORD attrib = GetFileAttributesA(path.c_str());
	return (attrib != INVALID_FILE_ATTRIBUTES);
#else
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
#endif
}

// ----------------------------------------------------------------------

bool AcmBuildTool::directoryExists(const std::string &path)
{
#ifdef _WIN32
	DWORD attrib = GetFileAttributesA(path.c_str());
	return (attrib != INVALID_FILE_ATTRIBUTES) && ((attrib & FILE_ATTRIBUTE_DIRECTORY) != 0);
#else
	struct stat info;
	return (stat(path.c_str(), &info) == 0) && S_ISDIR(info.st_mode);
#endif
}

// ----------------------------------------------------------------------

bool AcmBuildTool::ensureDirectory(const std::string &path)
{
	if (path.empty())
		return false;

	if (directoryExists(path))
		return true;

	std::string normalized = normalizePath(path);
	std::string current;

#ifdef _WIN32
	const bool hasDrivePrefix = (normalized.size() > 1 && normalized[1] == ':');
	if (hasDrivePrefix)
	{
		current = normalized.substr(0, 2);
	}
#endif

	std::stringstream parts(normalized);
	std::string token;
	while (std::getline(parts, token, '/'))
	{
		if (token.empty() || token == ".")
			continue;
#ifdef _WIN32
		if (hasDrivePrefix && current.size() == 2 && token == current)
			continue;
#endif

		if (!current.empty() && current[current.size() - 1] != '/')
			current += '/';
		current += token;

		if (directoryExists(current))
			continue;

#ifdef _WIN32
		if (_mkdir(current.c_str()) != 0 && errno != EEXIST)
			return false;
#else
		if (mkdir(current.c_str(), 0755) != 0 && errno != EEXIST)
			return false;
#endif
	}

	return directoryExists(normalized);
}

// ----------------------------------------------------------------------

std::string AcmBuildTool::normalizePath(const std::string &path)
{
	std::string normalized = path;
	std::replace(normalized.begin(), normalized.end(), '\\', '/');
	while (normalized.find("//") != std::string::npos)
		normalized.replace(normalized.find("//"), 2, "/");
	trimTrailingSeparators(normalized);
	return normalized;
}

// ----------------------------------------------------------------------

std::string AcmBuildTool::joinPath(const std::string &left, const std::string &right)
{
	if (left.empty())
		return normalizePath(right);
	if (right.empty())
		return normalizePath(left);

	std::string merged = normalizePath(left);
	if (merged[merged.size() - 1] != '/')
		merged += '/';
	merged += right;
	return normalizePath(merged);
}

// ----------------------------------------------------------------------

void AcmBuildTool::trimTrailingSeparators(std::string &path)
{
	while (path.size() > 1 && (path[path.size() - 1] == '/' || path[path.size() - 1] == '\\'))
	{
#ifdef _WIN32
		if (path.size() == 3 && path[1] == ':')
			break;
#endif
		path.erase(path.size() - 1);
	}
}

// ----------------------------------------------------------------------

bool AcmBuildTool::configureFromRootPath(const std::string &rootPath, AcmBuildConfig &config)
{
	std::string root = normalizePath(rootPath);
	if (!directoryExists(root))
	{
		std::cerr << "ERROR: Root path does not exist: " << root << std::endl;
		return false;
	}

	config.basePath = joinPath(root, "dsrc/sku.0/sys.shared/compiled/game");
	config.clientPath = joinPath(root, "data/sku.0/sys.client/compiled/game");
	config.collectScript = joinPath(root, "client/tools/collectAssetCustomizationInfo.pl");
	config.buildScript = joinPath(root, "client/tools/buildAssetCustomizationManagerData.pl");
#ifdef _WIN32
	config.miffPath = joinPath(root, "exe/win32/Miff.exe");
#else
	config.miffPath = joinPath(root, "exe/linux/Miff");
#endif

	return true;
}

// ----------------------------------------------------------------------

bool AcmBuildTool::deleteFile(const std::string &path)
{
	if (fileExists(path))
	{
#ifdef _WIN32
		return DeleteFileA(path.c_str()) != 0;
#else
		return unlink(path.c_str()) == 0;
#endif
	}
	return true;
}

// ----------------------------------------------------------------------

bool AcmBuildTool::executeCommand(const std::string &command, std::string &output, std::string &error)
{
	if (ms_verbose)
	{
		std::cout << "  Executing: " << command << std::endl;
	}

#ifdef _WIN32
	// Windows implementation using cmd /C to preserve quoting and arguments.
	const std::string wrappedCommand = "cmd /C \"" + command + "\"";
	FILE *pipe = _popen(wrappedCommand.c_str(), "r");
	if (!pipe)
	{
		error = "Failed to execute command";
		return false;
	}

	char buffer[256];
	while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
	{
		output += buffer;
	}

	int result = _pclose(pipe);
	return result == 0;
#else
	// Linux implementation using popen
	std::string fullCommand = command + " 2>&1";
	FILE *pipe = popen(fullCommand.c_str(), "r");
	if (!pipe)
	{
		error = "Failed to execute command";
		return false;
	}

	char buffer[256];
	while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
	{
		output += buffer;
	}

	int status = pclose(pipe);
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

// ----------------------------------------------------------------------

bool AcmBuildTool::gatherFilesForCompile(const std::string &basePath, std::set<std::string> &outFiles)
{
	// Define the directories we need to search
	const char *directories[] = {
		"appearance",
		"shader",
		"texturerenderer"
	};

	// Define the file extensions we're looking for
	const char *extensions[] = {
		".apt", ".cmp", ".lmg", ".lod", ".lsb", ".mgn", ".msh", ".sat", ".pob",  // appearance
		".sht",  // shader
		".trt"   // texturerenderer
	};

#ifdef _WIN32
	// Windows recursive scan implementation.
	std::function<void(const std::string&)> scanDirectory = [&](const std::string &path)
	{
		std::string pattern = path + "\\*";
		WIN32_FIND_DATAA findData;
		HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);
		if (hFind == INVALID_HANDLE_VALUE)
			return;

		do
		{
			if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
				continue;

			const std::string fullPath = path + "\\" + findData.cFileName;
			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				scanDirectory(fullPath);
			}
			else
			{
				const std::string fileName = findData.cFileName;
				for (const char *ext : extensions)
				{
					if (fileName.length() >= strlen(ext) &&
					    fileName.compare(fileName.length() - strlen(ext), strlen(ext), ext) == 0)
					{
						outFiles.insert(fullPath);
						break;
					}
				}
			}
		}
		while (FindNextFileA(hFind, &findData) != 0);

		FindClose(hFind);
	};

	for (const char *dir : directories)
	{
		const std::string searchPath = basePath + PATH_SEPARATOR + dir;
		scanDirectory(searchPath);
	}
#else
	// Linux implementation
	std::function<void(const std::string&)> scanDirectory = [&](const std::string &path)
	{
		DIR *dir = opendir(path.c_str());
		if (!dir)
			return;

		struct dirent *entry;
		while ((entry = readdir(dir)) != nullptr)
		{
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
				continue;

			std::string fullPath = path + "/" + entry->d_name;
			struct stat statbuf;
			if (stat(fullPath.c_str(), &statbuf) == 0)
			{
				if (S_ISDIR(statbuf.st_mode))
				{
					scanDirectory(fullPath);
				}
				else if (S_ISREG(statbuf.st_mode))
				{
					std::string fileName = entry->d_name;
					for (const char *ext : extensions)
					{
						if (fileName.length() >= strlen(ext) &&
						    fileName.compare(fileName.length() - strlen(ext), strlen(ext), ext) == 0)
						{
							outFiles.insert(fullPath);
							break;
						}
					}
				}
			}
		}
		closedir(dir);
	};

	for (const char *dir : directories)
	{
		std::string searchPath = basePath + "/" + dir;
		scanDirectory(searchPath);
	}
#endif

	if (ms_verbose)
	{
		std::cout << "  Gathered " << outFiles.size() << " files" << std::endl;
	}

	return true;
}

// ----------------------------------------------------------------------

bool AcmBuildTool::createLookupFile(const std::string &basePath, const std::string &lookupFile, const std::set<std::string> &files)
{
	std::ofstream outFile(lookupFile);
	if (!outFile.is_open())
	{
		std::cerr << "ERROR: Failed to create lookup file: " << lookupFile << std::endl;
		return false;
	}

	// Write the base path; collector expects trailing slash for base + relative concatenation.
	std::string basePathWithSlash = basePath;
	if (!basePathWithSlash.empty() && basePathWithSlash[basePathWithSlash.size() - 1] != '/' && basePathWithSlash[basePathWithSlash.size() - 1] != '\\')
		basePathWithSlash += '/';
	outFile << "p " << basePathWithSlash << ":0" << std::endl;

	// Write each file entry
	for (const std::string &file : files)
	{
		// Extract the relative path from basePath
		size_t pos = file.find("compiled");
		if (pos != std::string::npos)
		{
			pos = file.find("game", pos);
			if (pos != std::string::npos)
			{
				std::string relativePath = file.substr(pos + 5); // +5 to skip "game/"
				
				// Replace backslashes with forward slashes for consistency
				std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
				
				outFile << "e " << relativePath << ":0" << std::endl;
			}
		}
	}

	outFile.close();
	return true;
}

// ----------------------------------------------------------------------

bool AcmBuildTool::collectAssetCustomizationInfo(const AcmBuildConfig &config, const std::string &lookupFile, const std::string &outputFile)
{
	std::string command = "\"" + config.perlInterpreter + "\" \"" + config.collectScript + "\" -t \"" + lookupFile + "\"";
	std::string output, error;

	if (!executeCommand(command, output, error))
	{
		std::cerr << "ERROR: Failed to collect asset customization info" << std::endl;
		std::cerr << "Error: " << error << std::endl;
		return false;
	}

	// Write the output to the result file
	std::ofstream outFile(outputFile);
	if (!outFile.is_open())
	{
		std::cerr << "ERROR: Failed to create output file: " << outputFile << std::endl;
		return false;
	}

	outFile << output;

	// Append force_add_variable_usage.dat if it exists
	std::string forceAddPath = config.basePath + PATH_SEPARATOR + config.forceAddFile;
	if (fileExists(forceAddPath))
	{
		std::ifstream forceAddInput(forceAddPath);
		if (forceAddInput.is_open())
		{
			std::string line;
			while (std::getline(forceAddInput, line))
			{
				if (!line.empty() && line[0] != '#')
				{
					outFile << line << std::endl;
				}
			}
			forceAddInput.close();
		}
	}

	outFile.close();
	return true;
}

// ----------------------------------------------------------------------

bool AcmBuildTool::optimizeAssetCustomizationData(const AcmBuildConfig &config, const std::string &inputFile, const std::string &lookupFile)
{
	std::string command = "\"" + config.perlInterpreter + "\" \"" + config.buildScript + "\" -i \"" + inputFile + "\" -r -t \"" + lookupFile + "\"";
	std::string output, error;

	if (!executeCommand(command, output, error))
	{
		std::cerr << "ERROR: Failed to optimize asset customization data" << std::endl;
		std::cerr << "Error: " << error << std::endl;
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------

bool AcmBuildTool::buildAssetCustomizationManagerData(const AcmBuildConfig &config, const std::string &inputFile, const std::string &acmOutput, const std::string &cimOutput, const std::string &lookupFile)
{
	std::string command = "\"" + config.perlInterpreter + "\" \"" + config.buildScript + "\" -i \"" + inputFile + "\"" +
	                      " -o \"" + acmOutput + "\"" +
	                      " -m \"" + cimOutput + "\"" +
	                      " -t \"" + lookupFile + "\"";
	std::string output, error;

	if (!executeCommand(command, output, error))
	{
		std::cerr << "ERROR: Failed to build asset customization manager data" << std::endl;
		std::cerr << "Error: " << error << std::endl;
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------

bool AcmBuildTool::compileMiff(const AcmBuildConfig &config, const std::string &inputMiff, const std::string &outputIff)
{
	std::string command = config.miffPath + " -i \"" + inputMiff + "\" -o \"" + outputIff + "\"";
	std::string output, error;

	if (!executeCommand(command, output, error))
	{
		std::cerr << "ERROR: Failed to compile MIFF: " << inputMiff << std::endl;
		std::cerr << "Error: " << error << std::endl;
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------

bool AcmBuildTool::buildAssetCustomizationManager(const AcmBuildConfig &config)
{
	printStep(1, 5, "Starting build of Asset Customization Manager. This may take a few minutes...");

	// Step 1: Gather files and create lookup table
	std::set<std::string> files;
	if (!gatherFilesForCompile(config.basePath, files))
	{
		std::cerr << "ERROR: Failed to gather files for compilation" << std::endl;
		return false;
	}

	const std::string lookupFile = "acm_lookup.dat";
	deleteFile(lookupFile);

	if (!createLookupFile(config.basePath, lookupFile, files))
	{
		std::cerr << "ERROR: Failed to create lookup file" << std::endl;
		return false;
	}

	printStep(2, 5, "Finished Writing ACM Lookup File. Starting Asset Customization Info Collection...");

	// Step 2: Collect asset customization info
	const std::string collectResultFile = "acm_collect_result.dat";
	
	deleteFile(collectResultFile);

	if (!collectAssetCustomizationInfo(config, lookupFile, collectResultFile))
	{
		std::cerr << "ERROR: Failed to collect asset customization info" << std::endl;
		return false;
	}

	printStep(3, 5, "Finished Writing Asset Customization Info Collection. Running Optimization...");

	// Step 3: Optimize asset customization data
	if (!optimizeAssetCustomizationData(config, collectResultFile, lookupFile))
	{
		std::cerr << "ERROR: Failed to optimize asset customization data" << std::endl;
		return false;
	}

	printStep(4, 5, "Finished Optimizing Lookup. Assembling Asset Customization Manager MIF...");

	// Step 4: Build asset customization manager
	const std::string optimizedFile = "acm_collect_result-optimized.dat";
	const std::string acmMiff = config.basePath + PATH_SEPARATOR + "customization" + PATH_SEPARATOR + "asset_customization_manager.mif";
	const std::string cimMiff = config.basePath + PATH_SEPARATOR + "customization" + PATH_SEPARATOR + "customization_id_manager.mif";

	if (!buildAssetCustomizationManagerData(config, optimizedFile, acmMiff, cimMiff, lookupFile))
	{
		std::cerr << "ERROR: Failed to build asset customization manager data" << std::endl;
		return false;
	}

	printStep(5, 5, "Finished Creating ACM and CIM Miff Files... Compiling MIFFs into IFFs for Client...");

	// Step 5: Compile MIFFs to IFFs
	const std::string acmIff = config.clientPath + PATH_SEPARATOR + "customization" + PATH_SEPARATOR + "asset_customization_manager.iff";
	const std::string cimIff = config.clientPath + PATH_SEPARATOR + "customization" + PATH_SEPARATOR + "customization_id_manager.iff";

	if (!compileMiff(config, acmMiff, acmIff))
	{
		std::cerr << "ERROR: Failed to compile ACM MIFF" << std::endl;
		return false;
	}

	if (!compileMiff(config, cimMiff, cimIff))
	{
		std::cerr << "ERROR: Failed to compile CIM MIFF" << std::endl;
		return false;
	}

	std::cout << "***SUCCESS*** Finished compiling Asset Customization Manager and Customization ID Manager!" << std::endl;

	// Cleanup temporary files
	deleteFile(lookupFile);
	deleteFile(collectResultFile);
	deleteFile(optimizedFile);
	deleteFile("build_output.txt");
	deleteFile("error_output.txt");

	return true;
}

// ======================================================================
