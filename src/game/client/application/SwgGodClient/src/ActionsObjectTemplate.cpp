// ======================================================================
//
// ActionsObjectTemplate.cpp
// copyright(c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "ActionsObjectTemplate.h"
#include "ActionsObjectTemplate.moc"

#include "clientGame/ClientWorld.h"
#include "clientGame/Game.h"
#include "clientGame/GroundScene.h"
#include "clientGame/StaticObject.h"
#include "clientGame/TangibleObject.h"
#include "clientObject/ComponentAppearanceTemplate.h"
#include "clientObject/DetailAppearanceTemplate.h"
#include "fileInterface/StdioFile.h"
#include "sharedCollision/CollisionProperty.h"
#include "sharedCollision/CollisionWorld.h"
#include "sharedFile/Iff.h"
#include "sharedFile/TreeFile.h"
#include "sharedGame/SharedObjectTemplate.h"
#include "sharedGame/SharedTangibleObjectTemplate.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/AppearanceTemplate.h"
#include "sharedObject/AppearanceTemplateList.h"
#include "sharedObject/ContainedByProperty.h"
#include "sharedObject/ObjectTemplate.h"
#include "sharedObject/ObjectTemplateList.h"
#include "sharedObject/World.h"

#include "ActionHack.h"
#include "ConfigGodClient.h"
#include "FavoritesWindow.h"
#include "FileControlReloadHandler.h"
#include "GodClientData.h"
#include "IconLoader.h"
#include "MainFrame.h"
#include "ServerCommander.h"
#include "TemplateEditorWindow.h"
#include "Unicode.h"

#include <qmessagebox.h>
#include <process.h>
#include <qfiledialog.h>
#include <qprocess.h>
#include <qstatusbar.h>
#include <qstringlist.h>
#include <qtimer.h>

#include <direct.h>
#include <map>
#include <set>
#include <sys/stat.h>

// ======================================================================

namespace ActionsObjectTemplateNamespace
{
	// A compile that takes longer than this (WSL cold start included) is killed.
	const int cms_compileTimeoutMs = 120 * 1000;

	std::string toLower(const std::string& text)
	{
		std::string result = text;
		for (size_t i = 0; i < result.size(); ++i)
			if (result[i] >= 'A' && result[i] <= 'Z')
				result[i] = static_cast<char>(result[i] - 'A' + 'a');
		return result;
	}

	//----------------------------------------------------------------------

	/** Lower case, forward slashes, no doubled slashes: for comparing file paths. */
	std::string normalizePath(const std::string& path)
	{
		const std::string lower = toLower(path);
		std::string result;
		for (size_t i = 0; i < lower.size(); ++i)
		{
			const char c = lower[i] == '\\' ? '/' : lower[i];
			if (c == '/' && !result.empty() && result[result.size() - 1] == '/')
				continue;
			result += c;
		}
		return result;
	}

	//----------------------------------------------------------------------

	/**
	 * The Template Editor hands over paths relative to the repository root (dsrc/sku.0/...).
	 * Anchor those on the root of the configured template source path.
	 */
	std::string makeAbsoluteTemplatePath(const std::string& path)
	{
		std::string result = path;
		for (size_t i = 0; i < result.size(); ++i)
			if (result[i] == '\\')
				result[i] = '/';

		const bool absolute = (result.size() > 1 && result[1] == ':') || (!result.empty() && result[0] == '/');
		if (absolute)
			return result;

		const std::string sourcePath = NON_NULL(ConfigGodClient::getData().templateClientSourcePath);
		const size_t dsrcPos = sourcePath.find("/dsrc/");
		if (result.find("dsrc/") == 0 && dsrcPos != std::string::npos)
			return sourcePath.substr(0, dsrcPos + 1) + result;

		return result;
	}

	//----------------------------------------------------------------------

	time_t getFileWriteTime(const std::string& path)
	{
		struct _stat info;
		if (_stat(path.c_str(), &info) != 0)
			return 0;
		return info.st_mtime;
	}

	//----------------------------------------------------------------------

	/** TemplateCompiler does not create output directories. */
	void makeParentDirectories(const std::string& filePath)
	{
		for (size_t slash = filePath.find('/', 3); slash != std::string::npos; slash = filePath.find('/', slash + 1))
			IGNORE_RETURN(_mkdir(filePath.substr(0, slash).c_str()));
	}

	//----------------------------------------------------------------------

	/** Splits a command line on whitespace; double quotes group an argument that contains spaces. */
	QStringList splitCommandLine(const std::string& commandLine)
	{
		QStringList result;
		std::string current;
		bool quoted = false;
		bool haveToken = false;

		for (size_t i = 0; i < commandLine.size(); ++i)
		{
			const char c = commandLine[i];
			if (c == '"')
			{
				quoted = !quoted;
				haveToken = true;
			}
			else if (!quoted && (c == ' ' || c == '\t'))
			{
				if (haveToken)
					result.append(current.c_str());
				current.erase();
				haveToken = false;
			}
			else
			{
				current += c;
				haveToken = true;
			}
		}

		if (haveToken)
			result.append(current.c_str());

		return result;
	}

	//----------------------------------------------------------------------

	/** Removes ANSI colour sequences (titan.sh logs in colour) from a line of tool output. */
	std::string stripAnsi(const std::string& line)
	{
		std::string result;
		for (size_t i = 0; i < line.size(); ++i)
		{
			if (line[i] == '\x1b' && i + 1 < line.size() && line[i + 1] == '[')
			{
				i += 2;
				while (i < line.size() && !((line[i] >= 'A' && line[i] <= 'Z') || (line[i] >= 'a' && line[i] <= 'z')))
					++i;
				continue;
			}
			result += line[i];
		}
		return result;
	}

	//----------------------------------------------------------------------

	bool looksLikeError(const std::string& line)
	{
		const std::string lower = toLower(line);
		return lower.find("error") != std::string::npos || lower.find("fatal") != std::string::npos || lower.find("fail") != std::string::npos;
	}

	//----------------------------------------------------------------------

	void report(const std::string& text)
	{
		MainFrame::getInstance().textToConsole(("Templates: " + text).c_str());
	}

	//----------------------------------------------------------------------

	void reportAppearances(const std::string& text)
	{
		MainFrame::getInstance().textToConsole(("Appearances: " + text).c_str());
	}

	//----------------------------------------------------------------------

	std::string intToString(int value)
	{
		char buffer[32];
		IGNORE_RETURN(_snprintf(buffer, sizeof(buffer), "%d", value));
		buffer[sizeof(buffer) - 1] = '\0';
		return buffer;
	}

	//----------------------------------------------------------------------

	void showStatus(const std::string& text)
	{
		MainFrame::getInstance().statusBar()->message(text.c_str(), 10000);
	}

	//----------------------------------------------------------------------

	/**
	 * ObjectTemplateList::reload re-reads parameters into the existing template, so it cannot
	 * follow a change of template class or of @base: the base reference counts would no longer
	 * match the references every live object holds. Returns why the file can't be reloaded.
	 */
	std::string getReloadProblem(Iff& iff, const ObjectTemplate& loadedTemplate)
	{
		if (iff.getCurrentName() != loadedTemplate.getId())
			return "its template class changed";

		std::string newBase;
		iff.enterForm();
		if (iff.isCurrentForm() && iff.getCurrentName() == TAG(D,E,R,V))
		{
			iff.enterForm();
			iff.enterChunk();
			iff.read_string(newBase);
			iff.exitChunk(true);
			iff.exitForm(true);
		}
		iff.exitForm(true);

		const ObjectTemplate* const base = loadedTemplate.getBaseTemplate();
		const std::string oldBase = base ? base->getName() : "";
		if (normalizePath(oldBase) != normalizePath(newBase))
			return "its @base changed from '" + oldBase + "' to '" + newBase + "'";

		return std::string();
	}

