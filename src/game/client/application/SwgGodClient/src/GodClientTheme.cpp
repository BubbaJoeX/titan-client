// ======================================================================
//
// GodClientTheme.cpp
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "GodClientTheme.h"

#include <qapplication.h>
#include <qcolor.h>
#include <qpalette.h>

// ======================================================================

void GodClientTheme::applyOledToApplication(QApplication* const app)
{
	if (!app)
		return;

	QColor const black(0, 0, 0);
	QColor const panel(14, 14, 14);
	QColor const field(8, 8, 8);
	QColor const textBright(235, 235, 235);
	QColor const textDim(200, 200, 200);
	QColor const accent(0, 220, 96);

	QColorGroup active;
	active.setColor(QColorGroup::Foreground, textBright);
	active.setColor(QColorGroup::Background, black);
	active.setColor(QColorGroup::Base, field);
	active.setColor(QColorGroup::Text, textBright);
	active.setColor(QColorGroup::Button, panel);
	active.setColor(QColorGroup::ButtonText, textDim);
	active.setColor(QColorGroup::Highlight, accent);
	active.setColor(QColorGroup::HighlightedText, black);
	active.setColor(QColorGroup::Mid, QColor(42, 42, 42));
	active.setColor(QColorGroup::Light, QColor(72, 72, 72));
	active.setColor(QColorGroup::Dark, QColor(0, 0, 0));

	QPalette pal;
	pal.setActive(active);
	pal.setInactive(active);

	QColorGroup disabled(active);
	disabled.setColor(QColorGroup::Foreground, QColor(100, 100, 100));
	disabled.setColor(QColorGroup::Text, QColor(95, 95, 95));
	disabled.setColor(QColorGroup::ButtonText, QColor(85, 85, 85));
	disabled.setColor(QColorGroup::Highlight, QColor(55, 55, 55));
	disabled.setColor(QColorGroup::HighlightedText, QColor(160, 160, 160));
	pal.setDisabled(disabled);

	app->setPalette(pal, true);
}
