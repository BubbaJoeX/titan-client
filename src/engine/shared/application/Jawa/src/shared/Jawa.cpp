// ======================================================================
//
// Jawa.cpp
//
// Asset Customization Manager Build Tool (Jawa)
// Cross-platform implementation with Perl .pm path fix
//
// ======================================================================

#include "Jawa.h"
#include "JawaConfig.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
	#include <windows.h>
	#include <direct.h>
	#define popen _popen
	#define pclose _pclose
	#define PATH_SEPARATOR "\\"
#else
	#include <unistd.h>
	#include <sys/wait.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <dirent.h>
	#define PATH_SEPARATOR "/"
#endif

// ======================================================================

bool Jawa::ms_verbose = false;

// ======================================================================

JawaConfig::JawaConfig()
: miffPath(JAWA_DEFAULT_MIFF_PATH)
, collectScript(JAWA_DEFAULT_COLLECT_SCRIPT)
, buildScript(JAWA_DEFAULT_BUILD_SCRIPT)
, basePath(JAWA_DEFAULT_BASE_PATH)
, clientPath(JAWA_DEFAULT_CLIENT_PATH)
, forceAddFile(JAWA_DEFAULT_FORCE_ADD_FILE)
, perlInterpreter(JAWA_DEFAULT_PERL_INTERPRETER)
, perlLibPath(JAWA_DEFAULT_PERL_LIB_PATH)
, verbose(false)
{
}

// ======================================================================

