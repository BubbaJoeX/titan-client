// ======================================================================
//
// MakeGroundSpawnerDialog.h
// God client dialog for creating ground spawners
//
// ======================================================================

#ifndef INCLUDED_MakeGroundSpawnerDialog_H
#define INCLUDED_MakeGroundSpawnerDialog_H

// ======================================================================

#include "sharedMath/Vector.h"
#include <qdialog.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <vector>
#include <qspinbox.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <qlabel.h>

// ======================================================================

class MakeGroundSpawnerDialog : public QDialog
{
	Q_OBJECT

public:
	MakeGroundSpawnerDialog(QWidget* parent = 0, char const* name = 0);
	virtual ~MakeGroundSpawnerDialog();

	void loadCreatureList();

private slots:
	void onAccept();
	void doCreateAndClose();
	void onTypeChanged(int index);
	void onPlaceWaypoints();
	void onPatrolWaypointChanged();
	void onPatrolPathChanged();

private:
	QComboBox*       m_mobCombo;
	QLineEdit*       m_nameEdit;
	QComboBox*       m_typeCombo;
	QLineEdit*       m_locationTypeEdit;
	QLineEdit*       m_radiusEdit;
	QSpinBox*        m_spawnCountSpin;
	QComboBox*       m_behaviorCombo;
	QCheckBox*       m_useGoodLocationCheck;
	QLineEdit*       m_minSpawnTimeEdit;
	QLineEdit*       m_maxSpawnTimeEdit;
	QComboBox*       m_patrolPathCombo;
	QPushButton*     m_placeWaypointsButton;
	QLabel*          m_waypointCountLabel;

	struct PendingCreate
	{
		std::string name;
		std::string spawns;
		std::string type;
		std::string locationType;
		float radius;
		int spawnCount;
		int behavior;
		bool useGoodLocation;
		float minSpawnTime;
		float maxSpawnTime;
		std::string pathType;
		std::vector<Vector> waypoints;
		bool isPatrol;
	};
	PendingCreate m_pendingCreate;
};

// ======================================================================

#endif
