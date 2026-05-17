// ======================================================================
//
// ObjvarEditDialog.cpp
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "ObjvarEditDialog.h"

#include <cstdlib>
#include <vector>

#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qmultilineedit.h>
#include <qpushbutton.h>
#include <qdialog.h>

// ======================================================================

namespace
{
	std::string trim(std::string const & s)
	{
		std::string::size_type b = s.find_first_not_of(" \t\r\n");
		if (b == std::string::npos)
			return std::string();
		std::string::size_type e = s.find_last_not_of(" \t\r\n");
		return s.substr(b, e - b + 1);
	}

	void splitCsv(std::string const & s, char sep, std::vector<std::string> & out)
	{
		out.clear();
		std::string token;
		for (size_t i = 0; i < s.size(); ++i)
		{
			if (s[i] == sep)
			{
				out.push_back(trim(token));
				token.clear();
			}
			else
				token += s[i];
		}
		if (!token.empty() || !s.empty())
			out.push_back(trim(token));
	}

	bool parseLocationDisplay(std::string const & display, std::vector<std::string> & fields)
	{
		fields.clear();
		fields.resize(5);
		std::string s = display;
		std::string::size_type xPos = s.find("x=");
		std::string::size_type yPos = s.find("y=");
		std::string::size_type zPos = s.find("z=");
		std::string::size_type scenePos = s.find("scene=");
		std::string::size_type cellPos = s.find("cell=");
		if (xPos == std::string::npos || yPos == std::string::npos || zPos == std::string::npos)
			return false;

		fields[0] = trim(s.substr(xPos + 2, yPos - (xPos + 2)));
		if (!fields[0].empty() && fields[0][fields[0].size() - 1] == ',')
			fields[0].erase(fields[0].size() - 1);
		fields[1] = trim(s.substr(yPos + 2, zPos - (yPos + 2)));
		if (!fields[1].empty() && fields[1][fields[1].size() - 1] == ',')
			fields[1].erase(fields[1].size() - 1);

		if (scenePos != std::string::npos)
		{
			fields[2] = trim(s.substr(zPos + 2, scenePos - (zPos + 2)));
			if (!fields[2].empty() && fields[2][fields[2].size() - 1] == ',')
				fields[2].erase(fields[2].size() - 1);
			fields[3] = trim(s.substr(scenePos + 6, cellPos != std::string::npos ? cellPos - (scenePos + 6) : std::string::npos));
			if (!fields[3].empty() && fields[3][fields[3].size() - 1] == ',')
				fields[3].erase(fields[3].size() - 1);
		}
		else
		{
			fields[2] = trim(s.substr(zPos + 2));
		}

		if (cellPos != std::string::npos)
		{
			fields[4] = trim(s.substr(cellPos + 5));
			std::string::size_type close = fields[4].find(')');
			if (close != std::string::npos)
				fields[4] = fields[4].substr(0, close);
		}
		return true;
	}

	std::string encodeLocation(std::string const & x, std::string const & y, std::string const & z, std::string const & scene, std::string const & cell)
	{
		return x + "," + y + "," + z + "," + scene + "," + cell;
	}

	QLineEdit * addLabeledField(QWidget * parent, QVBoxLayout * layout, char const * label, std::string const & value)
	{
		IGNORE_RETURN(layout->addWidget(new QLabel(label, parent)));
		QLineEdit * const field = new QLineEdit(parent);
		field->setText(value.c_str());
		IGNORE_RETURN(layout->addWidget(field));
		return field;
	}
}

// ----------------------------------------------------------------------

bool ObjvarEditDialog::isComplexType(std::string const & type)
{
	return type != "int" && type != "float" && type != "string" && type != "networkid";
}

// ----------------------------------------------------------------------

