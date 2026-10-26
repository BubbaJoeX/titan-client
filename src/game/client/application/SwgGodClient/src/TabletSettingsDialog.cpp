// ======================================================================
//
// TabletSettingsDialog.cpp
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "TabletSettingsDialog.h"
#include "TabletSettingsDialog.moc"

#include <qcheckbox.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qprogressbar.h>
#include <qpushbutton.h>
#include <qslider.h>
#include <qtimer.h>

#include <algorithm>
#include <cmath>

// ======================================================================

namespace TabletSettingsDialogNamespace
{
	int const kPollIntervalMs = 33;

	// The curve slider runs 0..100 and maps to an exponent of 0.25..4 (50 = linear).
	float curveFromSlider(int const value)
	{
		return static_cast<float>(std::pow(2.0, (static_cast<double>(value) - 50.0) / 25.0));
	}

	int sliderFromCurve(float const curve)
	{
		double const value = 50.0 + 25.0 * std::log(static_cast<double>(std::max(0.01f, curve))) / std::log(2.0);
		return std::max(0, std::min(100, static_cast<int>(value + 0.5)));
	}

	QString describeCurve(float const curve)
	{
		QString text;
		if (curve < 0.95f)
			text.sprintf("%.2f (soft)", curve);
		else if (curve > 1.05f)
			text.sprintf("%.2f (firm)", curve);
		else
			text.sprintf("%.2f (linear)", curve);
		return text;
	}
}

using namespace TabletSettingsDialogNamespace;

// ======================================================================

