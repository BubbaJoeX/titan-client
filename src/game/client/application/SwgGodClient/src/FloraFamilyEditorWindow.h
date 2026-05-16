// ======================================================================
//
// FloraFamilyEditorWindow.h
// copyright (c) 2026 Sony Online Entertainment
//
// Live TerrainGenerator flora family inspector (TerrainEditor FormFloraFamily).
//
// ======================================================================

#ifndef INCLUDED_FloraFamilyEditorWindow_H
#define INCLUDED_FloraFamilyEditorWindow_H

// ======================================================================

#include <qdialog.h>

#include <vector>

class QListBox;
class QLabel;
class QLineEdit;
class QListView;
class QPushButton;
class QSpinBox;
class QCheckBox;

// ======================================================================

class FloraFamilyEditorWindow : public QDialog
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762

public:

	static void showSingleton(QWidget* parent);

	explicit FloraFamilyEditorWindow(QWidget* parent = 0, const char* name = 0);
	virtual ~FloraFamilyEditorWindow();

	void reloadFromTerrain();

public slots:

	void onFamilyListSelectionChanged();
	void onApplyEdits();
	void onAddFamily();
	void onRemoveFamily();
	void onAddChildAppearance();
	void onRemoveChild();
	void onClose();

private:

	FloraFamilyEditorWindow(FloraFamilyEditorWindow const&);
	FloraFamilyEditorWindow& operator=(FloraFamilyEditorWindow const&);

	void rebuildFamilyList();
	int selectedFamilyId() const;
	void loadFieldsForFamily(int familyId);
	void rebuildChildList(int familyId);
	void commitFloraGroupToTerrain();

	QListBox*     m_familyList;
	QLabel*       m_idLabel;
	QLineEdit*    m_nameEdit;
	QSpinBox*     m_redSpin;
	QSpinBox*     m_greenSpin;
	QSpinBox*     m_blueSpin;
	QLineEdit*    m_densityEdit;
	QCheckBox*    m_floatsCheck;
	QListView*    m_childList;
	QPushButton*  m_applyButton;
	QPushButton*  m_addFamilyButton;
	QPushButton*  m_removeFamilyButton;
	QPushButton*  m_addChildAppearanceButton;
	QPushButton*  m_removeChildButton;
	QPushButton*  m_closeButton;

	std::vector<int> m_listFamilyIds;
	bool             m_loadingFields;
};

// ======================================================================

#endif
