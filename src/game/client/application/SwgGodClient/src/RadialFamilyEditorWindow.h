// ======================================================================
//
// RadialFamilyEditorWindow.h
// copyright (c) 2026 Sony Online Entertainment
//
// Live TerrainGenerator radial (dynamic flora) family inspector (TerrainEditor FormRadialFamily).
//
// ======================================================================

#ifndef INCLUDED_RadialFamilyEditorWindow_H
#define INCLUDED_RadialFamilyEditorWindow_H

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

class RadialFamilyEditorWindow : public QDialog
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762

public:

	static void showSingleton(QWidget* parent);

	explicit RadialFamilyEditorWindow(QWidget* parent = 0, const char* name = 0);
	virtual ~RadialFamilyEditorWindow();

	void reloadFromTerrain();

public slots:

	void onFamilyListSelectionChanged();
	void onApplyEdits();
	void onClose();

private:

	RadialFamilyEditorWindow(RadialFamilyEditorWindow const&);
	RadialFamilyEditorWindow& operator=(RadialFamilyEditorWindow const&);

	void rebuildFamilyList();
	int selectedFamilyId() const;
	void loadFieldsForFamily(int familyId);
	void rebuildChildList(int familyId);
	void commitRadialGroupToTerrain();

	QListBox*     m_familyList;
	QLabel*       m_idLabel;
	QLabel*       m_nameLabel;
	QSpinBox*     m_redSpin;
	QSpinBox*     m_greenSpin;
	QSpinBox*     m_blueSpin;
	QLineEdit*    m_densityEdit;
	QListView*    m_childList;
	QPushButton*  m_applyButton;
	QPushButton*  m_closeButton;

	std::vector<int> m_listFamilyIds;
	bool             m_loadingFields;
};

// ======================================================================

#endif
