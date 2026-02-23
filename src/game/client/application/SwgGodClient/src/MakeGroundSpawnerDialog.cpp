// ======================================================================
//
// MakeGroundSpawnerDialog.cpp
// God client dialog for creating ground spawners
//
// Qt3 CONSTRAINT: Create/close flow must run synchronously in the button slot.
// Do NOT use QTimer::singleShot, QMetaObject::invokeMethod(QueuedConnection),
// or any deferred execution for hide()/accept() - causes 0xc0000005 crash.
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "MakeGroundSpawnerDialog.h"
#include "MakeGroundSpawnerDialog.moc"

#include "ConfigGodClient.h"
#include "GodClientData.h"
#include "ServerCommander.h"

#include <qstring.h>
#include <qapplication.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qtimer.h>

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// ======================================================================

namespace
{
	std::string qstringToStdString(QString const& qstr)
	{
		QCString const cstr = qstr.utf8();
		return cstr.data() ? std::string(cstr.data()) : std::string();
	}

	void parseTabLine(std::string const& line, std::vector<std::string>& tokens)
	{
		tokens.clear();
		std::string token;
		for (size_t i = 0; i <= line.size(); ++i)
		{
			if (i == line.size() || line[i] == '\t')
			{
				if (!token.empty() || i < line.size())
					tokens.push_back(token);
				token.clear();
			}
			else
				token += line[i];
		}
	}

#ifdef _WIN32
	void collectTabTypes(std::string const& dirPath, std::set<std::string>& out)
	{
		WIN32_FIND_DATA data;
		std::string spec = dirPath + "/*";
		HANDLE h = FindFirstFile(spec.c_str(), &data);
		if (h == INVALID_HANDLE_VALUE)
			return;
		do
		{
			if (data.cFileName[0] == '.')
				continue;
			std::string name(data.cFileName);
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				collectTabTypes(dirPath + "/" + name, out);
			}
			else if (name.size() > 4 && name.compare(name.size() - 4, 4, ".tab") == 0)
			{
				out.insert(name.substr(0, name.size() - 4));
			}
		}
		while (FindNextFile(h, &data));
		FindClose(h);
	}
#endif
}

// ======================================================================

