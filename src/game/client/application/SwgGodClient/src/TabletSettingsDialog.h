// ======================================================================
//
// TabletSettingsDialog.h
//
// God Client tool: pen tablet settings (pressure, eraser, barrel button,
// Windows Ink gestures) with a live readout of the stylus.
//
// ======================================================================

#ifndef INCLUDED_TabletSettingsDialog_H
#define INCLUDED_TabletSettingsDialog_H

// ======================================================================

#include <qdialog.h>

#include "GodClientPen.h"

class QCheckBox;
class QGroupBox;
class QLabel;
class QProgressBar;
class QSlider;
class QTimer;

// ======================================================================

class TabletSettingsDialog : public QDialog
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762

public:

	explicit TabletSettingsDialog(QWidget * parent = 0, char const * name = 0);
	virtual ~TabletSettingsDialog();

	/// Modal; applies and saves the settings when the user presses OK.
	static void run(QWidget * parent);

private slots:

	void onDefaults();
	void onControlsChanged();
	void onPollPen();

private:

	void                   loadFrom(GodClientPen::Settings const & settings);
	GodClientPen::Settings collect() const;
	void                   updateControlState();

private:

	TabletSettingsDialog(TabletSettingsDialog const &);
	TabletSettingsDialog & operator=(TabletSettingsDialog const &);

private:

	QLabel *       m_statusLabel;
	QProgressBar * m_pressureBar;
	QProgressBar * m_brushScaleBar;
	QLabel *       m_detailLabel;

	QCheckBox *    m_enabledCheck;
	QCheckBox *    m_inkGesturesCheck;
	QCheckBox *    m_barrelNavigationCheck;

	QGroupBox *    m_terrainGroup;
	QCheckBox *    m_pressureStrengthCheck;
	QCheckBox *    m_pressureSizeCheck;
	QSlider *      m_minimumScaleSlider;
	QLabel *       m_minimumScaleValue;
	QSlider *      m_curveSlider;
	QLabel *       m_curveValue;
	QCheckBox *    m_eraserCheck;

	QTimer *       m_pollTimer;
};

// ======================================================================

#endif
