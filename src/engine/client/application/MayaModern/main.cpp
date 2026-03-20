/**
 * The function of this file is simply to register the plug-in with Maya
 * and enable the functionality of the underlying components.
 */

#include "translators/mgn.h"
#include "translators/msh.h"
#include "translators/skt.h"
#include "translators/ans.h"
#include "translators/flr.h"
#include "translators/sat.h"
#include "translators/pob.h"
#include "translators/dds.h"

#include "commands/SetDirectoryCommand.h"
#include "commands/SetBaseDirectory.h"
#include "commands/GetDataRootDirCommand.h"
#include "commands/ExportSkeleton.h"
#include "commands/ImportSkeleton.h"
#include "commands/ImportAnimation.h"
#include "commands/RevertToBindPose.h"
#include "commands/ImportSat.h"
#include "commands/ImportPob.h"
#include "commands/ExportPob.h"
#include "commands/ImportLodMesh.h"
#include "commands/ImportSkeletalMesh.h"
#include "commands/ImportShader.h"
#include "commands/ExportShader.h"
#include "commands/ImportStaticMesh.h"
#include "commands/ExportStaticMesh.h"
#include "commands/SwgAssetDissector.h"

#include "ConfigFile.h"
#include "Globals.h"

#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>

#include <utility>

/**
 * Structure to store all translator classes
 */
struct Translator
{
    Translator(std::string translatorName, std::string pixmapName, void *(*creatorFunction)(),
               const char *optionsScriptName, const char *defaultOptionsString, bool requiresFullMel) : translatorName(std::move(
            translatorName)), pixmapName(std::move(pixmapName)), creatorFunction(creatorFunction),
            optionsScriptName(optionsScriptName), defaultOptionsString(defaultOptionsString),
            requiresFullMel(requiresFullMel)
    {}
    
    std::string translatorName;
    std::string pixmapName;
    MCreatorFunction creatorFunction;
    const char* optionsScriptName;
    const char* defaultOptionsString;
    bool requiresFullMel;
};
// Pass nullptr for optionsScriptName - no MEL option procedures; avoids "Cannot find procedure" errors
static Translator* TRANSLATORS[] =
        {
            new Translator("mgn", "mgnTranslator.rgb", &MgnTranslator::creator, nullptr, "showPositions=1", true),
            new Translator("msh", "mshTranslator.rgb", &MshTranslator::creator, nullptr, "showPositions=1", true),
            new Translator("skt", "sktTranslator.rgb", &SktTranslator::creator, nullptr, "showPositions=1", true),
            new Translator("ans", "ansTranslator.rgb", &AnsTranslator::creator, nullptr, "showPositions=1", true),
            new Translator("flr", "flrTranslator.rgb", &FlrTranslator::creator, nullptr, "showPositions=1", true),
            // preserveReferences=0 avoids Maya 2026 "Invalid file type specified: -pr" error
            new Translator("sat", "satTranslator.rgb", &SatTranslator::creator, nullptr, "preserveReferences=0", true),
            new Translator("pob", "pobTranslator.rgb", &PobTranslator::creator, nullptr, "preserveReferences=0", true),
            new Translator("dds", "ddsTranslator.rgb", &DdsTranslator::creator, nullptr, "", true)
        };