MakeGroundSpawnerDialog::MakeGroundSpawnerDialog(QWidget* parent, char const* name)
: QDialog(parent, name),
  m_mobCombo(0),
  m_nameEdit(0),
  m_typeCombo(0),
  m_locationTypeEdit(0),
  m_radiusEdit(0),
  m_spawnCountSpin(0),
  m_behaviorCombo(0),
  m_useGoodLocationCheck(0),
  m_minSpawnTimeEdit(0),
  m_maxSpawnTimeEdit(0),
  m_patrolPathCombo(0),
	m_placeWaypointsButton(0),
	m_waypointCountLabel(0),
	m_pendingCreate()
{
	m_pendingCreate.isPatrol = false;
	setCaption("Make Ground Spawner");
	setMinimumWidth(320);
	setModal(false);

	QGridLayout* layout = new QGridLayout(this, 12, 2, 6, 6);

	int row = 0;

	layout->addWidget(new QLabel("Spawner Name:", this), row, 0);
	m_nameEdit = new QLineEdit(this);
	m_nameEdit->setText("ground_spawner");
	m_nameEdit->setMaxLength(64);
	layout->addWidget(m_nameEdit, row, 1);
	++row;

	layout->addWidget(new QLabel("Mob/Creature:", this), row, 0);
	m_mobCombo = new QComboBox(this);
	m_mobCombo->setEditable(true);
	m_mobCombo->setMinimumWidth(180);
	layout->addWidget(m_mobCombo, row, 1);
	++row;

	layout->addWidget(new QLabel("Type:", this), row, 0);
	m_typeCombo = new QComboBox(this);
	m_typeCombo->insertItem("area");
	m_typeCombo->insertItem("location");
	m_typeCombo->insertItem("patrol");
	layout->addWidget(m_typeCombo, row, 1);
	++row;

	layout->addWidget(new QLabel("Patrol Path (patrol type):", this), row, 0);
	m_patrolPathCombo = new QComboBox(this);
	m_patrolPathCombo->insertItem("cycle");
	m_patrolPathCombo->insertItem("oscillate");
	m_patrolPathCombo->setEnabled(false);
	layout->addWidget(m_patrolPathCombo, row, 1);
	++row;

	layout->addWidget(new QLabel("Location File (location type):", this), row, 0);
	m_locationTypeEdit = new QLineEdit(this);
	m_locationTypeEdit->setEnabled(false);
	layout->addWidget(m_locationTypeEdit, row, 1);
	++row;

	layout->addWidget(new QLabel("Radius (m):", this), row, 0);
	m_radiusEdit = new QLineEdit(this);
	m_radiusEdit->setText("32.0");
	m_radiusEdit->setMaxLength(16);
	layout->addWidget(m_radiusEdit, row, 1);
	++row;

	layout->addWidget(new QLabel("Spawn Count:", this), row, 0);
	m_spawnCountSpin = new QSpinBox(this);
	m_spawnCountSpin->setRange(1, 100);
	m_spawnCountSpin->setValue(5);
	layout->addWidget(m_spawnCountSpin, row, 1);
	++row;

	layout->addWidget(new QLabel("Behavior:", this), row, 0);
	m_behaviorCombo = new QComboBox(this);
	m_behaviorCombo->insertItem("wander");
	m_behaviorCombo->insertItem("sentinel");
	m_behaviorCombo->insertItem("loiter");
	m_behaviorCombo->insertItem("stop");
	layout->addWidget(m_behaviorCombo, row, 1);
	++row;

	layout->addWidget(new QLabel("Use getGoodLocation:", this), row, 0);
	m_useGoodLocationCheck = new QCheckBox(this);
	m_useGoodLocationCheck->setChecked(false);
	layout->addWidget(m_useGoodLocationCheck, row, 1);
	++row;

	layout->addWidget(new QLabel("Min Spawn Time (s):", this), row, 0);
	m_minSpawnTimeEdit = new QLineEdit(this);
	m_minSpawnTimeEdit->setText("60.0");
	m_minSpawnTimeEdit->setMaxLength(16);
	layout->addWidget(m_minSpawnTimeEdit, row, 1);
	++row;

	layout->addWidget(new QLabel("Max Spawn Time (s):", this), row, 0);
	m_maxSpawnTimeEdit = new QLineEdit(this);
	m_maxSpawnTimeEdit->setText("120.0");
	m_maxSpawnTimeEdit->setMaxLength(16);
	layout->addWidget(m_maxSpawnTimeEdit, row, 1);
	++row;

	m_placeWaypointsButton = new QPushButton("Place Waypoints", this);
	m_placeWaypointsButton->setEnabled(false);
	connect(m_placeWaypointsButton, SIGNAL(clicked()), this, SLOT(onPlaceWaypoints()));
	layout->addWidget(m_placeWaypointsButton, row, 0);

	m_waypointCountLabel = new QLabel("0 waypoints", this);
	layout->addWidget(m_waypointCountLabel, row, 1);
	++row;

	QPushButton* okButton = new QPushButton("Create", this);
	okButton->setAutoDefault(false);
	okButton->setDefault(false);
	connect(okButton, SIGNAL(clicked()), this, SLOT(onAccept()));
	layout->addWidget(okButton, row, 0);

	QPushButton* cancelButton = new QPushButton("Cancel", this);
	connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));
	layout->addWidget(cancelButton, row, 1);

	connect(m_typeCombo, SIGNAL(activated(int)), this, SLOT(onTypeChanged(int)));
	if (m_patrolPathCombo)
		connect(m_patrolPathCombo, SIGNAL(activated(int)), this, SLOT(onPatrolPathChanged()));
	onTypeChanged(0);

	QTimer* updateTimer = new QTimer(this);
	connect(updateTimer, SIGNAL(timeout()), this, SLOT(onPatrolWaypointChanged()));
	updateTimer->start(250);

	loadCreatureList();
}

// ----------------------------------------------------------------------

MakeGroundSpawnerDialog::~MakeGroundSpawnerDialog()
{
	GodClientData::getInstance().clearPatrolWaypoints();
	GodClientData::getInstance().setPatrolWaypointPlacementMode(false);
}

// ----------------------------------------------------------------------

// ----------------------------------------------------------------------

