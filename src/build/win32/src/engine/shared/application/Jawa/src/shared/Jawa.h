// ======================================================================
//
// Jawa.h
//
// Asset Customization Manager Build Tool (Jawa)
// Cross-platform implementation for Linux and Windows
//
// ======================================================================

#ifndef INCLUDED_Jawa_H
#define INCLUDED_Jawa_H

// ======================================================================

#include <string>
#include <vector>
#include <set>

// ======================================================================

struct JawaConfig
{
	std::string miffPath;
	std::string collectScript;
	std::string buildScript;
	std::string basePath;
	std::string clientPath;
	std::string forceAddFile;
	std::string perlInterpreter;
	std::string perlLibPath;
	bool verbose;

	JawaConfig();
};

// ======================================================================

class Jawa
{
public:
	static int run(int argc, char **argv);
	
private:
	static bool buildAssetCustomizationManager(const JawaConfig &config);
	static bool gatherFilesForCompile(const std::string &basePath, std::set<std::string> &outFiles);
	static bool createLookupFile(const std::string &basePath, const std::string &lookupFile, const std::set<std::string> &files);
	static bool collectAssetCustomizationInfo(const JawaConfig &config, const std::string &lookupFile, const std::string &outputFile);
	static bool optimizeAssetCustomizationData(const JawaConfig &config, const std::string &inputFile, const std::string &lookupFile);
	static bool buildAssetCustomizationManagerData(const JawaConfig &config, const std::string &inputFile, const std::string &acmOutput, const std::string &cimOutput, const std::string &lookupFile);
	static bool compileMiff(const JawaConfig &config, const std::string &inputMiff, const std::string &outputIff);
	static bool executeCommand(const std::string &command, std::string &output, std::string &error, const JawaConfig *config = nullptr);
	static bool fileExists(const std::string &path);
	static bool deleteFile(const std::string &path);
	static std::string normalizePath(const std::string &path);
	static std::string joinPath(const std::string &base, const std::string &sub);
	static bool parseCommandLine(int argc, char **argv, JawaConfig &config);
	static void printUsage();
	static void printVersion();
	static void printStep(int step, int total, const std::string &message);
	static std::string buildPerlCommand(const JawaConfig &config, const std::string &script, const std::string &args);

private:
	static bool ms_verbose;
};

// ======================================================================

#endif
