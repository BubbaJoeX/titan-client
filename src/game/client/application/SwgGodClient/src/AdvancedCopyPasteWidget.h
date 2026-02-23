// ======================================================================
//
// AdvancedCopyPasteWidget.h
// God client dock widget for advanced copy/paste options
//
// ======================================================================

#ifndef INCLUDED_AdvancedCopyPasteWidget_H
#define INCLUDED_AdvancedCopyPasteWidget_H

// ======================================================================

#include <qwidget.h>

// ======================================================================

class QCheckBox;

// ======================================================================

class AdvancedCopyPasteWidget : public QWidget
{
	Q_OBJECT

public:
	explicit AdvancedCopyPasteWidget(QWidget* parent = 0, char const* name = 0);
	virtual ~AdvancedCopyPasteWidget();

	bool getCopyTransform() const;
	bool getCopyScale() const;
	bool getCopyScripts() const;
	bool getCopyObjvars() const;

private slots:
	void onTransformToggled(bool);
	void onScaleToggled(bool);
	void onScriptsToggled(bool);
	void onObjvarsToggled(bool);

private:
	void loadSettings();
	void saveSettings();

	QCheckBox* m_copyTransform;
	QCheckBox* m_copyScale;
	QCheckBox* m_copyScripts;
	QCheckBox* m_copyObjvars;
};

// ======================================================================

#endif
