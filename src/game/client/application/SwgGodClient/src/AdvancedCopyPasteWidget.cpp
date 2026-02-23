// ======================================================================
//
// AdvancedCopyPasteWidget.cpp
// God client dock widget for advanced copy/paste options
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "AdvancedCopyPasteWidget.h"
#include "AdvancedCopyPasteWidget.moc"

#include "GodClientData.h"

#include <qcheckbox.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qgroupbox.h>
#include <qsettings.h>

// ======================================================================

namespace
{
	char const* const s_settingsGroup = "AdvancedCopyPaste";
	char const* const s_copyTransformKey = "copyTransform";
	char const* const s_copyScaleKey = "copyScale";
	char const* const s_copyScriptsKey = "copyScripts";
	char const* const s_copyObjvarsKey = "copyObjvars";
}

// ======================================================================

AdvancedCopyPasteWidget::AdvancedCopyPasteWidget(QWidget* parent, char const* name)
: QWidget(parent, name),
  m_copyTransform(0),
  m_copyScale(0),
  m_copyScripts(0),
  m_copyObjvars(0)
{
	QVBoxLayout* layout = new QVBoxLayout(this, 4, 4);

	QGroupBox* group = new QGroupBox("Copy options", this);
	QVBoxLayout* groupLayout = new QVBoxLayout(group, 4, 4);

	m_copyTransform = new QCheckBox("Transform (position, rotation)", group);
	m_copyTransform->setChecked(true);
	IGNORE_RETURN(connect(m_copyTransform, SIGNAL(toggled(bool)), this, SLOT(onTransformToggled(bool))));
	groupLayout->addWidget(m_copyTransform);

	m_copyScale = new QCheckBox("Scale", group);
	m_copyScale->setChecked(false);
	IGNORE_RETURN(connect(m_copyScale, SIGNAL(toggled(bool)), this, SLOT(onScaleToggled(bool))));
	groupLayout->addWidget(m_copyScale);

	m_copyScripts = new QCheckBox("Scripts", group);
	m_copyScripts->setChecked(false);
	IGNORE_RETURN(connect(m_copyScripts, SIGNAL(toggled(bool)), this, SLOT(onScriptsToggled(bool))));
	groupLayout->addWidget(m_copyScripts);

	m_copyObjvars = new QCheckBox("ObjVars", group);
	m_copyObjvars->setChecked(false);
	IGNORE_RETURN(connect(m_copyObjvars, SIGNAL(toggled(bool)), this, SLOT(onObjvarsToggled(bool))));
	groupLayout->addWidget(m_copyObjvars);

	layout->addWidget(group);
	layout->addStretch();

	loadSettings();
	GodClientData::getInstance().setCopyTransform(getCopyTransform());
	GodClientData::getInstance().setCopyScale(getCopyScale());
	GodClientData::getInstance().setCopyScripts(getCopyScripts());
	GodClientData::getInstance().setCopyObjvars(getCopyObjvars());
}

// ----------------------------------------------------------------------

AdvancedCopyPasteWidget::~AdvancedCopyPasteWidget()
{
	saveSettings();
}

// ----------------------------------------------------------------------

bool AdvancedCopyPasteWidget::getCopyTransform() const
{
	return m_copyTransform && m_copyTransform->isChecked();
}

// ----------------------------------------------------------------------

bool AdvancedCopyPasteWidget::getCopyScale() const
{
	return m_copyScale && m_copyScale->isChecked();
}

// ----------------------------------------------------------------------

bool AdvancedCopyPasteWidget::getCopyScripts() const
{
	return m_copyScripts && m_copyScripts->isChecked();
}

// ----------------------------------------------------------------------

bool AdvancedCopyPasteWidget::getCopyObjvars() const
{
	return m_copyObjvars && m_copyObjvars->isChecked();
}

// ----------------------------------------------------------------------

void AdvancedCopyPasteWidget::onTransformToggled(bool)
{
	saveSettings();
	GodClientData::getInstance().setCopyTransform(getCopyTransform());
}

// ----------------------------------------------------------------------

void AdvancedCopyPasteWidget::onScaleToggled(bool)
{
	saveSettings();
	GodClientData::getInstance().setCopyScale(getCopyScale());
}

// ----------------------------------------------------------------------

void AdvancedCopyPasteWidget::onScriptsToggled(bool)
{
	saveSettings();
	GodClientData::getInstance().setCopyScripts(getCopyScripts());
}

// ----------------------------------------------------------------------

void AdvancedCopyPasteWidget::onObjvarsToggled(bool)
{
	saveSettings();
	GodClientData::getInstance().setCopyObjvars(getCopyObjvars());
}

// ----------------------------------------------------------------------

void AdvancedCopyPasteWidget::loadSettings()
{
	QSettings settings;
	settings.beginGroup(s_settingsGroup);
	m_copyTransform->setChecked(settings.readBoolEntry(s_copyTransformKey, true));
	m_copyScale->setChecked(settings.readBoolEntry(s_copyScaleKey, false));
	m_copyScripts->setChecked(settings.readBoolEntry(s_copyScriptsKey, false));
	m_copyObjvars->setChecked(settings.readBoolEntry(s_copyObjvarsKey, false));
	settings.endGroup();
}

// ----------------------------------------------------------------------

void AdvancedCopyPasteWidget::saveSettings()
{
	QSettings settings;
	settings.beginGroup(s_settingsGroup);
	settings.writeEntry(s_copyTransformKey, m_copyTransform->isChecked());
	settings.writeEntry(s_copyScaleKey, m_copyScale->isChecked());
	settings.writeEntry(s_copyScriptsKey, m_copyScripts->isChecked());
	settings.writeEntry(s_copyObjvarsKey, m_copyObjvars->isChecked());
	settings.endGroup();
}

// ======================================================================
