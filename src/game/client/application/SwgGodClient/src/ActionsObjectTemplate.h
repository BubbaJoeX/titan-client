// =====================================================================
//
// ActionsObjectTemplate.h
// copyright(c) 2001 Sony Online Entertainment
//
// =====================================================================

#ifndef INCLUDED_ActionsObjectTemplate_H
#define INCLUDED_ActionsObjectTemplate_H

// =====================================================================

#include "Singleton/Singleton.h"

#include <ctime>

// =====================================================================

class ActionHack;
class ClientObject;
class QProcess;
class QTimer;

//----------------------------------------------------------------------

/**
* ActionsObjectTemplate is the resting place of various ObjectTemplate-menu related actions
*/
class ActionsObjectTemplate : public QObject, public Singleton<ActionsObjectTemplate>
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762 various deficiencies in the Qt macro

public:

	struct DragMessages
	{
		static const char* const ObjectTemplate_SERVER_TEMPLATE_DRAGGED;
	};

	//lint -save
	//lint -e1925 // public data member
	ActionHack*             m_serverCreate;
	ActionHack*             m_serverView;
	ActionHack*             m_serverEdit;
	ActionHack*             m_serverRevert;
	ActionHack*             m_serverRefresh;
	ActionHack*             m_serverSubmit;
	ActionHack*             m_serverCompile;
	ActionHack*             m_serverReload;
	ActionHack*             m_serverReplace;

	ActionHack*             m_clientCreate;
	ActionHack*             m_clientView;
	ActionHack*             m_clientEdit;
	ActionHack*             m_clientRevert;
	ActionHack*             m_clientRefresh;
	ActionHack*             m_clientSubmit;
	ActionHack*             m_clientCompile;
	ActionHack*             m_clientReplace;
	ActionHack*             m_reloadAppearance;

	ActionHack*				m_serverAddToFavorites;
	ActionHack*				m_clientAddToFavorites;
	//lint -restore

public slots:
	void onServerObjectTemplatePathSelectionChanged(const std::string& path, bool isFile, bool isNew, bool isEdit);
	bool getSelectedServerObjectTemplate           (std::string& path) const;
	void onClientObjectTemplatePathSelectionChanged(const std::string& path, bool isFile, bool isNew, bool isEdit);
	bool getSelectedClientObjectTemplate           (std::string& path) const;

private slots:
	void doEditFile     (const std::string& filename) const;
	void onServerRevert ();
	void onServerView   () const;
	void onServerEdit   ();
	void onServerNew    ();
	void onServerSubmit ();
	void onServerCompile();
	void onServerReload ()  const;
	void onServerReplace();

	void onClientRevert ();
	void onClientView   () const;
	void onClientEdit   ();
	void onClientNew    ();
	void onClientSubmit ();
	void onClientCompile();
	void onClientReplace();
	void onReloadAppearance();

	void onServerAddToFavorites ();
	void onClientAddToFavorites ();

	void onCompileOutput   ();
	void onCompileExited   ();
	void onCompileTimedOut ();

public:
	ActionsObjectTemplate ();
	~ActionsObjectTemplate();

	bool        compileTemplate     (const std::string& tpfPath);
	static bool reloadClientTemplate(const std::string& templateName, const std::string& compiledFile);
	static bool reloadAppearances   (const std::vector<ClientObject*>& objects);

private:
	//disabled
	ActionsObjectTemplate(const ActionsObjectTemplate& rhs);
	ActionsObjectTemplate& operator=(const ActionsObjectTemplate& rhs);

private:
	static const std::string convertToClassPath(const std::string& path);

	const std::string getServerObjectTemplateTpfFilePath() const;
	const std::string getServerObjectTemplateIffFilePath() const;
	const std::string getClientObjectTemplateTpfFilePath() const;
	const std::string getClientObjectTemplateIffFilePath() const;

	void replaceObject(bool client);

	struct CompileRequest
	{
		std::string tpfPath;       // absolute source path
		std::string iffPath;       // absolute path the compiler writes
		std::string templateName;  // TreeFile name, e.g. object/tangible/foo/shared_bar.iff
		bool        server;
		time_t      previousWriteTime;
	};

	void startNextCompile    ();
	void appendCompileOutput (const char* data, int length, bool flush);

	QProcess*                   m_compileProcess;
	QTimer*                     m_compileTimer;
	std::vector<CompileRequest> m_compileQueue; // front() is the one running
	std::string                 m_compileLine;
	int                         m_compileErrorLines;

	std::string   m_selectedServerPath;
	std::string   m_selectedClientPath;
	bool          m_isServerFile;
	bool          m_isServerNew;
	bool          m_isServerEdit;
	bool          m_isClientFile;
	bool          m_isClientNew;
	bool          m_isClientEdit;

	static const char* const ms_objectTemplateSourceExtension;
	static const char* const ms_objectTemplateCompiledExtension;
};

// ======================================================================

#endif