void MakeGroundSpawnerDialog::onTypeChanged(int index)
{
	bool const isLocation = (index == 1);
	bool const isPatrol = (index == 2);
	if (m_locationTypeEdit)
		m_locationTypeEdit->setEnabled(isLocation);
	if (m_patrolPathCombo)
		m_patrolPathCombo->setEnabled(isPatrol);
	if (m_placeWaypointsButton)
		m_placeWaypointsButton->setEnabled(isPatrol);
	if (m_behaviorCombo)
		m_behaviorCombo->setEnabled(!isPatrol);
	if (m_useGoodLocationCheck)
		m_useGoodLocationCheck->setEnabled(!isPatrol);
	if (isPatrol && m_patrolPathCombo)
		GodClientData::getInstance().setPatrolPathType(qstringToStdString(m_patrolPathCombo->currentText()));
	else
		GodClientData::getInstance().setPatrolPathType("cycle");
	if (!isPatrol)
		GodClientData::getInstance().setPatrolWaypointPlacementMode(false);
	onPatrolWaypointChanged();
}

// ----------------------------------------------------------------------

void MakeGroundSpawnerDialog::onPlaceWaypoints()
{
	bool const inMode = GodClientData::getInstance().getPatrolWaypointPlacementMode();
	GodClientData::getInstance().setPatrolWaypointPlacementMode(!inMode);
	if (!inMode)
	{
		m_placeWaypointsButton->setText("Finish Placing");
		QMessageBox::information(this, "Place Waypoints", "Click in the world to add patrol waypoints. Spheres will appear with numbers. Click Finish Placing when done.");
	}
	else
		m_placeWaypointsButton->setText("Place Waypoints");
	onPatrolWaypointChanged();
}

// ----------------------------------------------------------------------

void MakeGroundSpawnerDialog::onPatrolWaypointChanged()
{
	size_t const n = GodClientData::getInstance().getPatrolWaypoints().size();
	if (m_waypointCountLabel)
	{
		char buf[64];
		sprintf(buf, "%u waypoint(s)", static_cast<unsigned>(n));
		m_waypointCountLabel->setText(buf);
	}
}

// ----------------------------------------------------------------------

void MakeGroundSpawnerDialog::onPatrolPathChanged()
{
	if (m_patrolPathCombo)
		GodClientData::getInstance().setPatrolPathType(qstringToStdString(m_patrolPathCombo->currentText()));
}

// ----------------------------------------------------------------------

void MakeGroundSpawnerDialog::loadCreatureList()
{
	NOT_NULL(m_mobCombo);
	m_mobCombo->clear();

	std::set<std::string> allNames;

	// Load creatures from creatures.tab
	{
		std::string path;
		path += ConfigGodClient::getServerDataTableTheaterDirectory();
		path += "/datatables/mob/creatures.tab";

		std::ifstream file(path.c_str());
		if (file.is_open())
		{
			std::string line;
			std::vector<std::string> tokens;
			int creatureNameCol = 0;

			if (std::getline(file, line))
			{
				parseTabLine(line, tokens);
				for (size_t i = 0; i < tokens.size(); ++i)
				{
					if (tokens[i] == "creatureName")
					{
						creatureNameCol = static_cast<int>(i);
						break;
					}
				}
			}
			std::getline(file, line);  // Skip type descriptor row

			while (std::getline(file, line))
			{
				parseTabLine(line, tokens);
				if (creatureNameCol < static_cast<int>(tokens.size()) && !tokens[creatureNameCol].empty())
				{
					QString const creatureName = tokens[creatureNameCol].c_str();
					if (!creatureName.startsWith("s[") && !creatureName.startsWith("i["))
						allNames.insert(tokens[creatureNameCol]);
				}
			}
		}
	}

#ifdef _WIN32
	// Load spawn types from ground_spawning/types/*.tab
	{
		std::string typesPath;
		typesPath += ConfigGodClient::getServerDataTableTheaterDirectory();
		typesPath += "/datatables/spawning/ground_spawning/types";
		collectTabTypes(typesPath, allNames);
	}
#endif

	if (allNames.empty())
	{
		m_mobCombo->insertItem("(load creatures.tab and types from dsrc)");
		return;
	}

	for (std::set<std::string>::const_iterator it = allNames.begin(); it != allNames.end(); ++it)
		m_mobCombo->insertItem(it->c_str());
}

// ----------------------------------------------------------------------

