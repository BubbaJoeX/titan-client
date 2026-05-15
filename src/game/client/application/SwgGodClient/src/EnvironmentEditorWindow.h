// ======================================================================
//
// EnvironmentEditorWindow.h
// copyright (c) 2026 Sony Online Entertainment
//
// Pop-out editor for TerrainGenerator environment families (names,
// preview colors, feather clamps). Mutates the live generator group
// used by the current procedural terrain.
//
// ======================================================================

#ifndef INCLUDED_EnvironmentEditorWindow_H
#define INCLUDED_EnvironmentEditorWindow_H

// ======================================================================

#include <qdialog.h>

#include <vector>

class QListBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QSpinBox;

// ======================================================================

class EnvironmentEditorWindow : public QDialog
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762

public:

	static void showSingleton(QWidget* parent);

	explicit EnvironmentEditorWindow(QWidget* parent = 0, const char* name = 0);
	virtual ~EnvironmentEditorWindow();

	void reloadFromTerrain();

public slots:

	void onFamilyListSelectionChanged();
	void onApplyEdits();
	void onAddFamily();
	void onRemoveFamily();
	void onClose();

private:

	EnvironmentEditorWindow(EnvironmentEditorWindow const&);
	EnvironmentEditorWindow& operator=(EnvironmentEditorWindow const&);

	void rebuildFamilyList();
	int selectedFamilyId() const;
	void loadFieldsForFamily(int familyId);
	void commitEnvironmentGroupToTerrain();

	QListBox*     m_familyList;
	QLabel*       m_idLabel;
	QLineEdit*    m_nameEdit;
	QSpinBox*     m_redSpin;
	QSpinBox*     m_greenSpin;
	QSpinBox*     m_blueSpin;
	QLineEdit*    m_featherClampEdit;
	QPushButton*  m_applyButton;
	QPushButton*  m_addButton;
	QPushButton*  m_removeButton;
	QPushButton*  m_closeButton;

	std::vector<int> m_listFamilyIds;
	bool             m_loadingFields;
};

// ======================================================================

#endif
