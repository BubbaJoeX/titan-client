// ======================================================================
//
// Kaadu.h
//
// Asset Customization Manager Build Tool
// Cross-platform implementation for Linux and Windows
//
// ======================================================================

#ifndef INCLUDED_Kaadu_H
#define INCLUDED_Kaadu_H

// ======================================================================

#include <string>
#include <vector>
#include <set>

// ======================================================================

struct AcmBuildConfig
{
	std::string miffPath;
	std::string collectScript;
	std::string buildScript;
	std::string basePath;
	std::string clientPath;
	std::string forceAddFile;
	std::string perlInterpreter;
	bool verbose;

	AcmBuildConfig();
};

// ======================================================================

class Kaadu
{
public:
	static int run(int argc, char **argv);
	
private:
	static bool buildAssetCustomizationManager(const AcmBuildConfig &config);
	static bool gatherFilesForCompile(const std::string &basePath, std::set<std::string> &outFiles);
	static bool createLookupFile(const std::string &basePath, const std::string &lookupFile, const std::set<std::string> &files);
	static bool collectAssetCustomizationInfo(const AcmBuildConfig &config, const std::string &lookupFile, const std::string &outputFile);
	static bool optimizeAssetCustomizationData(const AcmBuildConfig &config, const std::string &inputFile, const std::string &lookupFile);
	static bool buildAssetCustomizationManagerData(const AcmBuildConfig &config, const std::string &inputFile, const std::string &acmOutput, const std::string &cimOutput, const std::string &lookupFile);
	static bool compileMiff(const AcmBuildConfig &config, const std::string &inputMiff, const std::string &outputIff);
	static bool executeCommand(const std::string &command, std::string &output, std::string &error);
	static bool fileExists(const std::string &path);
	static bool deleteFile(const std::string &path);
	static bool parseCommandLine(int argc, char **argv, AcmBuildConfig &config);
	static void printUsage();
	static void printVersion();
	static void printStep(int step, int total, const std::string &message);

private:
	static bool ms_verbose;
};

// ======================================================================

#endif
