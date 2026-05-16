// ======================================================================
//
// BitmapFamilyEditorWindow.h
// copyright (c) 2026 Sony Online Entertainment
//
// Live TerrainGenerator bitmap stamp family inspector (TerrainEditor FormBitmapFamily).
//
// ======================================================================

#ifndef INCLUDED_BitmapFamilyEditorWindow_H
#define INCLUDED_BitmapFamilyEditorWindow_H

// ======================================================================

#include <qdialog.h>

#include <vector>

class QListBox;
class QLabel;
class QLineEdit;
class QPushButton;

// ======================================================================

class BitmapFamilyEditorWindow : public QDialog
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762

public:

	static void showSingleton(QWidget* parent);

	explicit BitmapFamilyEditorWindow(QWidget* parent = 0, const char* name = 0);
	virtual ~BitmapFamilyEditorWindow();

	void reloadFromTerrain();

public slots:

	void onFamilyListSelectionChanged();
	void onApplyEdits();
	void onAddFamily();
	void onRemoveFamily();
	void onBrowseTgaBasename();
	void onReloadTgaClicked();
	void onClose();

private:

	BitmapFamilyEditorWindow(BitmapFamilyEditorWindow const&);
	BitmapFamilyEditorWindow& operator=(BitmapFamilyEditorWindow const&);

	void rebuildFamilyList();
	int selectedFamilyId() const;
	void loadFieldsForFamily(int familyId);
	void commitBitmapGroupToTerrain(int familyId);

	QListBox*     m_familyList;
	QLabel*       m_idLabel;
	QLineEdit*    m_displayNameEdit;
	QLineEdit*    m_tgaBasenameEdit;
	QLabel*       m_hintLabel;
	QPushButton*  m_addFamilyButton;
	QPushButton*  m_removeFamilyButton;
	QPushButton*  m_browseTgaBasenameButton;
	QPushButton*  m_reloadTgaButton;
	QPushButton*  m_applyButton;
	QPushButton*  m_closeButton;

	std::vector<int> m_listFamilyIds;
	bool             m_loadingFields;
};

// ======================================================================

#endif