std::string ObjvarEditDialog::inferTypeFromDisplay(std::string const & displayValue)
{
	std::string const & d = displayValue;
	if (d.find("<list>") != std::string::npos)
		return "list";
	if (d.find("(NetworkId)") != std::string::npos)
		return "networkid";
	if (d.find("(x=") != std::string::npos && d.find("scene=") != std::string::npos)
		return "location";
	if (d.find("(Vector)") != std::string::npos)
		return "vector";
	if (d.find("(StringId)") != std::string::npos)
		return "stringid";
	if (d.find("(Transform)") != std::string::npos)
		return "transform";
	if (!d.empty() && d[0] == '[')
	{
		if (d.find("(NetworkId)") != std::string::npos)
			return "networkidarray";
		if (d.find("(x=") != std::string::npos)
			return "locationarray";
		if (d.find("(Vector)") != std::string::npos)
			return "vectorarray";
		if (d.find("(StringId)") != std::string::npos)
			return "stringidarray";
		if (d.find("(Transform)") != std::string::npos)
			return "transformarray";
		if (d.find('.') != std::string::npos)
			return "floatarray";
		return "intarray";
	}
	char * end = 0;
	strtol(d.c_str(), &end, 10);
	if (end && *end == '\0')
		return "int";
	end = 0;
	strtod(d.c_str(), &end);
	if (end && *end == '\0')
		return "float";
	return "string";
}

// ----------------------------------------------------------------------