void MakeGroundSpawnerDialog::onAccept()
{
	QString const name = m_nameEdit->text().stripWhiteSpace();
	if (name.isEmpty())
	{
		QMessageBox::warning(this, "Make Ground Spawner", "Please enter a spawner name.");
		return;
	}
	QString const spawns = m_mobCombo->currentText().stripWhiteSpace();
	if (spawns.isEmpty())
	{
		QMessageBox::warning(this, "Make Ground Spawner", "Please enter or select a mob/creature.");
		return;
	}
	if (name.find(' ') >= 0)
	{
		QMessageBox::warning(this, "Make Ground Spawner", "Spawner name cannot contain spaces (use underscores).");
		return;
	}

	QString locationType;
	if (m_typeCombo->currentItem() == 1)
	{
		locationType = m_locationTypeEdit->text().stripWhiteSpace();
		if (locationType.isEmpty())
		{
			QMessageBox::warning(this, "Make Ground Spawner", "Location type requires a location file (buildout file name).");
			return;
		}
	}

	float radius = static_cast<float>(atof(m_radiusEdit->text().stripWhiteSpace().latin1()));
	if (radius < 0.1f || radius > 1000.0f)
	{
		QMessageBox::warning(this, "Make Ground Spawner", "Radius must be between 0.1 and 1000.0.");
		return;
	}

	float minSpawnTime = static_cast<float>(atof(m_minSpawnTimeEdit->text().stripWhiteSpace().latin1()));
	float maxSpawnTime = static_cast<float>(atof(m_maxSpawnTimeEdit->text().stripWhiteSpace().latin1()));
	if (minSpawnTime < 0.0f || minSpawnTime > 3600.0f)
	{
		QMessageBox::warning(this, "Make Ground Spawner", "Min spawn time must be between 0 and 3600.");
		return;
	}
	if (maxSpawnTime < 0.0f || maxSpawnTime > 3600.0f)
	{
		QMessageBox::warning(this, "Make Ground Spawner", "Max spawn time must be between 0 and 3600.");
		return;
	}
	if (maxSpawnTime < minSpawnTime)
	{
		QMessageBox::warning(this, "Make Ground Spawner", "Max spawn time must be greater than or equal to min spawn time.");
		return;
	}
	int const typeIndex = m_typeCombo->currentItem();
	if (typeIndex == 2)  // patrol
	{
		std::vector<Vector> const & waypoints = GodClientData::getInstance().getPatrolWaypoints();
		if (waypoints.size() < 2)
		{
			QMessageBox::warning(this, "Make Ground Spawner", "Patrol spawner requires at least 2 waypoints. Click Place Waypoints and click in the world to add them.");
			return;
		}
		m_pendingCreate.isPatrol = true;
		m_pendingCreate.waypoints = waypoints;
		m_pendingCreate.pathType = qstringToStdString(m_patrolPathCombo ? m_patrolPathCombo->currentText() : "cycle");
	}
	else
	{
		m_pendingCreate.isPatrol = false;
		m_pendingCreate.type = qstringToStdString(m_typeCombo->currentText());
		m_pendingCreate.locationType = qstringToStdString(locationType);
	}

	m_pendingCreate.name = qstringToStdString(name);
	m_pendingCreate.spawns = qstringToStdString(spawns);
	m_pendingCreate.radius = radius;
	m_pendingCreate.spawnCount = m_spawnCountSpin->value();
	m_pendingCreate.behavior = m_behaviorCombo->currentItem();
	m_pendingCreate.useGoodLocation = m_useGoodLocationCheck->isChecked();
	m_pendingCreate.minSpawnTime = minSpawnTime;
	m_pendingCreate.maxSpawnTime = maxSpawnTime;

	doCreateAndClose();
}

// ----------------------------------------------------------------------

void MakeGroundSpawnerDialog::doCreateAndClose()
{
	if (m_pendingCreate.isPatrol)
	{
		ServerCommander::getInstance().makePatrolSpawner(
			m_pendingCreate.name,
			m_pendingCreate.spawns,
			m_pendingCreate.radius,
			m_pendingCreate.spawnCount,
			m_pendingCreate.minSpawnTime,
			m_pendingCreate.maxSpawnTime,
			m_pendingCreate.pathType,
			m_pendingCreate.waypoints
		);
		if (m_placeWaypointsButton)
			m_placeWaypointsButton->setText("Place Waypoints");
	}
	else
	{
		ServerCommander::getInstance().makeGroundSpawner(
			m_pendingCreate.name,
			m_pendingCreate.type,
			m_pendingCreate.spawns,
			m_pendingCreate.radius,
			m_pendingCreate.spawnCount,
			m_pendingCreate.behavior,
			m_pendingCreate.useGoodLocation,
			m_pendingCreate.minSpawnTime,
			m_pendingCreate.maxSpawnTime,
			m_pendingCreate.locationType
		);
	}

	GodClientData::getInstance().clearPatrolWaypoints();
	GodClientData::getInstance().setPatrolWaypointPlacementMode(false);

	hide();
}

// ======================================================================
