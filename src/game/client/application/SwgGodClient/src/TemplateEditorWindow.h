// ======================================================================
//
// TemplateEditorWindow.h
// copyright 2024 Sony Online Entertainment
//
// Qt window for creating/editing template files (.tpf) and compiling
// with TemplateCompiler. Three panes: sys.server (top-left),
// sys.shared (top-right), string file (bottom-left). Save and Compile
// buttons with double-confirm on save. Supports new template creation
// with type-based boilerplate.
//
// ======================================================================

#ifndef INCLUDED_TemplateEditorWindow_H
#define INCLUDED_TemplateEditorWindow_H

// ======================================================================

#include <qdialog.h>
#include <string>
#include <vector>

class QTextEdit;
class QPushButton;
class QLabel;
class QSplitter;
class QLineEdit;
class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QHideEvent;
class QGridLayout;

class CuiWidget3dObjectListViewer;
class Object;

// ======================================================================

class TemplateEditorWindow : public QDialog
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762

public:

	explicit TemplateEditorWindow(QWidget * parent = 0, const char * name = 0);
	virtual ~TemplateEditorWindow();

	void loadTemplate(const std::string & tpfPath);
	void newTemplate();

signals:
	void statusMessage(const char * msg);

public slots:
	void onSave();
	void onCompile();
	void onBrowseServerPath();
	void onBrowseSharedPath();
	void onBrowseStringPath();
	void onNewTemplate();
	void onApplyStructuredFields();
	void onRefreshStringValues();
	void onAppearanceChanged(const QString & text);
	void onBrowseAppearance();
	void onOpenScriptToolbox();
	void onPreviewAppearance();
	void onAppearanceFilterChanged(const QString & text);

protected:
	virtual void dragEnterEvent(QDragEnterEvent * event);
	virtual void dropEvent(QDropEvent * event);
	virtual void hideEvent(QHideEvent * event);

private:
	TemplateEditorWindow(const TemplateEditorWindow &);
	TemplateEditorWindow & operator=(const TemplateEditorWindow &);

	bool saveToFile(const std::string & path, const std::string & contents);
	bool runTemplateCompiler(const std::string & tpfPath);
	std::string getBoilerplate(const std::string & templateType, const std::string & templateName, bool isShared) const;
	void parseSharedTemplateIntoStructuredFields();
	void applyStructuredFieldsToSharedTemplate();
	void refreshStringValuesFromTool();
	bool setStringValueWithTool(const std::string & file, const std::string & key, const std::string & value, std::string & error);
	bool findStringValueWithTool(const std::string & file, const std::string & key, std::string & outValue, std::string & error);
	bool runLocalizationTool(const std::string & filePath, const std::string & args, std::string & output, int & resultCode) const;
	void addKnownContainerTypes();
	void loadKnownTemplateTypesFromTdf();
	void updateAppearancePreviewText();
	void appendScriptToServerTemplate(const std::string & classPath);
	std::string convertCompiledPathToSource(const std::string & path) const;
	bool parseTemplateIdentityFromPath(const std::string & path, std::string & outType, std::string & outName) const;
	std::string getSharedCounterpartFromServerPath(const std::string & serverPath) const;
	std::string getServerCounterpartFromSharedPath(const std::string & sharedPath) const;
	void showAppearanceViewer(const std::string & appearancePath);
	void hideAppearanceViewer();
	void populateAppearanceFilter(const std::string & filter);

	QTextEdit *   m_serverEditor;
	QTextEdit *   m_sharedEditor;
	QTextEdit *   m_stringEditor;
	QLineEdit *   m_serverPathEdit;
	QLineEdit *   m_sharedPathEdit;
	QLineEdit *   m_stringPathEdit;
	QPushButton * m_saveButton;
	QPushButton * m_compileButton;
	QPushButton * m_newButton;
	QPushButton * m_browseServerButton;
	QPushButton * m_browseSharedButton;
	QPushButton * m_browseStringButton;
	QPushButton * m_applyStructuredButton;
	QPushButton * m_refreshStringsButton;
	QPushButton * m_scriptToolboxButton;
	QPushButton * m_previewButton;
	QComboBox *   m_templateTypeCombo;
	QComboBox *   m_containerTypeCombo;
	QComboBox *   m_appearanceCombo;
	QLineEdit *   m_appearanceFilterEdit;
	QLabel *      m_statusLabel;
	QLabel *      m_appearancePreviewLabel;

	CuiWidget3dObjectListViewer * m_appearanceViewer;
	Object *      m_previewObject;
	bool          m_viewerVisible;

	QLineEdit *   m_objectNameFileEdit;
	QLineEdit *   m_objectNameKeyEdit;
	QLineEdit *   m_objectNameValueEdit;
	QLineEdit *   m_detailedDescFileEdit;
	QLineEdit *   m_detailedDescKeyEdit;
	QLineEdit *   m_detailedDescValueEdit;
	QLineEdit *   m_lookAtFileEdit;
	QLineEdit *   m_lookAtKeyEdit;
	QLineEdit *   m_lookAtValueEdit;

	std::string   m_currentServerPath;
	std::string   m_currentSharedPath;
	std::string   m_currentStringPath;
	bool          m_hasLoadedTemplate;

	std::vector<std::string> m_allAppearances;
};

// ======================================================================

#endif