bool ObjvarEditDialog::run(QWidget * parent, std::string const & type, std::string const & displayValue, std::string & outSetexValue)
{
	outSetexValue.clear();
	if (type == "list")
		return false;

	QDialog dialog(parent, "objvarEdit", true);
	IGNORE_RETURN(dialog.setCaption("Edit Objvar"));

	QVBoxLayout * const layout = new QVBoxLayout(&dialog);
	QLabel * const typeLabel = new QLabel(("Type: " + type).c_str(), &dialog);
	IGNORE_RETURN(layout->addWidget(typeLabel));

	if (type == "location")
	{
		std::vector<std::string> fields;
		if (!parseLocationDisplay(displayValue, fields))
			splitCsv(displayValue, ',', fields);
		while (fields.size() < 5)
			fields.push_back(std::string());

		QLineEdit * const fx = addLabeledField(&dialog, layout, "X", fields[0]);
		QLineEdit * const fy = addLabeledField(&dialog, layout, "Y", fields[1]);
		QLineEdit * const fz = addLabeledField(&dialog, layout, "Z", fields[2]);
		QLineEdit * const fscene = addLabeledField(&dialog, layout, "Scene", fields[3]);
		QLineEdit * const fcell = addLabeledField(&dialog, layout, "Cell (NetworkId)", fields[4]);

		QHBoxLayout * const buttons = new QHBoxLayout(layout);
		QPushButton * const ok = new QPushButton("OK", &dialog);
		QPushButton * const cancel = new QPushButton("Cancel", &dialog);
		IGNORE_RETURN(buttons->addWidget(ok));
		IGNORE_RETURN(buttons->addWidget(cancel));
		ok->setDefault(true);
		cancel->setAutoDefault(false);
		QObject::connect(ok, SIGNAL(clicked()), &dialog, SLOT(accept()));
		QObject::connect(cancel, SIGNAL(clicked()), &dialog, SLOT(reject()));

		if (dialog.exec() != QDialog::Accepted)
			return false;

		outSetexValue = encodeLocation(fx->text().ascii(), fy->text().ascii(), fz->text().ascii(), fscene->text().ascii(), fcell->text().ascii());
		return true;
	}

	if (type == "vector")
	{
		std::string raw = displayValue;
		std::string::size_type p = raw.find('[');
		if (p != std::string::npos)
			raw = raw.substr(p);
		std::vector<std::string> parts;
		splitCsv(raw, ',', parts);
		while (parts.size() < 3)
			parts.push_back("0");

		QLineEdit * const fx = addLabeledField(&dialog, layout, "X", parts[0]);
		QLineEdit * const fy = addLabeledField(&dialog, layout, "Y", parts.size() > 1 ? parts[1] : "0");
		QLineEdit * const fz = addLabeledField(&dialog, layout, "Z", parts.size() > 2 ? parts[2] : "0");

		QHBoxLayout * const buttons = new QHBoxLayout(layout);
		QPushButton * const ok = new QPushButton("OK", &dialog);
		QPushButton * const cancel = new QPushButton("Cancel", &dialog);
		IGNORE_RETURN(buttons->addWidget(ok));
		IGNORE_RETURN(buttons->addWidget(cancel));
		ok->setDefault(true);
		cancel->setAutoDefault(false);
		QObject::connect(ok, SIGNAL(clicked()), &dialog, SLOT(accept()));
		QObject::connect(cancel, SIGNAL(clicked()), &dialog, SLOT(reject()));

		if (dialog.exec() != QDialog::Accepted)
			return false;

		outSetexValue = std::string(fx->text().ascii()) + "," + fy->text().ascii() + "," + fz->text().ascii();
		return true;
	}

	if (type == "stringid")
	{
		std::string table;
		std::string text;
		std::string raw = displayValue;
		std::string::size_type p = raw.find("(StringId)");
		if (p != std::string::npos)
			raw = trim(raw.substr(p + 10));
		std::string::size_type pipe = raw.find('|');
		if (pipe != std::string::npos)
		{
			table = trim(raw.substr(0, pipe));
			text = trim(raw.substr(pipe + 1));
		}
		else
		{
			table = raw;
		}

		QLineEdit * const ftable = addLabeledField(&dialog, layout, "Table", table);
		QLineEdit * const ftext = addLabeledField(&dialog, layout, "Text", text);

		QHBoxLayout * const buttons = new QHBoxLayout(layout);
		QPushButton * const ok = new QPushButton("OK", &dialog);
		QPushButton * const cancel = new QPushButton("Cancel", &dialog);
		IGNORE_RETURN(buttons->addWidget(ok));
		IGNORE_RETURN(buttons->addWidget(cancel));
		ok->setDefault(true);
		cancel->setAutoDefault(false);
		QObject::connect(ok, SIGNAL(clicked()), &dialog, SLOT(accept()));
		QObject::connect(cancel, SIGNAL(clicked()), &dialog, SLOT(reject()));

		if (dialog.exec() != QDialog::Accepted)
			return false;

		outSetexValue = std::string(ftable->text().ascii()) + "|" + ftext->text().ascii();
		return true;
	}

	if (type == "transform")
	{
		QLabel * const hint = new QLabel("Format: px,py,pz,yaw,pitch,roll,sx,sy,sz (angles in radians)", &dialog);
		IGNORE_RETURN(layout->addWidget(hint));
		QLineEdit * const field = addLabeledField(&dialog, layout, "Value", displayValue);

		QHBoxLayout * const buttons = new QHBoxLayout(layout);
		QPushButton * const ok = new QPushButton("OK", &dialog);
		QPushButton * const cancel = new QPushButton("Cancel", &dialog);
		IGNORE_RETURN(buttons->addWidget(ok));
		IGNORE_RETURN(buttons->addWidget(cancel));
		ok->setDefault(true);
		cancel->setAutoDefault(false);
		QObject::connect(ok, SIGNAL(clicked()), &dialog, SLOT(accept()));
		QObject::connect(cancel, SIGNAL(clicked()), &dialog, SLOT(reject()));

		if (dialog.exec() != QDialog::Accepted)
			return false;

		outSetexValue = field->text().ascii();
		return true;
	}

	// Arrays and strings: multiline editor
	{
		QLabel * const hint = new QLabel(
			type.find("array") != std::string::npos
				? "Comma-separated values. location/vector/stringid/transform arrays use ';' between elements."
				: "Value text",
			&dialog);
		IGNORE_RETURN(layout->addWidget(hint));

		QMultiLineEdit * const editor = new QMultiLineEdit(&dialog);
		std::string editText = displayValue;
		if (type == "networkid" && editText.find("(NetworkId)") != std::string::npos)
			editText = trim(editText.substr(editText.find("(NetworkId)") + 11));
		editor->setText(editText.c_str());
		IGNORE_RETURN(layout->addWidget(editor));

		QHBoxLayout * const buttons = new QHBoxLayout(layout);
		QPushButton * const ok = new QPushButton("OK", &dialog);
		QPushButton * const cancel = new QPushButton("Cancel", &dialog);
		IGNORE_RETURN(buttons->addWidget(ok));
		IGNORE_RETURN(buttons->addWidget(cancel));
		ok->setDefault(true);
		cancel->setAutoDefault(false);
		QObject::connect(ok, SIGNAL(clicked()), &dialog, SLOT(accept()));
		QObject::connect(cancel, SIGNAL(clicked()), &dialog, SLOT(reject()));

		if (dialog.exec() != QDialog::Accepted)
			return false;

		outSetexValue = editor->text().ascii();
		// Collapse newlines to commas for simple arrays
		for (size_t i = 0; i < outSetexValue.size(); ++i)
		{
			if (outSetexValue[i] == '\n' || outSetexValue[i] == '\r')
				outSetexValue[i] = ',';
		}
		return true;
	}
}

// ======================================================================