	//----------------------------------------------------------------------

	/**
	 * Rebuilds a placed object's appearance from its (reloaded) template the way its constructor did.
	 * Returns 0 on success, else why the object was left alone. Objects whose appearance pointer is
	 * held elsewhere are skipped: creatures (worn items), anything attached or contained, and
	 * buildings (portal layout).
	 *
	 * The floor and the collision world's entries are built from the appearance (Floor keeps a
	 * pointer to it), so the object leaves the collision world while its appearance changes and
	 * comes back with a new floor and fresh extents.
	 */
	const char* refreshObjectAppearance(ClientObject& object)
	{
		if (object.asCreatureObject())
			return "creature";
		if (object.asShipObject())
			return "ship";
		if (object.asCellObject() || object.getPortalProperty())
			return "building or cell";
		if (object.isChildObject())
			return "attached to another object";

		const ContainedByProperty* const containedBy = object.getContainedByProperty();
		const Object* const container = containedBy ? containedBy->getContainedBy() : 0;
		if (container && !(container->asClientObject() && container->asClientObject()->asCellObject()))
			return "inside a container";

		TangibleObject* const tangible = object.asTangibleObject();
		StaticObject* const staticObject = object.asStaticObject();
		const SharedTangibleObjectTemplate* const tangibleTemplate = tangible ? dynamic_cast<const SharedTangibleObjectTemplate*>(object.getObjectTemplate()) : 0;
		const SharedObjectTemplate* const staticTemplate = (staticObject && object.getObjectTemplate()) ? object.getObjectTemplate()->asSharedObjectTemplate() : 0;

		if (!tangible && !staticObject)
			return "not a tangible or static object";
		if (tangible && !tangibleTemplate)
			return "not a shared tangible template";
		if (staticObject && (!staticTemplate || staticTemplate->getAppearanceFilename().empty()))
			return "no appearance";

		CollisionProperty* const collision = object.getCollisionProperty();
		const bool inCollisionWorld = collision && collision->isInCollisionWorld();
		if (inCollisionWorld && collision->getDisableCollisionWorldAddRemove())
			return "pinned in the collision world";

		Appearance* staticAppearance = 0;
		if (staticObject)
		{
			staticAppearance = AppearanceTemplateList::createAppearance(staticTemplate->getAppearanceFilename().c_str());
			if (!staticAppearance)
				return "appearance failed to load";
		}

		if (inCollisionWorld)
			CollisionWorld::removeObject(&object);

		if (tangible)
		{
			//-- changeAppearance drops the customization data; replay the server's string onto the new appearance
			const std::string appearanceData = tangible->getAppearanceData();
			tangible->changeAppearance(*tangibleTemplate);
			if (!appearanceData.empty())
			{
				tangible->setAppearanceData(std::string());
				tangible->setAppearanceData(appearanceData);
			}
		}
		else
			staticObject->setAppearance(staticAppearance);

		//-- the extents are cached from the old appearance; drop them so they rebuild
		if (collision)
		{
			collision->attachSourceExtent(0);
			collision->reloadFloor();
			if (inCollisionWorld)
				CollisionWorld::addObject(&object);
		}

		GodClientData::getInstance().refreshGhostAppearance(object);
		return 0;
	}

	//----------------------------------------------------------------------

