// ======================================================================
//
// ShaderFamilyEditorWindow.h
// copyright (c) 2026 Sony Online Entertainment
//
// Live TerrainGenerator shader family inspector (TerrainEditor FormShaderFamily).
//
// ======================================================================

#ifndef INCLUDED_ShaderFamilyEditorWindow_H
#define INCLUDED_ShaderFamilyEditorWindow_H

// ======================================================================

#include <qdialog.h>

#include <vector>

class QListBox;
class QLabel;
class QLineEdit;
class QListView;
class QPushButton;
class QSpinBox;

// ======================================================================

class ShaderFamilyEditorWindow : public QDialog
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762

public:

	static void showSingleton(QWidget* parent);

	explicit ShaderFamilyEditorWindow(QWidget* parent = 0, const char* name = 0);
	virtual ~ShaderFamilyEditorWindow();

	void reloadFromTerrain();

public slots:

	void onFamilyListSelectionChanged();
	void onApplyEdits();
	void onAddFamily();
	void onRemoveFamily();
	void onAddChildShader();
	void onRemoveChild();
	void onClose();

private:

	ShaderFamilyEditorWindow(ShaderFamilyEditorWindow const&);
	ShaderFamilyEditorWindow& operator=(ShaderFamilyEditorWindow const&);

	void rebuildFamilyList();
	int selectedFamilyId() const;
	void loadFieldsForFamily(int familyId);
	void rebuildChildList(int familyId);
	void commitShaderGroupToTerrain();

	QListBox*     m_familyList;
	QLabel*       m_idLabel;
	QLineEdit*    m_nameEdit;
	QSpinBox*     m_redSpin;
	QSpinBox*     m_greenSpin;
	QSpinBox*     m_blueSpin;
	QLineEdit*    m_surfacePropertiesEdit;
	QLineEdit*    m_featherClampEdit;
	QListView*    m_childList;
	QPushButton*  m_applyButton;
	QPushButton*  m_addFamilyButton;
	QPushButton*  m_removeFamilyButton;
	QPushButton*  m_addChildShaderButton;
	QPushButton*  m_removeChildButton;
	QPushButton*  m_closeButton;

	std::vector<int> m_listFamilyIds;
	bool             m_loadingFields;
};

// ======================================================================

#endif
