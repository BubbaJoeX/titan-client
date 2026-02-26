// ======================================================================
//
// BuildoutEditorWindow.h
// copyright 2024 Sony Online Entertainment
//
// Buildout Manager: renders the planet map with an overlay grid of
// buildout areas. Supports selection, save, and reload of areas.
//
// ======================================================================

#ifndef INCLUDED_BuildoutEditorWindow_H
#define INCLUDED_BuildoutEditorWindow_H

// ======================================================================

#include <qdialog.h>
#include <qimage.h>
#include <string>
#include <vector>

class QPushButton;
class QLabel;
class QComboBox;

// ======================================================================

class BuildoutAreaWidget : public QWidget
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762

public:
	explicit BuildoutAreaWidget(QWidget * parent = 0, const char * name = 0);
	virtual ~BuildoutAreaWidget();

	struct BuildoutArea
	{
		std::string name;
		float       x1;
		float       z1;
		float       x2;
		float       z2;
		bool        selected;
		bool        isEvent;
	};

	void setPlanetName(const std::string & planet);
	void setMapImage(const QImage & image);
	void setMapBounds(float minX, float minZ, float maxX, float maxZ);
	void setBuildoutAreas(const std::vector<BuildoutArea> & areas);
	void setShowEvents(bool showEvents);
	bool getShowEvents() const;
	const std::vector<BuildoutArea> & getBuildoutAreas() const;
	int  getSelectedCount() const;
	std::vector<std::string> getSelectedAreaNames() const;
	std::vector<std::string> getAllSelectedAreaNames() const;

signals:
	void selectionChanged(int selectedCount);

protected:
	virtual void paintEvent(QPaintEvent * event);
	virtual void mousePressEvent(QMouseEvent * event);

private:
	BuildoutAreaWidget(const BuildoutAreaWidget &);
	BuildoutAreaWidget & operator=(const BuildoutAreaWidget &);

	int findAreaAtPoint(int px, int py) const;
	void worldToScreen(float wx, float wz, int & sx, int & sy) const;
	void getSquareViewport(int & ox, int & oy, int & side) const;
	void recomputeWorldBounds();

	std::string m_planetName;
	std::vector<BuildoutArea> m_areas;
	QImage m_mapImage;
	bool m_showEvents;

	float m_mapMinX;
	float m_mapMinZ;
	float m_mapMaxX;
	float m_mapMaxZ;

	float m_worldMinX;
	float m_worldMinZ;
	float m_worldMaxX;
	float m_worldMaxZ;
};

// ======================================================================

class BuildoutEditorWindow : public QDialog
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762

public:

	explicit BuildoutEditorWindow(QWidget * parent = 0, const char * name = 0);
	virtual ~BuildoutEditorWindow();

signals:
	void statusMessage(const char * msg);

public slots:
	void onSave();
	void onReloadArea();
	void onPlanetChanged(const QString & planet);
	void onSelectionChanged(int count);
	void onToggleMode();

private:
	BuildoutEditorWindow(const BuildoutEditorWindow &);
	BuildoutEditorWindow & operator=(const BuildoutEditorWindow &);

	void loadPlanetBuildouts(const std::string & planet);
	bool loadPlanetMapImage(const std::string & planet);
	void updateModeButton();

	BuildoutAreaWidget * m_areaWidget;
	QPushButton *        m_saveButton;
	QPushButton *        m_reloadButton;
	QPushButton *        m_modeButton;
	QLabel *             m_statusLabel;
	QLabel *             m_selectionLabel;
	QComboBox *          m_planetCombo;
	std::string          m_currentPlanet;
};

// ======================================================================

#endif