TabletSettingsDialog::TabletSettingsDialog(QWidget * const parent, char const * const name)
: QDialog(parent, name, true),
  m_statusLabel(0),
  m_pressureBar(0),
  m_brushScaleBar(0),
  m_detailLabel(0),
  m_enabledCheck(0),
  m_inkGesturesCheck(0),
  m_barrelNavigationCheck(0),
  m_terrainGroup(0),
  m_pressureStrengthCheck(0),
  m_pressureSizeCheck(0),
  m_minimumScaleSlider(0),
  m_minimumScaleValue(0),
  m_curveSlider(0),
  m_curveValue(0),
  m_eraserCheck(0),
  m_pollTimer(0)
{
	setCaption("Pen Tablet Settings");

	QVBoxLayout * const outer = new QVBoxLayout(this, 10, 8);

	//-- live stylus readout
	QGroupBox * const penGroup = new QGroupBox(this, "penGroup");
	penGroup->setTitle("Stylus");
	penGroup->setColumnLayout(0, Qt::Vertical);
	penGroup->layout()->setSpacing(4);
	penGroup->layout()->setMargin(8);
	QGridLayout * const penGrid = new QGridLayout(penGroup->layout(), 4, 2, 6);

	m_statusLabel = new QLabel(penGroup);
	penGrid->addMultiCellWidget(m_statusLabel, 0, 0, 0, 1);

	penGrid->addWidget(new QLabel("Pressure:", penGroup), 1, 0);
	m_pressureBar = new QProgressBar(100, penGroup);
	penGrid->addWidget(m_pressureBar, 1, 1);

	penGrid->addWidget(new QLabel("Brush gets:", penGroup), 2, 0);
	m_brushScaleBar = new QProgressBar(100, penGroup);
	penGrid->addWidget(m_brushScaleBar, 2, 1);

	m_detailLabel = new QLabel(penGroup);
	penGrid->addMultiCellWidget(m_detailLabel, 3, 3, 0, 1);

	outer->addWidget(penGroup);

	//-- general
	m_enabledCheck = new QCheckBox("Enable pen tablet features", this);
	m_inkGesturesCheck = new QCheckBox("Turn off Windows Ink press-and-hold, flicks and tap ripples in the 3D view", this);
	m_barrelNavigationCheck = new QCheckBox("Barrel button drag moves the camera (orbit; Shift pans; Ctrl zooms)", this);
	outer->addWidget(m_enabledCheck);
	outer->addWidget(m_inkGesturesCheck);
	outer->addWidget(m_barrelNavigationCheck);

	//-- terrain brushes
	m_terrainGroup = new QGroupBox(this, "terrainGroup");
	m_terrainGroup->setTitle("Terrain brushes");
	m_terrainGroup->setColumnLayout(0, Qt::Vertical);
	m_terrainGroup->layout()->setSpacing(4);
	m_terrainGroup->layout()->setMargin(8);
	QGridLayout * const terrainGrid = new QGridLayout(m_terrainGroup->layout(), 5, 3, 6);

	m_pressureStrengthCheck = new QCheckBox("Pen pressure sets brush strength (raise/lower speed, paint, flora)", m_terrainGroup);
	m_pressureSizeCheck = new QCheckBox("Pen pressure sets brush size", m_terrainGroup);
	terrainGrid->addMultiCellWidget(m_pressureStrengthCheck, 0, 0, 0, 2);
	terrainGrid->addMultiCellWidget(m_pressureSizeCheck, 1, 1, 0, 2);

	terrainGrid->addWidget(new QLabel("Lightest touch:", m_terrainGroup), 2, 0);
	m_minimumScaleSlider = new QSlider(0, 100, 5, 10, Qt::Horizontal, m_terrainGroup);
	m_minimumScaleValue = new QLabel(m_terrainGroup);
	m_minimumScaleValue->setMinimumWidth(90);
	terrainGrid->addWidget(m_minimumScaleSlider, 2, 1);
	terrainGrid->addWidget(m_minimumScaleValue, 2, 2);

	terrainGrid->addWidget(new QLabel("Pressure curve:", m_terrainGroup), 3, 0);
	m_curveSlider = new QSlider(0, 100, 5, 50, Qt::Horizontal, m_terrainGroup);
	m_curveValue = new QLabel(m_terrainGroup);
	m_curveValue->setMinimumWidth(90);
	terrainGrid->addWidget(m_curveSlider, 3, 1);
	terrainGrid->addWidget(m_curveValue, 3, 2);

	m_eraserCheck = new QCheckBox("Eraser end (or a pen button set to Eraser) swaps Raise and Lower", m_terrainGroup);
	terrainGrid->addMultiCellWidget(m_eraserCheck, 4, 4, 0, 2);

	outer->addWidget(m_terrainGroup);

	QLabel * const hint = new QLabel(this);
	hint->setText(
		"Pressure, tilt and the eraser come from Windows Ink. In the Huion Tablet app keep\n"
		"Windows Ink on for SwgGodClient_r.exe; with it off the pen still works as a mouse.");
	outer->addWidget(hint);

	//-- buttons
	QHBoxLayout * const buttons = new QHBoxLayout(0, 0, 6);
	QPushButton * const defaultsButton = new QPushButton("Defaults", this);
	QPushButton * const okButton = new QPushButton("OK", this);
	QPushButton * const cancelButton = new QPushButton("Cancel", this);
	okButton->setDefault(true);
	buttons->addWidget(defaultsButton);
	buttons->addStretch(1);
	buttons->addWidget(okButton);
	buttons->addWidget(cancelButton);
	outer->addLayout(buttons);

	loadFrom(GodClientPen::getSettings());

	IGNORE_RETURN(connect(defaultsButton, SIGNAL(clicked()), this, SLOT(onDefaults())));
	IGNORE_RETURN(connect(okButton, SIGNAL(clicked()), this, SLOT(accept())));
	IGNORE_RETURN(connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject())));
	IGNORE_RETURN(connect(m_enabledCheck, SIGNAL(toggled(bool)), this, SLOT(onControlsChanged())));
	IGNORE_RETURN(connect(m_pressureStrengthCheck, SIGNAL(toggled(bool)), this, SLOT(onControlsChanged())));
	IGNORE_RETURN(connect(m_pressureSizeCheck, SIGNAL(toggled(bool)), this, SLOT(onControlsChanged())));
	IGNORE_RETURN(connect(m_minimumScaleSlider, SIGNAL(valueChanged(int)), this, SLOT(onControlsChanged())));
	IGNORE_RETURN(connect(m_curveSlider, SIGNAL(valueChanged(int)), this, SLOT(onControlsChanged())));

	m_pollTimer = new QTimer(this, "tabletPollTimer");
	IGNORE_RETURN(connect(m_pollTimer, SIGNAL(timeout()), this, SLOT(onPollPen())));
	IGNORE_RETURN(m_pollTimer->start(kPollIntervalMs, false));
	onPollPen();
}

// ----------------------------------------------------------------------

TabletSettingsDialog::~TabletSettingsDialog()
{
	m_pollTimer = 0; //-- owned by Qt
}

// ----------------------------------------------------------------------