	/**
	 * Refreshes each object (see refreshObjectAppearance) and describes the outcome for the Console,
	 * e.g. "refreshed 3 placed object(s); left alone: 1 creature (re-place those to see the change)".
	 * Returns the number of objects refreshed.
	 */
	int refreshObjects(const std::vector<ClientObject*>& objects, std::string& summary)
	{
		int refreshed = 0;
		std::map<std::string, int> skipped;
		for (std::vector<ClientObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
		{
			const char* const reason = refreshObjectAppearance(**it);
			if (reason)
				++skipped[reason];
			else
				++refreshed;
		}

		summary = "refreshed " + intToString(refreshed) + " placed object(s)";
		if (!skipped.empty())
		{
			summary += "; left alone:";
			for (std::map<std::string, int>::const_iterator s = skipped.begin(); s != skipped.end(); ++s)
				summary += std::string(s != skipped.begin() ? "," : "") + " " + intToString(s->second) + " " + s->first;
			summary += " (re-place those to see the change)";
		}

		return refreshed;
	}

	//----------------------------------------------------------------------

	/** The objects placed in the world. Collect first: refreshing must not run while walking the world lists. */
	void collectPlacedObjects(std::vector<ClientObject*>& objects)
	{
		const int lists[] = { static_cast<int>(WOL_Tangible), static_cast<int>(WOL_TangibleNotTargetable) };
		for (size_t l = 0; l < sizeof(lists) / sizeof(lists[0]); ++l)
		{
			for (int i = 0; i < ClientWorld::getNumberOfObjects(lists[l]); ++i)
			{
				Object* const worldObject = ClientWorld::getObject(lists[l], i);
				ClientObject* const object = worldObject ? worldObject->asClientObject() : 0;
				if (object)
					objects.push_back(object);
			}
		}
	}

	//----------------------------------------------------------------------

	/**
	 * Adds the files an appearance template is built from to fileNames: its own file, plus the
	 * levels of a .lod and the parts of a .cmp, recursively. Levels a .lod has not loaded yet are
	 * added by name.
	 */
	void collectAppearanceFiles(const AppearanceTemplate& appearanceTemplate, std::set<std::string>& fileNames)
	{
		const char* const name = appearanceTemplate.getName();
		if (!name || !*name || !fileNames.insert(normalizePath(name)).second)
			return;

		const DetailAppearanceTemplate* const detailTemplate = dynamic_cast<const DetailAppearanceTemplate*>(&appearanceTemplate);
		if (detailTemplate)
		{
			for (int i = 0; i < detailTemplate->getDetailLevelCount(); ++i)
			{
				const AppearanceTemplate* const child = detailTemplate->getAppearanceTemplate(i);
				const char* const childName = detailTemplate->getAppearanceTemplateName(i);
				if (child)
					collectAppearanceFiles(*child, fileNames);
				else if (childName && *childName)
					IGNORE_RETURN(fileNames.insert(normalizePath(childName)));
			}
		}

		const ComponentAppearanceTemplate* const componentTemplate = dynamic_cast<const ComponentAppearanceTemplate*>(&appearanceTemplate);
		if (componentTemplate)
		{
			for (int i = 0; i < componentTemplate->getNumberOfComponents(); ++i)
			{
				const AppearanceTemplate* const child = componentTemplate->getAppearanceTemplate(i);
				if (child)
					collectAppearanceFiles(*child, fileNames);
			}
		}
	}

	//----------------------------------------------------------------------

	/**
	 * The appearance files a placed object shows: its template's appearanceFilename (usually an .apt
	 * that redirects to a .lod) and everything its current appearance is built from.
	 */
	void collectObjectAppearanceFiles(const ClientObject& object, std::set<std::string>& fileNames)
	{
		const ObjectTemplate* const objectTemplate = object.getObjectTemplate();
		const SharedObjectTemplate* const sharedTemplate = objectTemplate ? objectTemplate->asSharedObjectTemplate() : 0;
		if (sharedTemplate && !sharedTemplate->getAppearanceFilename().empty())
			IGNORE_RETURN(fileNames.insert(normalizePath(sharedTemplate->getAppearanceFilename())));

		const Appearance* const appearance = object.getAppearance();
		const AppearanceTemplate* const appearanceTemplate = appearance ? appearance->getAppearanceTemplate() : 0;
		if (appearanceTemplate)
			collectAppearanceFiles(*appearanceTemplate, fileNames);
	}

	//----------------------------------------------------------------------

	bool reloadFromFileControl(const std::string& relativePath)
	{
		return ActionsObjectTemplate::reloadClientTemplate(relativePath, std::string());
	}
}

using namespace ActionsObjectTemplateNamespace;

// ======================================================================

const char * const ActionsObjectTemplate::DragMessages::ObjectTemplate_SERVER_TEMPLATE_DRAGGED = "ObjectTemplateListView::DragMessages::ObjectTemplate_SERVER_TEMPLATE_DRAGGED";

const char* const ActionsObjectTemplate::ms_objectTemplateSourceExtension = ".tpf";
const char* const ActionsObjectTemplate::ms_objectTemplateCompiledExtension = ".iff";

//-----------------------------------------------------------------------

ActionsObjectTemplate::ActionsObjectTemplate()
: QObject(),
Singleton<ActionsObjectTemplate>(),
m_serverCreate(0),
m_serverView(0),
m_serverEdit(0),
m_serverRevert(0),
m_serverRefresh(0),
m_serverSubmit(0),
m_serverCompile(0),
m_serverReload(0),
m_serverReplace(0),
m_clientCreate(0),
m_clientView(0),
m_clientEdit(0),
m_clientRevert(0),
m_clientRefresh(0),
m_clientSubmit(0),
m_clientCompile(0),
m_clientReplace(0),
m_reloadAppearance(0),
m_serverAddToFavorites(0),
m_clientAddToFavorites(0),
m_compileProcess(0),
m_compileTimer(0),
m_compileQueue(),
m_compileLine(),
m_compileErrorLines(0),
m_selectedServerPath(),
m_selectedClientPath(),
m_isServerFile(false),
m_isServerNew(false),
m_isServerEdit(false),
m_isClientFile(false),
m_isClientNew(false),
m_isClientEdit(false)
{
	QWidget * const p =&MainFrame::getInstance();

	m_serverCreate    = new ActionHack("New ServerObjectTemplate",                  IL_PIXMAP(hi16_mime_document2),  "&New",     0, p, "ServerObjectTemplate_new");
	IGNORE_RETURN(connect(m_serverCreate, SIGNAL(activated()), this, SLOT(onServerNew())));

	m_serverView      = new ActionHack("View ServerObjectTemplate in Editor",       IL_PIXMAP(hi16_mime_document),   "&View",    0, p, "ServerObjectTemplate_view");
	IGNORE_RETURN(connect(m_serverView, SIGNAL(activated()), this, SLOT(onServerView())));

	m_serverEdit      = new ActionHack("Edit ServerObjectTemplate from Perforce",   IL_PIXMAP(hi16_action_edit),     "&Edit",    0, p, "ServerObjectTemplate_edit");
	IGNORE_RETURN(connect(m_serverEdit, SIGNAL(activated()), this, SLOT(onServerEdit())));

	m_serverRevert    = new ActionHack("Revert ServerObjectTemplate from Perforce", IL_PIXMAP(hi16_action_revert),   "Re&vert",  0, p, "ServerObjectTemplate_revert");
	IGNORE_RETURN(connect(m_serverRevert, SIGNAL(activated()), this, SLOT(onServerRevert())));

	m_serverSubmit    = new ActionHack("Submit ServerObjectTemplate to Perforce",   IL_PIXMAP(hi16_action_finish),   "&Submit",  0, p, "ServerObjectTemplate_submit");
	IGNORE_RETURN(connect(m_serverSubmit, SIGNAL(activated()), this, SLOT(onServerSubmit())));

	m_serverCompile   = new ActionHack("Compile ServerObjectTemplate and reload it on the server", IL_PIXMAP(hi16_action_gear), "&Compile", 0, p, "ServerObjectTemplate_compile");
	IGNORE_RETURN(connect(m_serverCompile, SIGNAL(activated()), this, SLOT(onServerCompile())));

	m_serverRefresh   = new ActionHack("Refresh ServerObjectTemplate List",         IL_PIXMAP(hi16_action_reload),   "&Refresh", 0, p, "ServerObjectTemplate_refresh");

	m_serverReload  = new ActionHack("Reload ServerObjectTemplate on Server", IL_PIXMAP(hi16_filesys_network), "&Server Reload",  0, p, "ServerObjectTemplate_server_reload");
	IGNORE_RETURN(connect(m_serverReload, SIGNAL(activated()), this, SLOT(onServerReload())));

	m_serverReplace = new ActionHack("Replace selection with ServerObjectTemplate", IL_PIXMAP(hi16_filesys_network), "&Server Replace",  0, p, "ServerObjectTemplate_server_replace");
	IGNORE_RETURN(connect(m_serverReplace, SIGNAL(activated()), this, SLOT(onServerReplace())));

	m_clientCreate    = new ActionHack("New ClientObjectTemplate",                  IL_PIXMAP(hi16_mime_document2),  "&New",     0, p, "ClientObjectTemplate_new");
	IGNORE_RETURN(connect(m_clientCreate, SIGNAL(activated()), this, SLOT(onClientNew())));

	m_clientView      = new ActionHack("View ClientObjectTemplate in Editor",       IL_PIXMAP(hi16_mime_document),   "&View",    0, p, "ClientObjectTemplate_view");
	IGNORE_RETURN(connect(m_clientView, SIGNAL(activated()), this, SLOT(onClientView())));

	m_clientEdit      = new ActionHack("Edit ClientObjectTemplate from Perforce",   IL_PIXMAP(hi16_action_edit),     "&Edit",    0, p, "ClientObjectTemplate_edit");
	IGNORE_RETURN(connect(m_clientEdit, SIGNAL(activated()), this, SLOT(onClientEdit())));

	m_clientRevert    = new ActionHack("Revert ClientObjectTemplate from Perforce", IL_PIXMAP(hi16_action_revert),   "Re&vert",  0, p, "ClientObjectTemplate_revert");
	IGNORE_RETURN(connect(m_clientRevert, SIGNAL(activated()), this, SLOT(onClientRevert())));

	m_clientSubmit    = new ActionHack("Submit ClientObjectTemplate to Perforce",   IL_PIXMAP(hi16_action_finish),   "&Submit",  0, p, "ClientObjectTemplate_submit");
	IGNORE_RETURN(connect(m_clientSubmit, SIGNAL(activated()), this, SLOT(onClientSubmit())));

	m_clientCompile   = new ActionHack("Compile ClientObjectTemplate and reload it in this client", IL_PIXMAP(hi16_action_gear), "&Compile", 0, p, "ClientObjectTemplate_compile");
	IGNORE_RETURN(connect(m_clientCompile, SIGNAL(activated()), this, SLOT(onClientCompile())));

	m_clientRefresh   = new ActionHack("Refresh ClientObjectTemplate List",         IL_PIXMAP(hi16_action_reload),   "&Refresh", 0, p, "ClientObjectTemplate_refresh");

	m_clientReplace   = new ActionHack("Replace selection with ClientObjectTemplate", IL_PIXMAP(hi16_action_gear),     "&Replace", 0, p, "ClientObjectTemplate_replace");
	IGNORE_RETURN(connect(m_clientReplace, SIGNAL(activated()), this, SLOT(onClientReplace())));

	m_reloadAppearance = new ActionHack("Reload the selection's appearance files (.apt, .lod, .msh) from disk", IL_PIXMAP(hi16_action_reload), "Reload &Appearance", 0, p, "ObjectTemplate_reloadAppearance");
	IGNORE_RETURN(connect(m_reloadAppearance, SIGNAL(activated()), this, SLOT(onReloadAppearance())));

	m_serverAddToFavorites =new ActionHack("Add ServerTemplate to favorites", IL_PIXMAP(hi16_action_gear), "&Add to favorites", 0, p, "ServerObjectTemplate_addToFavorites");
	IGNORE_RETURN(connect(m_serverAddToFavorites, SIGNAL(activated()), this, SLOT(onServerAddToFavorites())));

	m_clientAddToFavorites = new ActionHack("Add ClientTemplate to favorites", IL_PIXMAP(hi16_action_gear), "&Add to favorites", 0, p, "ClientObjectTemplate_addToFavorites");
	IGNORE_RETURN(connect(m_clientAddToFavorites, SIGNAL(activated()), this, SLOT(onClientAddToFavorites())));

	m_serverView->setEnabled        (false);
	m_serverEdit->setEnabled        (false);
	m_serverRevert->setEnabled      (false);
	m_serverReload->setEnabled      (false);
	m_serverSubmit->setEnabled      (false);
	m_serverCompile->setEnabled     (false);
	m_serverReplace->setEnabled     (false);

	m_clientView->setEnabled        (false);
	m_clientEdit->setEnabled        (false);
	m_clientRevert->setEnabled      (false);
	m_clientSubmit->setEnabled      (false);
	m_clientCompile->setEnabled     (false);
	m_clientReplace->setEnabled     (false);

	m_serverAddToFavorites->setEnabled (false);
	m_clientAddToFavorites->setEnabled (false);

	m_compileProcess = new QProcess(this, "ObjectTemplateCompileProcess");
	IGNORE_RETURN(connect(m_compileProcess, SIGNAL(readyReadStdout()), this, SLOT(onCompileOutput())));
	IGNORE_RETURN(connect(m_compileProcess, SIGNAL(readyReadStderr()), this, SLOT(onCompileOutput())));
	IGNORE_RETURN(connect(m_compileProcess, SIGNAL(processExited()), this, SLOT(onCompileExited())));

	m_compileTimer = new QTimer(this, "ObjectTemplateCompileTimer");
	IGNORE_RETURN(connect(m_compileTimer, SIGNAL(timeout()), this, SLOT(onCompileTimedOut())));

	FileControlReloadHandler::setTemplateReloadFunction(reloadFromFileControl);
}

//-----------------------------------------------------------------------

ActionsObjectTemplate::~ActionsObjectTemplate()
{
	FileControlReloadHandler::setTemplateReloadFunction(0);

	//-- the process and timer are QObject children; just make sure no compile outlives us
	if (m_compileProcess && m_compileProcess->isRunning())
		m_compileProcess->kill();
	m_compileProcess = 0;
	m_compileTimer = 0;

	m_serverCreate = 0;
	m_serverView = 0;
	m_serverEdit = 0;
	m_serverRevert = 0;
	m_serverRefresh = 0;
	m_serverSubmit = 0;
	m_serverCompile = 0;
	m_serverReload = 0;
	m_serverReplace = 0;
	m_clientCreate = 0;
	m_clientView = 0;
	m_clientEdit = 0;
	m_clientRevert = 0;
	m_clientRefresh = 0;
	m_clientSubmit = 0;
	m_clientCompile = 0;
	m_clientReplace = 0;
	m_reloadAppearance = 0;
	m_serverAddToFavorites = 0;
	m_clientAddToFavorites = 0;
}

//-----------------------------------------------------------------------

/**
 * Open an arbitrary file in the user's editor.  Editor is set in [GodClient] editor
 *
 */
void ActionsObjectTemplate::doEditFile(const std::string& filename) const
{
	AbstractFile * fl = new StdioFile(filename.c_str(), "r");

	if(!fl->isOpen())
	{
		delete fl;
		fl = new StdioFile(filename.c_str(), "w");

		if(!fl->isOpen())
		{
			const std::string err = std::string("Can't create new file: [") + filename + "].";
			IGNORE_RETURN(QMessageBox::warning(&MainFrame::getInstance(), "Warning", err.c_str()));
			delete fl;
			return;		
		}

		delete fl;
	}

	const char * editor = NON_NULL(ConfigGodClient::getData().templateEditor);

	std::string arg0 = editor;

	const size_t slashpos = arg0.find_last_of("\\/");
	if(slashpos != arg0.npos && slashpos <(arg0.size() - 1)) //lint !e737 implicit promotion, bug in STL with size_t and std::string::npos being of different signage
		arg0 = arg0.substr(slashpos + 1);

	const int retval = _spawnlp(_P_NOWAIT, editor, arg0.c_str(), filename.c_str(), 0);

	if(retval == -1)
	{
		const std::string err = std::string("Unable to execute ObjectTemplateEditor [") + editor + "]: " + strerror(errno);
		IGNORE_RETURN(QMessageBox::warning(&MainFrame::getInstance(), "Warning", err.c_str()));
		return;
	}
}
/**
 * Simply convert all / to .
 * Does not strip .class or .ObjectTemplate suffix
 */

//-----------------------------------------------------------------------

const std::string ActionsObjectTemplate::convertToClassPath(const std::string& path)
{
	//-- convert the filesystem path to java package-class notation
	std::string classPath = path;
	size_t slashPos = classPath.find('/', 0);

	while(slashPos != classPath.npos) //lint !e737 implicit promotion, bug in STL with size_t and std::string::npos being of different signage
	{
		classPath [slashPos] = '.';
		slashPos = classPath.find('/', slashPos + 1);
	}

	return classPath;
}


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
//BEGIN SERVER OBJECT TEMPLATE FUNCTIONS

/**
 * Slot to handle updating the global ObjectTemplate or ObjectTemplate path selection.
 */
void ActionsObjectTemplate::onServerObjectTemplatePathSelectionChanged(const std::string& path, bool isFile, bool isNew, bool isEdit)
{
	m_selectedServerPath = path;
	m_isServerFile       = isFile;
	m_isServerNew        = isNew;
	m_isServerEdit       = isEdit;

	m_serverView->setEnabled        (!m_selectedServerPath.empty() && isFile);
	m_serverEdit->setEnabled        (m_serverView->isEnabled());
	m_serverReload->setEnabled      (m_serverView->isEnabled());

	m_serverRevert->setEnabled      (m_serverView->isEnabled() && (isEdit || isNew));
	m_serverSubmit->setEnabled      (m_serverView->isEnabled() && (isEdit || isNew));
	m_serverCompile->setEnabled     (m_serverView->isEnabled());
	m_serverReplace->setEnabled     (m_serverView->isEnabled());

	m_serverAddToFavorites->setEnabled(m_serverView->isEnabled());
}

//-----------------------------------------------------------------------

/**
 * Revert the selected ObjectTemplate.  Calls p4 revert on both the class and ObjectTemplate file
 */
void ActionsObjectTemplate::onServerRevert()
{
	if (m_selectedServerPath.empty() || !m_isServerFile || !(m_isServerEdit || m_isServerNew))
		return;

	{
		const std::string classPath = ActionsObjectTemplate::convertToClassPath(m_selectedServerPath);
		const std::string msg = std::string("Really revert ObjectTemplate: [") + classPath + "]?";
		const int retval = QMessageBox::warning(&MainFrame::getInstance(), "Warning", msg.c_str(), QMessageBox::Yes, QMessageBox::No);

		if (retval == QMessageBox::No)
			return;
	}

	m_serverRefresh->doActivate();
}

//-----------------------------------------------------------------------

/**
 * Opens the editor on the selected .ObjectTemplate file
 *
 */
void ActionsObjectTemplate::onServerView() const
{
	if (m_selectedServerPath.empty() || !m_isServerFile)
		return;

	MainFrame & mf = MainFrame::getInstance();
	if (!mf.m_templateEditor)
		mf.m_templateEditor = new TemplateEditorWindow(0, "TemplateEditor");
	if (mf.m_templateEditor)
	{
		mf.m_templateEditor->loadTemplate(getServerObjectTemplateTpfFilePath());
		mf.m_templateEditor->show();
		mf.m_templateEditor->raise();
	}
}

//-----------------------------------------------------------------------

/**
 * p4 edits the selected .ObjectTemplate and .class file, and opens the editor on the .ObjectTemplate file
 *
 */
void ActionsObjectTemplate::onServerEdit()
{
	if (m_selectedServerPath.empty() || !m_isServerFile)
		return;

	MainFrame & mf = MainFrame::getInstance();
	if (!mf.m_templateEditor)
		mf.m_templateEditor = new TemplateEditorWindow(0, "TemplateEditor");
	if (mf.m_templateEditor)
	{
		mf.m_templateEditor->loadTemplate(getServerObjectTemplateTpfFilePath());
		mf.m_templateEditor->show();
		mf.m_templateEditor->raise();
	}
}

//-----------------------------------------------------------------------

/**
 * Prompts the user to select a new .ObjectTemplate file.  Adds the resulting .ObjectTemplate and .class files to perforce.
 * Creates the .ObjectTemplate file and opens the editor on it.
 *
 */
void ActionsObjectTemplate::onServerNew()
{
	std::string actualPath = m_selectedServerPath;

	//-- trim the filename from the path
	if (!actualPath.empty() && m_isServerFile)
	{
		const size_t slashpos = actualPath.rfind('/');

		if (slashpos == actualPath.npos) //lint !e737 implicit promotion, bug in STL with size_t and std::string::npos being of different signage 
			actualPath.clear();
		else
			actualPath = actualPath.substr(0, slashpos);
	}

	m_serverRefresh->doActivate();
}

//-----------------------------------------------------------------------

/**
 * Requests the server to relaod the ObjectTemplate.
 *
 */
void ActionsObjectTemplate::onServerReload() const
{
	if(m_selectedServerPath.empty() || !m_isServerFile)
		return;

	std::string classPath = "object/";
	classPath += m_selectedServerPath + ".iff";
	const std::string msg = std::string("Really server-reload ObjectTemplate: [") + classPath + "]?";
	const int retval = QMessageBox::warning(&MainFrame::getInstance(), "Warning", msg.c_str(), QMessageBox::Yes, QMessageBox::No);

	if(retval == QMessageBox::No)
		return;

	IGNORE_RETURN(ServerCommander::getInstance().serverTemplateReload(classPath));
}

//-----------------------------------------------------------------------

/**
 * Submit the selected ObjectTemplate to perforce, reverting unchanged first if possible.
 *
 */
void ActionsObjectTemplate::onServerSubmit()
{
	if (m_selectedServerPath.empty() || !m_isServerFile || !(m_isServerEdit || m_isServerNew))
		return;

	const std::string classPath = ActionsObjectTemplate::convertToClassPath(m_selectedServerPath);
	const std::string msg = std::string("Really submit ObjectTemplate: [") + classPath + "]?";
	const int retval = QMessageBox::warning(&MainFrame::getInstance(), "Warning", msg.c_str(), QMessageBox::Yes, QMessageBox::No);

	if (retval == QMessageBox::No)
		return;

	m_serverRefresh->doActivate();
}

//-----------------------------------------------------------------------

/**
 * Compile the selected ObjectTemplate's .tpf, then have the server reload it.
 * Compiler output goes to the Console.
 */
void ActionsObjectTemplate::onServerCompile()
{
	if(m_selectedServerPath.empty() || !m_isServerFile)
		return;

	IGNORE_RETURN(compileTemplate(getServerObjectTemplateTpfFilePath()));
}

//-----------------------------------------------------------------------

const std::string ActionsObjectTemplate::getServerObjectTemplateTpfFilePath() const
{
	if (m_selectedServerPath.empty() || !m_isServerFile)
		return "";

	const char* const srcPath = NON_NULL(ConfigGodClient::getData().templateServerSourcePath);
	return std::string(srcPath) + "/" + m_selectedServerPath + ms_objectTemplateSourceExtension;
}

//-----------------------------------------------------------------------

const std::string ActionsObjectTemplate::getServerObjectTemplateIffFilePath() const
{
	if (m_selectedServerPath.empty() || !m_isServerFile)
		return "";

	const char* const iffPath = NON_NULL(ConfigGodClient::getData().templateServerIffPath);
	return std::string(iffPath) + "/" + m_selectedServerPath + ms_objectTemplateCompiledExtension;
}

//-----------------------------------------------------------------------

bool ActionsObjectTemplate::getSelectedServerObjectTemplate(std::string& path) const
{
	if(m_selectedServerPath.empty() || !m_isServerFile)
		return false;

	path = convertToClassPath(m_selectedServerPath);
	return true;
}

//END SERVER OBJECT TEMPLATE FUNCTIONS
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
//BEGIN *CLIENT* OBJECT TEMPLATE FUNCTIONS

/**
 * Slot to handle updating the global ObjectTemplate or ObjectTemplate path selection.
 */
void ActionsObjectTemplate::onClientObjectTemplatePathSelectionChanged(const std::string& path, bool isFile, bool isNew, bool isEdit)
{
	m_selectedClientPath = path;
	m_isClientFile = isFile;
	m_isClientNew  = isNew;
	m_isClientEdit = isEdit;

	m_clientView->setEnabled        (!m_selectedClientPath.empty() && isFile);
	m_clientEdit->setEnabled        (m_clientView->isEnabled());

	m_clientRevert->setEnabled      (m_clientView->isEnabled() && (isEdit || isNew));
	m_clientSubmit->setEnabled      (m_clientView->isEnabled() && (isEdit || isNew));
	m_clientCompile->setEnabled     (m_clientView->isEnabled());
	m_clientReplace->setEnabled     (m_clientView->isEnabled());

	m_clientAddToFavorites->setEnabled(m_clientView->isEnabled());
}

//-----------------------------------------------------------------------

/**
 * Revert the selected ObjectTemplate.  Calls p4 revert on both the class and ObjectTemplate file
 */
void ActionsObjectTemplate::onClientRevert()
{
	if (m_selectedClientPath.empty() || !m_isClientFile || !(m_isClientEdit || m_isClientNew))
		return;

	{
		const std::string classPath = ActionsObjectTemplate::convertToClassPath(m_selectedClientPath);
		const std::string msg = std::string("Really revert ObjectTemplate: [") + classPath + "]?";
		const int retval = QMessageBox::warning(&MainFrame::getInstance(), "Warning", msg.c_str(), QMessageBox::Yes, QMessageBox::No);

		if (retval == QMessageBox::No)
			return;
	}

	m_clientRefresh->doActivate();
}

//-----------------------------------------------------------------------

/**
 * Opens the editor on the selected .ObjectTemplate file
 *
 */
void ActionsObjectTemplate::onClientView() const
{
	if (m_selectedClientPath.empty() || !m_isClientFile)
		return;

	MainFrame & mf = MainFrame::getInstance();
	if (!mf.m_templateEditor)
		mf.m_templateEditor = new TemplateEditorWindow(0, "TemplateEditor");
	if (mf.m_templateEditor)
	{
		mf.m_templateEditor->loadTemplate(getClientObjectTemplateTpfFilePath());
		mf.m_templateEditor->show();
		mf.m_templateEditor->raise();
	}
}

//-----------------------------------------------------------------------

/**
 * p4 edits the selected .ObjectTemplate and .class file, and opens the editor on the .ObjectTemplate file
 *
 */
void ActionsObjectTemplate::onClientEdit()
{
	if (m_selectedClientPath.empty() || !m_isClientFile)
		return;

	MainFrame & mf = MainFrame::getInstance();
	if (!mf.m_templateEditor)
		mf.m_templateEditor = new TemplateEditorWindow(0, "TemplateEditor");
	if (mf.m_templateEditor)
	{
		mf.m_templateEditor->loadTemplate(getClientObjectTemplateTpfFilePath());
		mf.m_templateEditor->show();
		mf.m_templateEditor->raise();
	}
}

//-----------------------------------------------------------------------

/**
 * Prompts the user to select a new .ObjectTemplate file.  Adds the resulting .ObjectTemplate and .class files to perforce.
 * Creates the .ObjectTemplate file and opens the editor on it.
 *
 */
void ActionsObjectTemplate::onClientNew()
{
	std::string actualPath = m_selectedClientPath;

	//-- trim the filename from the path
	if (!actualPath.empty() && m_isClientFile)
	{
		const size_t slashpos = actualPath.rfind('/');

		if (slashpos == actualPath.npos) //lint !e737 implicit promotion, bug in STL with size_t and std::string::npos being of different signage 
			actualPath.clear();
		else
			actualPath = actualPath.substr(0, slashpos);
	}

	m_clientRefresh->doActivate();
}

//-----------------------------------------------------------------------

/**
 * Submit the selected ObjectTemplate to perforce, reverting unchanged first if possible.
 *
 */
void ActionsObjectTemplate::onClientSubmit()
{
	if (m_selectedClientPath.empty() || !m_isClientFile || !(m_isClientEdit || m_isClientNew))
		return;

	const std::string classPath = ActionsObjectTemplate::convertToClassPath(m_selectedClientPath);
	const std::string msg = std::string("Really submit ObjectTemplate: [") + classPath + "]?";
	const int retval = QMessageBox::warning(&MainFrame::getInstance(), "Warning", msg.c_str(), QMessageBox::Yes, QMessageBox::No);

	if (retval == QMessageBox::No)
		return;

	m_clientRefresh->doActivate();
}

//-----------------------------------------------------------------------

/**
 * Compile the selected ObjectTemplate's .tpf, then reload it in this client and refresh the
 * placed objects that use it. Compiler output goes to the Console.
 */
void ActionsObjectTemplate::onClientCompile()
{
	if(m_selectedClientPath.empty() || !m_isClientFile)
		return;

	IGNORE_RETURN(compileTemplate(getClientObjectTemplateTpfFilePath()));
}

//-----------------------------------------------------------------------

const std::string ActionsObjectTemplate::getClientObjectTemplateTpfFilePath() const
{
	if (m_selectedClientPath.empty() || !m_isClientFile)
		return "";

	const char* const srcPath = NON_NULL(ConfigGodClient::getData().templateClientSourcePath);
	return std::string(srcPath) + "/" + m_selectedClientPath + ms_objectTemplateSourceExtension;
}

//-----------------------------------------------------------------------

const std::string ActionsObjectTemplate::getClientObjectTemplateIffFilePath() const
{
	if (m_selectedClientPath.empty() || !m_isClientFile)
		return "";

	const char* const iffPath = NON_NULL(ConfigGodClient::getData().templateClientIffPath);
	return std::string(iffPath) + "/" + m_selectedClientPath + ms_objectTemplateCompiledExtension;
}

//-----------------------------------------------------------------------

bool ActionsObjectTemplate::getSelectedClientObjectTemplate(std::string& path) const
{
	if(m_selectedClientPath.empty() || !m_isClientFile)
		return false;

	path = convertToClassPath(m_selectedClientPath);
	return true;
}

//-----------------------------------------------------------------------

void ActionsObjectTemplate::onServerReplace()
{
	replaceObject(false);
}

//-----------------------------------------------------------------------

void ActionsObjectTemplate::onClientReplace()
{
	replaceObject(true);
}

//-----------------------------------------------------------------------

void ActionsObjectTemplate::onClientAddToFavorites()
{
	FavoritesWindow *pFavWindow = MainFrame::getInstance().getFavoritesWindow();
	if(pFavWindow)
	{
		pFavWindow->addClientTemplateToRoot(m_selectedClientPath);
	}
}

//-----------------------------------------------------------------------

void ActionsObjectTemplate::onServerAddToFavorites()
{
	FavoritesWindow *pFavWindow = MainFrame::getInstance().getFavoritesWindow();
	if(pFavWindow)
	{
		pFavWindow->addServerTemplateToRoot(m_selectedServerPath);
	}
}

//-----------------------------------------------------------------------

void ActionsObjectTemplate::replaceObject(bool client)
{
	//-- replace the current object with the one in the menu
	GodClientData * gcd = &GodClientData::getInstance();

	if (gcd->getSelectionEmpty())
	{
		const std::string err = std::string("No objects are currently selected!");
		IGNORE_RETURN(QMessageBox::warning(&MainFrame::getInstance(), "Warning", err.c_str()));
		return;
	}

	std::string path;
	
	if (client)
		path = std::string("object/") + m_selectedClientPath + std::string(".iff");
	else
		path = std::string("object/") + m_selectedServerPath + std::string(".iff");

	if (!path.empty())
	{
		GodClientData::ObjectList_t olist;

		gcd->getSelection(olist);
		gcd->clearSelection();

		for(GodClientData::ObjectList_t::iterator it = olist.begin(); it != olist.end(); ++it)
		{
			ClientObject * const obj = *it;

			if (!obj)
			{
				const std::string err = std::string("Invalid object selection list!");
				IGNORE_RETURN(QMessageBox::warning(&MainFrame::getInstance(), "Warning", err.c_str()));
				return;
			}

			Transform t = obj->getTransform_o2p();

			CellProperty * parentCellProperty = obj->getParentCell();
			
			IGNORE_RETURN(ServerCommander::getInstance().createObject("", path, parentCellProperty, t));
			IGNORE_RETURN(ServerCommander::getInstance().deleteObject(obj));
		}
	}
}

//-----------------------------------------------------------------------

//END *CLIENT* OBJECT TEMPLATE FUNCTIONS
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
//BEGIN COMPILE AND RELOAD

/**
 * Queue a .tpf for [GodClient] templateCompileCommand, which gets the source path as its last
 * argument. When it has written the .iff, a server template is reloaded on the server and a
 * shared template is reloaded in this client (see reloadClientTemplate).
 *
 * @return false if the path is not a template source under dsrc/.../compiled/game/
 */
bool ActionsObjectTemplate::compileTemplate(const std::string& tpfPath)
{
	CompileRequest request;
	request.tpfPath           = makeAbsoluteTemplatePath(tpfPath);
	request.server            = false;
	request.previousWriteTime = 0;

	const std::string lower   = toLower(request.tpfPath);
	const size_t      dsrcPos = lower.rfind("/dsrc/");
	const size_t      gamePos = lower.find("/compiled/game/");
	const size_t      extLen  = strlen(ms_objectTemplateSourceExtension);

	if (dsrcPos == std::string::npos || gamePos == std::string::npos || gamePos < dsrcPos || lower.size() <= extLen || lower.compare(lower.size() - extLen, extLen, ms_objectTemplateSourceExtension) != 0)
	{
		report("can't compile [" + tpfPath + "]: expected a .tpf under dsrc/.../compiled/game/");
		return false;
	}

	if (getFileWriteTime(request.tpfPath) == 0)
	{
		report("can't compile: [" + request.tpfPath + "] does not exist");
		return false;
	}

	//-- TemplateCompiler writes dsrc/<tree>/foo.tpf to data/<tree>/foo.iff
	request.iffPath = request.tpfPath.substr(0, dsrcPos) + "/data/" + request.tpfPath.substr(dsrcPos + 6);
	request.iffPath.replace(request.iffPath.size() - extLen, extLen, ms_objectTemplateCompiledExtension);

	request.templateName = lower.substr(gamePos + 15);
	request.templateName.replace(request.templateName.size() - extLen, extLen, ms_objectTemplateCompiledExtension);
	request.server = lower.find("/sys.server/") != std::string::npos;

	//-- the running compile (front) may be of an older save; only skip duplicates still waiting
	for (size_t i = 1; i < m_compileQueue.size(); ++i)
	{
		if (m_compileQueue[i].tpfPath == request.tpfPath)
		{
			report(request.templateName + " is already queued");
			return true;
		}
	}

	m_compileQueue.push_back(request);
	if (m_compileQueue.size() == 1)
		startNextCompile();
	else
		report("queued " + request.templateName + " behind the running compile");

	return true;
}

//-----------------------------------------------------------------------

void ActionsObjectTemplate::startNextCompile()
{
	while (!m_compileQueue.empty())
	{
		CompileRequest& request = m_compileQueue.front();

		QStringList arguments = splitCommandLine(NON_NULL(ConfigGodClient::getData().templateCompileCommand));
		if (arguments.isEmpty())
		{
			report("[GodClient] templateCompileCommand is empty; nothing compiled");
			m_compileQueue.clear();
			return;
		}
		arguments.append(request.tpfPath.c_str());

		makeParentDirectories(request.iffPath);
		request.previousWriteTime = getFileWriteTime(request.iffPath);
		m_compileLine.erase();
		m_compileErrorLines = 0;

		m_compileProcess->setArguments(arguments);
		report("compiling " + request.tpfPath);
		showStatus("Compiling " + request.templateName + "...");

		if (m_compileProcess->start())
		{
			//-- nothing is fed to the compiler; an open stdin pipe could only make it wait
			m_compileProcess->closeStdin();
			IGNORE_RETURN(m_compileTimer->start(cms_compileTimeoutMs, true));
			return;
		}

		report("FAILED to start [" + std::string(arguments.join(" ").latin1()) + "]; check [GodClient] templateCompileCommand");
		showStatus("Template compile failed to start (see Console)");
		m_compileQueue.erase(m_compileQueue.begin());
	}
}

//-----------------------------------------------------------------------

void ActionsObjectTemplate::onCompileOutput()
{
	const QByteArray out = m_compileProcess->readStdout();
	appendCompileOutput(out.data(), static_cast<int>(out.size()), false);

	const QByteArray err = m_compileProcess->readStderr();
	appendCompileOutput(err.data(), static_cast<int>(err.size()), false);
}

//-----------------------------------------------------------------------

/**
 * Echo compiler output to the Console a line at a time. wsl.exe prints its own messages in
 * UTF-16, so NULs are dropped to keep those readable.
 */
void ActionsObjectTemplate::appendCompileOutput(const char* data, int length, bool flush)
{
	for (int i = 0; i <= length; ++i)
	{
		const bool atEnd = (i == length);
		const char c     = atEnd ? '\n' : data[i];

		if (c == '\0' || c == '\r')
			continue;

		if (c != '\n')
		{
			m_compileLine += c;
			continue;
		}

		if (atEnd && !flush)
			break;

		const std::string line = stripAnsi(m_compileLine);
		m_compileLine.erase();
		if (line.empty())
			continue;

		if (looksLikeError(line))
			++m_compileErrorLines;
		MainFrame::getInstance().textToConsole(("  " + line).c_str());
	}
}

//-----------------------------------------------------------------------

void ActionsObjectTemplate::onCompileExited()
{
	m_compileTimer->stop();
	onCompileOutput();
	appendCompileOutput(0, 0, true);

	if (m_compileQueue.empty())
		return;

	const CompileRequest request = m_compileQueue.front();
	m_compileQueue.erase(m_compileQueue.begin());

	const bool   exitedCleanly = m_compileProcess->normalExit() && m_compileProcess->exitStatus() == 0;
	const time_t writeTime     = getFileWriteTime(request.iffPath);
	const bool   written       = writeTime != 0 && writeTime != request.previousWriteTime;

	if (!exitedCleanly || !written)
	{
		char exitText[64];
		if (m_compileProcess->normalExit())
			IGNORE_RETURN(_snprintf(exitText, sizeof(exitText), "exit code %d", m_compileProcess->exitStatus()));
		else
			IGNORE_RETURN(_snprintf(exitText, sizeof(exitText), "killed or crashed"));
		exitText[sizeof(exitText) - 1] = '\0';

		report("compile FAILED for " + request.templateName + " (" + exitText + (written ? "" : std::string(", ") + request.iffPath + " was not written") + "); nothing reloaded");
		showStatus("Template compile FAILED: " + request.templateName + " (see Console)");
	}
	else
	{
		report("compiled " + request.iffPath + (m_compileErrorLines > 0 ? " (the output above mentions errors)" : ""));

		if (!request.server)
			IGNORE_RETURN(reloadClientTemplate(request.templateName, request.iffPath));
		else if (Game::getSinglePlayer())
		{
			report("not connected to a server, so " + request.templateName + " was not reloaded there");
			showStatus("Compiled " + request.templateName + " (no server to reload it)");
		}
		else
		{
			IGNORE_RETURN(ServerCommander::getInstance().serverTemplateReload(request.templateName));
			report("asked the server to reload " + request.templateName + "; its reply follows");
			showStatus("Compiled " + request.templateName + "; server reload requested");
		}
	}

	startNextCompile();
}

//-----------------------------------------------------------------------

void ActionsObjectTemplate::onCompileTimedOut()
{
	if (!m_compileProcess->isRunning())
		return;

	char seconds[16];
	IGNORE_RETURN(_snprintf(seconds, sizeof(seconds), "%d", cms_compileTimeoutMs / 1000));
	seconds[sizeof(seconds) - 1] = '\0';

	report(std::string("the compile is still running after ") + seconds + " s; killing it");
	m_compileProcess->kill();
}

//-----------------------------------------------------------------------

/**
 * Reload a compiled shared template into the running client and rebuild the appearance of the
 * placed objects made from it (or from templates derived from it).
 *
 * The file is read through TreeFile, as a restart would read it. When compiledFile is given and
 * a copy with a higher search priority shadows it, nothing is reloaded: the compile would not
 * show up after a restart either.
 *
 * @return true if the template was reloaded or is not loaded in this client
 */
bool ActionsObjectTemplate::reloadClientTemplate(const std::string& templateName, const std::string& compiledFile)
{
	char resolvedPath[1024];
	if (!TreeFile::getPathName(templateName.c_str(), resolvedPath, static_cast<int>(sizeof(resolvedPath))))
	{
		report(templateName + " is not on this client's search path; nothing reloaded");
		showStatus("Not on the search path: " + templateName);
		return false;
	}

	if (!compiledFile.empty() && normalizePath(resolvedPath) != normalizePath(compiledFile))
	{
		report("NOT reloaded: this client loads " + templateName + " from " + resolvedPath + ", which has a higher search priority than the compiled " + compiledFile + ". Update or remove that copy.");
		showStatus("Shadowed by another copy: " + templateName + " (see Console)");
		return false;
	}

	if (!ObjectTemplateList::isLoaded(templateName))
	{
		report(templateName + " is not loaded in this client; objects created from now on use the new file");
		showStatus("Compiled " + templateName + " (not loaded here, nothing to refresh)");
		return true;
	}

	if (!Iff::isValid(templateName.c_str()))
	{
		report(std::string(resolvedPath) + " is not a valid IFF; not reloaded");
		showStatus("Invalid template file: " + templateName);
		return false;
	}

	const ObjectTemplate* const loadedTemplate = ObjectTemplateList::fetch(templateName);
	if (!loadedTemplate)
	{
		report("could not fetch " + templateName + "; not reloaded");
		return false;
	}

	std::string problem;
	{
		Iff check(templateName.c_str(), true);
		problem = getReloadProblem(check, *loadedTemplate);
	}

	if (!problem.empty())
	{
		loadedTemplate->releaseReference();
		report(templateName + " was NOT reloaded: " + problem + ". Restart the God Client to load it.");
		showStatus("Needs a God Client restart: " + templateName + " (see Console)");
		return false;
	}

	Iff iff(templateName.c_str(), true);
	IGNORE_RETURN(ObjectTemplateList::reload(iff));

	const std::string canonicalName = loadedTemplate->getName();
	std::vector<ClientObject*> placed;
	collectPlacedObjects(placed);

	std::vector<ClientObject*> objects;
	for (std::vector<ClientObject*>::const_iterator it = placed.begin(); it != placed.end(); ++it)
	{
		const ObjectTemplate* const objectTemplate = (*it)->getObjectTemplate();
		if (objectTemplate && objectTemplate->derivesFrom(canonicalName))
			objects.push_back(*it);
	}
	loadedTemplate->releaseReference();

	std::string summary;
	const int refreshed = refreshObjects(objects, summary);

	report("reloaded " + templateName + "; " + summary);
	showStatus("Reloaded " + templateName + ": " + intToString(refreshed) + " object(s) refreshed");
	return true;
}

//-----------------------------------------------------------------------

/**
 * Reload the appearance files of the given placed objects from disk (after a fresh export to
 * D:/exported, say) and rebuild every placed object that shows any of those files.
 *
 * This covers the object template's appearanceFilename (.apt), the .lod or .cmp it redirects to,
 * and their .msh levels and parts. Shaders, textures, floor meshes (.flr) and skeletal assets
 * stay cached.
 *
 * @return true if any placed object was refreshed
 */
bool ActionsObjectTemplate::reloadAppearances(const std::vector<ClientObject*>& objects)
{
	std::set<std::string> fileNames;
	for (std::vector<ClientObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
	{
		if (*it)
			collectObjectAppearanceFiles(**it, fileNames);
	}

	if (fileNames.empty())
	{
		reportAppearances("the selection has no appearance files to reload");
		showStatus("Reload Appearance: no appearance files in the selection");
		return false;
	}

	//-- find everything that shows those files while the cache still knows them
	std::vector<ClientObject*> placed;
	collectPlacedObjects(placed);
	placed.insert(placed.end(), objects.begin(), objects.end());

	std::vector<ClientObject*> affected;
	std::set<ClientObject*> seen;
	for (std::vector<ClientObject*>::const_iterator it = placed.begin(); it != placed.end(); ++it)
	{
		if (!*it || !seen.insert(*it).second)
			continue;

		std::set<std::string> objectFiles;
		collectObjectAppearanceFiles(**it, objectFiles);
		for (std::set<std::string>::const_iterator f = objectFiles.begin(); f != objectFiles.end(); ++f)
		{
			if (fileNames.find(*f) != fileNames.end())
			{
				affected.push_back(*it);
				break;
			}
		}
	}

	//-- name the copy that will load, so a shadowing copy on another search path shows up
	reportAppearances("reloading " + intToString(static_cast<int>(fileNames.size())) + " appearance file(s):");
	int flushed = 0;
	for (std::set<std::string>::const_iterator f = fileNames.begin(); f != fileNames.end(); ++f)
	{
		char resolvedPath[1024];
		const bool onSearchPath = TreeFile::getPathName(f->c_str(), resolvedPath, static_cast<int>(sizeof(resolvedPath)));

		if (AppearanceTemplateList::flush(f->c_str()))
			++flushed;

		MainFrame::getInstance().textToConsole(("  " + *f + (onSearchPath ? std::string(" <- ") + resolvedPath : std::string(" (not on the search path)"))).c_str());
	}

	std::string summary;
	const int refreshed = refreshObjects(affected, summary);

	reportAppearances(intToString(flushed) + " of them were cached; " + summary);
	showStatus("Reloaded appearances: " + intToString(refreshed) + " object(s) refreshed");
	return refreshed > 0;
}

//-----------------------------------------------------------------------

void ActionsObjectTemplate::onReloadAppearance()
{
	GodClientData::ObjectList_t selection;
	GodClientData::getInstance().getSelection(selection);
	if (selection.empty())
	{
		reportAppearances("select the objects whose appearance files changed, then use Reload Appearance");
		showStatus("Reload Appearance: nothing selected");
		return;
	}

	const std::vector<ClientObject*> objects(selection.begin(), selection.end());
	IGNORE_RETURN(reloadAppearances(objects));
}

//END COMPILE AND RELOAD
//-----------------------------------------------------------------------

