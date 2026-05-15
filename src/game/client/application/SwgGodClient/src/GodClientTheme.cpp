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

	// ------------------------------------------------------------------
	// Core Theme Colors
	// ------------------------------------------------------------------

	// Main application background
	// Hex: #0A0A0A
	// Description: Deep OLED black for maximum contrast
	QColor const background(10, 10, 10);

	// Primary panel/button background
	// Hex: #181818
	// Description: Soft charcoal gray for UI surfaces
	QColor const panel(24, 24, 24);

	// Input fields / list backgrounds
	// Hex: #121212
	// Description: Slightly darker neutral surface layer
	QColor const surface(18, 18, 18);

	// Borders / separators / frame shadows
	// Hex: #2A2A2A
	// Description: Professional dark divider tone
	QColor const border(42, 42, 42);

	// Primary readable text
	// Hex: #E6E6E6
	// Description: Soft white optimized for dark themes
	QColor const textPrimary(230, 230, 230);

	// Secondary text / button labels
	// Hex: #B9B9B9
	// Description: Muted silver-gray for lower emphasis text
	QColor const textSecondary(185, 185, 185);

	// Disabled UI text
	// Hex: #6E6E6E
	// Description: Neutral gray indicating inactive state
	QColor const textDisabled(110, 110, 110);

	// Primary accent color
	// Hex: #3A82F6
	// Description: Modern professional blue highlight
	QColor const accent(58, 130, 246);

	// Hover / brighter accent variant
	// Hex: #4C95FF
	// Description: Elevated interactive blue tone
	QColor const accentHover(76, 149, 255);

	// ------------------------------------------------------------------
	// Active Color Group
	// ------------------------------------------------------------------

	QColorGroup active;

	// General foreground text/icons
	active.setColor(QColorGroup::Foreground, textPrimary);

	// Main application background
	active.setColor(QColorGroup::Background, background);

	// Text entry fields / editors
	active.setColor(QColorGroup::Base, surface);

	// Editable text color
	active.setColor(QColorGroup::Text, textPrimary);

	// Button and panel surface color
	active.setColor(QColorGroup::Button, panel);

	// Button label color
	active.setColor(QColorGroup::ButtonText, textSecondary);

	// Selection highlight color
	active.setColor(QColorGroup::Highlight, accent);

	// Selected text color
	active.setColor(QColorGroup::HighlightedText, QColor(255, 255, 255));

	// Mid-tone borders/shadows
	active.setColor(QColorGroup::Mid, border);

	// Raised edge highlights
	active.setColor(QColorGroup::Light, QColor(58, 58, 58));

	// Deep shadows
	active.setColor(QColorGroup::Dark, QColor(6, 6, 6));

	// ------------------------------------------------------------------
	// Palette Setup
	// ------------------------------------------------------------------

	QPalette pal;

	// Active window palette
	pal.setActive(active);

	// Inactive window palette
	pal.setInactive(active);

	// ------------------------------------------------------------------
	// Disabled State Colors
	// ------------------------------------------------------------------

	QColorGroup disabled(active);

	// Disabled foreground elements
	disabled.setColor(QColorGroup::Foreground, textDisabled);

	// Disabled text
	disabled.setColor(QColorGroup::Text, textDisabled);

	// Disabled button labels
	disabled.setColor(QColorGroup::ButtonText, QColor(95, 95, 95));

	// Disabled selection highlight
	disabled.setColor(QColorGroup::Highlight, QColor(50, 50, 50));

	// Disabled selected text
	disabled.setColor(QColorGroup::HighlightedText, QColor(140, 140, 140));

	pal.setDisabled(disabled);

	// ------------------------------------------------------------------
	// Apply Palette To Entire Application
	// ------------------------------------------------------------------

	app->setPalette(pal, true);
}