// ======================================================================
//
// GodClientTheme.h
// OLED-inspired high-contrast palette for the God Client (Qt 3).
//
// ======================================================================

#ifndef INCLUDED_GodClientTheme_H
#define INCLUDED_GodClientTheme_H

class QApplication;

namespace GodClientTheme
{
	/// Applies near-black surfaces, light text, and a green accent to standard widgets via QPalette.
	void applyOledToApplication(QApplication* app);
}

#endif
