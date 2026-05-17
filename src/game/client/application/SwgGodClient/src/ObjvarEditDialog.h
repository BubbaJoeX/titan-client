// ======================================================================
//
// ObjvarEditDialog.h
//
// Typed objvar editor for God Client Object Editor.
//
// ======================================================================

#ifndef INCLUDED_ObjvarEditDialog_H
#define INCLUDED_ObjvarEditDialog_H

#include <string>

class QWidget;

class ObjvarEditDialog
{
public:
	// Returns true if user accepted; outSetexValue is the payload for objvar setex.
	static bool run(QWidget * parent, std::string const & type, std::string const & displayValue, std::string & outSetexValue);

	// Infer type string from legacy display-only value text.
	static std::string inferTypeFromDisplay(std::string const & displayValue);

	// True when inline rename is insufficient and the dialog should be used.
	static bool isComplexType(std::string const & type);
};

#endif