void TabletSettingsDialog::run(QWidget * const parent)
{
	TabletSettingsDialog dialog(parent, "TabletSettingsDialog");
	if (dialog.exec() == QDialog::Accepted)
		GodClientPen::setSettings(dialog.collect());
}

// ----------------------------------------------------------------------

void TabletSettingsDialog::onDefaults()
{
	loadFrom(GodClientPen::getDefaultSettings());
}

// ----------------------------------------------------------------------

void TabletSettingsDialog::onControlsChanged()
{
	updateControlState();
}

// ----------------------------------------------------------------------

void TabletSettingsDialog::onPollPen()
{
	GodClientPen::State const & state = GodClientPen::getState();

	QString status;
	if (!GodClientPen::isPointerApiAvailable())
		status = "Windows Ink pointer input is not available (needs Windows 8 or later). The pen works as a mouse.";
	else if (!GodClientPen::hasSeenPen())
		status = "No pen seen yet. Hover or touch the pen on any god client window.";
	else
	{
		status = state.inContact ? "Pen touching" : (state.inRange ? "Pen hovering" : "Pen out of range");
		if (state.barrel)
			status += ", barrel button held";
		if (state.eraser)
			status += ", eraser";
	}
	m_statusLabel->setText(status);

	GodClientPen::Settings const settings(collect());
	float const curved = static_cast<float>(std::pow(static_cast<double>(state.pressure), static_cast<double>(settings.pressureCurve)));
	float brushScale = 1.0f;
	if (settings.pressureToStrength || settings.pressureToSize)
		brushScale = state.inContact ? settings.minimumScale + (1.0f - settings.minimumScale) * curved : 0.0f;

	m_pressureBar->setProgress(static_cast<int>(state.pressure * 100.0f + 0.5f));
	m_brushScaleBar->setProgress(static_cast<int>(brushScale * 100.0f + 0.5f));

	QString detail;
	detail.sprintf("Tilt %d / %d degrees, rotation %d degrees, %lu pen updates", state.tiltX, state.tiltY, state.rotation, state.packets);
	m_detailLabel->setText(detail);
}

// ----------------------------------------------------------------------

void TabletSettingsDialog::loadFrom(GodClientPen::Settings const & settings)
{
	m_enabledCheck->setChecked(settings.enabled);
	m_inkGesturesCheck->setChecked(settings.disableInkGestures);
	m_barrelNavigationCheck->setChecked(settings.barrelNavigation);
	m_pressureStrengthCheck->setChecked(settings.pressureToStrength);
	m_pressureSizeCheck->setChecked(settings.pressureToSize);
	m_minimumScaleSlider->setValue(static_cast<int>(settings.minimumScale * 100.0f + 0.5f));
	m_curveSlider->setValue(sliderFromCurve(settings.pressureCurve));
	m_eraserCheck->setChecked(settings.eraserInvertsBrush);
	updateControlState();
}

// ----------------------------------------------------------------------

GodClientPen::Settings TabletSettingsDialog::collect() const
{
	GodClientPen::Settings settings;
	settings.enabled            = m_enabledCheck->isChecked();
	settings.disableInkGestures = m_inkGesturesCheck->isChecked();
	settings.barrelNavigation   = m_barrelNavigationCheck->isChecked();
	settings.pressureToStrength = m_pressureStrengthCheck->isChecked();
	settings.pressureToSize     = m_pressureSizeCheck->isChecked();
	settings.minimumScale       = static_cast<float>(m_minimumScaleSlider->value()) / 100.0f;
	settings.pressureCurve      = curveFromSlider(m_curveSlider->value());
	settings.eraserInvertsBrush = m_eraserCheck->isChecked();
	return settings;
}

// ----------------------------------------------------------------------

void TabletSettingsDialog::updateControlState()
{
	bool const enabled = m_enabledCheck->isChecked();
	bool const pressure = m_pressureStrengthCheck->isChecked() || m_pressureSizeCheck->isChecked();

	m_inkGesturesCheck->setEnabled(enabled);
	m_barrelNavigationCheck->setEnabled(enabled);
	m_terrainGroup->setEnabled(enabled);
	m_minimumScaleSlider->setEnabled(enabled && pressure);
	m_curveSlider->setEnabled(enabled && pressure);

	QString minimumText;
	minimumText.sprintf("%d%%", m_minimumScaleSlider->value());
	m_minimumScaleValue->setText(minimumText);
	m_curveValue->setText(describeCurve(curveFromSlider(m_curveSlider->value())));
}

// ======================================================================