MStatus initializePlugin(MObject obj)
{
    MFnPlugin plugin(obj, "SWGMayaEditor", "1.0", "Any");

    //---- First we're setting up Maya's log for API Errors
    MGlobal::startErrorLogging();
    MGlobal::setErrorLogPathName("SwgMayaEditorMayaLog");

    //---- Load our Configuration File
    ConfigFile::install();
    ConfigFile::loadFile("SwgMayaEditor.cfg");

    //---- Setup directory configuration (for setBaseDir, getDataRootDir, import path resolution)
    SetDirectoryCommand::install();

    //---- Setup Swg Logging
    const bool logDebug = ConfigFile::getKeyBool("SwgMayaEditor", "verboseLogging", false);
    loguru::add_file("SwgMayaEditor.log", loguru::Append, logDebug ? loguru::Verbosity_MAX : loguru::Verbosity_WARNING);

    MStatus status;
    for(const auto &item: TRANSLATORS)
    {
        status = plugin.registerFileTranslator(
                    item->translatorName.c_str(),
                    item->pixmapName.c_str(),
                    item->creatorFunction,
                    item->optionsScriptName,
                    item->defaultOptionsString,
                    item->requiresFullMel
                );
        if(!status)
        {
            std::cerr << "ERROR: Unable to Register Translator " << item->translatorName << std::endl;
            return status;
        }
    }

    //---- Register MEL commands
    status = plugin.registerCommand("setBaseDir", SetBaseDirectory::creator);
    if (!status) { std::cerr << "ERROR: Unable to register setBaseDir" << std::endl; return status; }

    status = plugin.registerCommand("getDataRootDir", GetDataRootDirCommand::creator);
    if (!status) { std::cerr << "ERROR: Unable to register getDataRootDir" << std::endl; return status; }

    status = plugin.registerCommand("exportSkeleton", ExportSkeleton::creator);
    if (!status) { std::cerr << "ERROR: Unable to register exportSkeleton" << std::endl; return status; }

    status = plugin.registerCommand("importSkeleton", ImportSkeleton::creator);
    if (!status) { std::cerr << "ERROR: Unable to register importSkeleton" << std::endl; return status; }

    status = plugin.registerCommand("importAnimation", ImportAnimation::creator);
    if (!status) { std::cerr << "ERROR: Unable to register importAnimation" << std::endl; return status; }

    status = plugin.registerCommand("swgRevertToBindPose", RevertToBindPose::creator);
    if (!status) { std::cerr << "ERROR: Unable to register swgRevertToBindPose" << std::endl; return status; }

    status = plugin.registerCommand("importSat", ImportSat::creator);
    if (!status) { std::cerr << "ERROR: Unable to register importSat" << std::endl; return status; }

    status = plugin.registerCommand("importPob", ImportPob::creator);
    if (!status) { std::cerr << "ERROR: Unable to register importPob" << std::endl; return status; }
    status = plugin.registerCommand("exportPob", ExportPob::creator);
    if (!status) { std::cerr << "ERROR: Unable to register exportPob" << std::endl; return status; }

    status = plugin.registerCommand("importLodMesh", ImportLodMesh::creator);
    if (!status) { std::cerr << "ERROR: Unable to register importLodMesh" << std::endl; return status; }

    status = plugin.registerCommand("importSkeletalMesh", ImportSkeletalMesh::creator);
    if (!status) { std::cerr << "ERROR: Unable to register importSkeletalMesh" << std::endl; return status; }

    status = plugin.registerCommand("importShader", ImportShader::creator);
    if (!status) { std::cerr << "ERROR: Unable to register importShader" << std::endl; return status; }

    status = plugin.registerCommand("importStaticMesh", ImportStaticMesh::creator);
    if (!status) { std::cerr << "ERROR: Unable to register importStaticMesh" << std::endl; return status; }

    status = plugin.registerCommand("exportStaticMesh", ExportStaticMesh::creator);
    if (!status) { std::cerr << "ERROR: Unable to register exportStaticMesh" << std::endl; return status; }

    status = plugin.registerCommand("swgAssetDissector", SwgAssetDissector::creator);
    if (!status) { std::cerr << "ERROR: Unable to register swgAssetDissector" << std::endl; return status; }

    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject obj)
{
    MFnPlugin plugin(obj);
    MStatus status;
    for(const auto &item: TRANSLATORS)
    {
        status = plugin.deregisterFileTranslator(
                item->translatorName.c_str()
        );
        if(!status)
        {
            std::cerr << "ERROR: Unable to Deregister Translator " << item->translatorName << std::endl;
            return status;
        }
    }

    plugin.deregisterCommand("swgAssetDissector");
    plugin.deregisterCommand("exportStaticMesh");
    plugin.deregisterCommand("importStaticMesh");
    plugin.deregisterCommand("exportShader");
    plugin.deregisterCommand("importShader");
    plugin.deregisterCommand("importSkeletalMesh");
    plugin.deregisterCommand("importLodMesh");
    plugin.deregisterCommand("exportPob");
    plugin.deregisterCommand("importPob");
    plugin.deregisterCommand("importSat");
    plugin.deregisterCommand("importAnimation");
    plugin.deregisterCommand("swgRevertToBindPose");
    plugin.deregisterCommand("exportSkeleton");
    plugin.deregisterCommand("importSkeleton");
    plugin.deregisterCommand("getDataRootDir");
    plugin.deregisterCommand("setBaseDir");

    SetDirectoryCommand::remove();

    return MS::kSuccess;
}