int Jawa::run(int argc, char **argv)
{
	std::cout << "Jawa - Asset Customization Manager Build Tool" << std::endl;
	std::cout << "==============================================" << std::endl;
	std::cout << std::endl;

	JawaConfig config;
	
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
		if (!config.perlLibPath.empty())
			std::cout << "  Perl Lib Path: " << config.perlLibPath << std::endl;
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

std::string Jawa::buildPerlCommand(const JawaConfig &config, const std::string &script, const std::string &args)
{
std::string command;

// Set environment variable for datatables path
#ifdef _WIN32
if (!config.basePath.empty())
{
std::string datatablesPath = config.basePath + "sys.shared\\compiled\\game\\datatables\\";
command = "set SWG_DATATABLES_PATH=" + datatablesPath + " && ";
}
#else
if (!config.basePath.empty())
{
std::string datatablesPath = config.basePath + "sys.shared/compiled/game/datatables/";
command = "SWG_DATATABLES_PATH=" + datatablesPath + " ";
}
#endif

command += config.perlInterpreter;

if (!config.perlLibPath.empty())
{
command += " -I\"" + config.perlLibPath + "\"";
}

command += " \"" + script + "\" " + args;
return command;
}
		else if (arg == "--verbose")
		{
			config.verbose = true;
		}
		else if (arg == "--miff" && i + 1 < argc)
		{
			config.miffPath = argv[++i];
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
		else if (arg == "--perl-lib" && i + 1 < argc)
		{
			config.perlLibPath = argv[++i];
		}
		else if (arg[0] != '-')
		{
			if (!hasBasePath)
			{
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

	if (!hasBasePath)
	{
		std::cerr << "ERROR: Base path is required!" << std::endl;
		std::cout << std::endl;
		printUsage();
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------

void Jawa::printUsage()
{
	std::cout << "Usage: Jawa [OPTIONS] <base_path>" << std::endl;
	std::cout << std::endl;
	std::cout << "Required:" << std::endl;
	std::cout << "  <base_path>              Path to sys.shared/compiled/game/ directory" << std::endl;
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
	std::cout << "  --perl-lib <path>        Perl library path (PERL5LIB) for .pm modules" << std::endl;
	std::cout << std::endl;
	std::cout << "Examples:" << std::endl;
	std::cout << "  Jawa ../../sku.0/sys.shared/compiled/game/" << std::endl;
	std::cout << "  Jawa --verbose --perl-lib ./lib ../../sku.0/sys.shared/compiled/game/" << std::endl;
	std::cout << "  Jawa --client-path client/ --perl perl.exe ../../sku.0/sys.shared/compiled/game/" << std::endl;
}

// ----------------------------------------------------------------------

void Jawa::printVersion()
{
	std::cout << "Jawa version " << JAWA_VERSION_STRING << std::endl;
	std::cout << "Asset Customization Manager Build Tool" << std::endl;
	std::cout << "Cross-platform C++ implementation with Perl .pm support" << std::endl;
}

// ----------------------------------------------------------------------

void Jawa::printStep(int step, int total, const std::string &message)
{
	std::cout << "Jawa: " << message << " [Step " << step << "/" << total << "]" << std::endl;
}

// ----------------------------------------------------------------------

bool Jawa::fileExists(const std::string &path)
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

bool Jawa::deleteFile(const std::string &path)
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

bool Jawa::executeCommand(const std::string &command, std::string &output, std::string &error, const JawaConfig *config)
{
	if (ms_verbose || (config && config->verbose))
	{
		std::cout << "  Executing: " << command << std::endl;
	}

#ifdef _WIN32
	FILE *pipe = _popen(command.c_str(), "r");
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

bool Jawa::gatherFilesForCompile(const std::string &basePath, std::set<std::string> &outFiles)
{
	const char *directories[] = {
		"appearance",
		"shader",
		"texturerenderer"
	};

	const char *extensions[] = {
		".apt", ".cmp", ".lmg", ".lod", ".lsb", ".mgn", ".msh", ".sat", ".pob",
		".sht",
		".trt"
	};

#ifdef _WIN32
	for (const char *dir : directories)
	{
		std::string searchPath = basePath + PATH_SEPARATOR + dir;
		std::string pattern = searchPath + "\\*";

		WIN32_FIND_DATAA findData;
		HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);

		if (hFind == INVALID_HANDLE_VALUE)
			continue;

		do
		{
			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0)
				{
					std::string subPath = searchPath + "\\" + findData.cFileName;
					gatherFilesForCompile(subPath, outFiles);
				}
			}
			else
			{
				std::string fileName = findData.cFileName;
				for (const char *ext : extensions)
				{
					if (fileName.length() >= strlen(ext) &&
					    fileName.compare(fileName.length() - strlen(ext), strlen(ext), ext) == 0)
					{
						std::string fullPath = searchPath + "\\" + fileName;
						outFiles.insert(fullPath);
						break;
					}
				}
			}
		} while (FindNextFileA(hFind, &findData) != 0);

		FindClose(hFind);
	}
#else
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

bool Jawa::createLookupFile(const std::string &basePath, const std::string &lookupFile, const std::set<std::string> &files)
{
	std::ofstream outFile(lookupFile);
	if (!outFile.is_open())
	{
		std::cerr << "ERROR: Failed to create lookup file: " << lookupFile << std::endl;
		return false;
	}

	outFile << "p " << basePath << ":0" << std::endl;

	for (const std::string &file : files)
	{
		size_t pos = file.find("compiled");
		if (pos != std::string::npos)
		{
			pos = file.find("game", pos);
			if (pos != std::string::npos)
			{
				std::string relativePath = file.substr(pos + 5);
				std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
				outFile << "e " << relativePath << ":0" << std::endl;
			}
		}
	}

	outFile.close();
	return true;
}

// ----------------------------------------------------------------------

bool Jawa::collectAssetCustomizationInfo(const JawaConfig &config, const std::string &lookupFile, const std::string &outputFile)
{
	std::string command = buildPerlCommand(config, config.collectScript, "-t " + lookupFile);
	std::string output, error;

	if (!executeCommand(command, output, error, &config))
	{
		std::cerr << "ERROR: Failed to collect asset customization info" << std::endl;
		std::cerr << "Error: " << error << std::endl;
		std::cerr << "HINT: Use --perl-lib to specify path to Perl .pm modules" << std::endl;
		return false;
	}

	std::ofstream outFile(outputFile);
	if (!outFile.is_open())
	{
		std::cerr << "ERROR: Failed to create output file: " << outputFile << std::endl;
		return false;
	}

	outFile << output;

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

bool Jawa::optimizeAssetCustomizationData(const JawaConfig &config, const std::string &inputFile, const std::string &lookupFile)
{
	std::string command = buildPerlCommand(config, config.buildScript, "-i " + inputFile + " -r -t " + lookupFile);
	std::string output, error;

	if (!executeCommand(command, output, error, &config))
	{
		std::cerr << "ERROR: Failed to optimize asset customization data" << std::endl;
		std::cerr << "Error: " << error << std::endl;
		std::cerr << "HINT: Use --perl-lib to specify path to Perl .pm modules" << std::endl;
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------

bool Jawa::buildAssetCustomizationManagerData(const JawaConfig &config, const std::string &inputFile, const std::string &acmOutput, const std::string &cimOutput, const std::string &lookupFile)
{
	std::string command = buildPerlCommand(config, config.buildScript, 
		"-i " + inputFile + " -o " + acmOutput + " -m " + cimOutput + " -t " + lookupFile);
	std::string output, error;

	if (!executeCommand(command, output, error, &config))
	{
		std::cerr << "ERROR: Failed to build asset customization manager data" << std::endl;
		std::cerr << "Error: " << error << std::endl;
		std::cerr << "HINT: Use --perl-lib to specify path to Perl .pm modules" << std::endl;
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------

bool Jawa::compileMiff(const JawaConfig &config, const std::string &inputMiff, const std::string &outputIff)
{
	std::string command = config.miffPath + " -i \"" + inputMiff + "\" -o \"" + outputIff + "\"";
	std::string output, error;

	if (!executeCommand(command, output, error, &config))
	{
		std::cerr << "ERROR: Failed to compile MIFF: " << inputMiff << std::endl;
		std::cerr << "Error: " << error << std::endl;
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------

bool Jawa::buildAssetCustomizationManager(const JawaConfig &config)
{
	printStep(1, 5, "Starting build of Asset Customization Manager. This may take a few minutes...");

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

	const std::string collectResultFile = "acm_collect_result.dat";
	deleteFile(collectResultFile);

	if (!collectAssetCustomizationInfo(config, lookupFile, collectResultFile))
	{
		std::cerr << "ERROR: Failed to collect asset customization info" << std::endl;
		return false;
	}

	printStep(3, 5, "Finished Writing Asset Customization Info Collection. Running Optimization...");

	if (!optimizeAssetCustomizationData(config, collectResultFile, lookupFile))
	{
		std::cerr << "ERROR: Failed to optimize asset customization data" << std::endl;
		return false;
	}

	printStep(4, 5, "Finished Optimizing Lookup. Assembling Asset Customization Manager MIF...");

	const std::string optimizedFile = "acm_collect_result-optimized.dat";
	const std::string acmMiff = config.basePath + PATH_SEPARATOR + "customization" + PATH_SEPARATOR + "asset_customization_manager.mif";
	const std::string cimMiff = config.basePath + PATH_SEPARATOR + "customization" + PATH_SEPARATOR + "customization_id_manager.mif";

	if (!buildAssetCustomizationManagerData(config, optimizedFile, acmMiff, cimMiff, lookupFile))
	{
		std::cerr << "ERROR: Failed to build asset customization manager data" << std::endl;
		return false;
	}

	printStep(5, 5, "Finished Creating ACM and CIM Miff Files... Compiling MIFFs into IFFs for Client...");

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

	deleteFile(lookupFile);
	deleteFile(collectResultFile);
	deleteFile(optimizedFile);
	deleteFile("build_output.txt");
	deleteFile("error_output.txt");

	return true;
}

// ======================================================================
