// ======================================================================
//
// SatelliteMapGeneratorDialog.h
// copyright 2026 Sony Online Entertainment
//
// God Client tool: orthographic satellite capture of the current planet
// with terrain, water, rivers, buildouts, and static structures for
// live UI planet map generation at up to 4K resolution.
//
// ======================================================================

#ifndef INCLUDED_SatelliteMapGeneratorDialog_H
#define INCLUDED_SatelliteMapGeneratorDialog_H

// ======================================================================

#include <qdialog.h>
#include <qimage.h>
#include <string>

#include "clientGame/SwgCameraMapCapture.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;

// ======================================================================

class SatelliteMapGeneratorDialog : public QDialog
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762

public:
	explicit SatelliteMapGeneratorDialog(QWidget *parent = 0, const char *name = 0);
	virtual ~SatelliteMapGeneratorDialog();

private slots:
	void onGenerate();
	void onCancelCapture();
	void onPollProgress();

private:
	void refreshPlanetInfo();
	bool buildCaptureRequest(SwgCameraMapCapture::MapCaptureRequest &request) const;
	bool stitchTiles(QImage &outImage) const;
	void overlayMapLocations(QImage &image) const;
	bool saveOutputImage(QImage const &image) const;
	static bool saveTGA(QImage const &image, char const *path);
	static bool loadTileImage(char const *basePath, QImage &outImage);

	QLabel       *m_planetLabel;
	QComboBox    *m_resolutionCombo;
	QCheckBox    *m_filterCreatures;
	QCheckBox    *m_filterLairs;
	QCheckBox    *m_filterNpcLike;
	QCheckBox    *m_overlayCities;
	QCheckBox    *m_overlayPoi;
	QProgressBar *m_progressBar;
	QLabel       *m_statusLabel;
	QPushButton  *m_generateButton;
	QPushButton  *m_cancelButton;
	QTimer       *m_pollTimer;

	std::string   m_planetName;
	std::string   m_outputDir;
	int           m_imageSize;
	int           m_tilesPerSide;
	int           m_pixelsPerTile;
	float         m_worldMinX;
	float         m_worldMaxX;
	float         m_worldMinZ;
	float         m_worldMaxZ;
	bool          m_captureRunning;

	SatelliteMapGeneratorDialog(SatelliteMapGeneratorDialog const &);
	SatelliteMapGeneratorDialog &operator=(SatelliteMapGeneratorDialog const &);
};

// ======================================================================

#endif
