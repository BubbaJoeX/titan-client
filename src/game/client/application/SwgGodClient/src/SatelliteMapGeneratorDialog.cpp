// ======================================================================
//
// SatelliteMapGeneratorDialog.cpp
// copyright 2026 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "SatelliteMapGeneratorDialog.h"
#include "SatelliteMapGeneratorDialog.moc"

#include "clientGame/Game.h"
#include "clientGame/GroundScene.h"
#include "clientGame/PlanetMapManagerClient.h"
#include "clientGame/SwgCameraMapCapture.h"
#include "sharedGame/PlanetMapManager.h"
#include "sharedMath/Vector2d.h"
#include "sharedNetworkMessages/MapLocation.h"
#include "sharedTerrain/TerrainObject.h"

#include <qcombobox.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qmessagebox.h>
#include <qprogressbar.h>
#include <qpushbutton.h>
#include <qtimer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

// ======================================================================

namespace SatelliteMapGeneratorDialogNamespace
{
	int const kDefaultImageSize = 4096;
	int const kDefaultTilesPerSide = 8;

	bool fileExists(char const *path)
	{
		FILE *fp = fopen(path, "rb");
		if (!fp)
			return false;
		fclose(fp);
		return true;
	}

	bool loadUncompressedTga(char const *path, QImage &outImage)
	{
		FILE *fp = fopen(path, "rb");
		if (!fp)
			return false;

		unsigned char header[18];
		if (fread(header, 1, sizeof(header), fp) != sizeof(header))
		{
			fclose(fp);
			return false;
		}

		if (header[2] != 2)
		{
			fclose(fp);
			return false;
		}

		int const w = static_cast<int>(header[12]) | (static_cast<int>(header[13]) << 8);
		int const h = static_cast<int>(header[14]) | (static_cast<int>(header[15]) << 8);
		int const depth = static_cast<int>(header[16]);
		if (w <= 0 || h <= 0 || (depth != 24 && depth != 32))
		{
			fclose(fp);
			return false;
		}

		int const bytesPerPixel = depth / 8;
		int const rowSize = w * bytesPerPixel;
		std::vector<unsigned char> row(static_cast<size_t>(rowSize));

		outImage = QImage(w, h, 32);
		for (int y = h - 1; y >= 0; --y)
		{
			if (fread(&row[0], 1, static_cast<size_t>(rowSize), fp) != static_cast<size_t>(rowSize))
			{
				fclose(fp);
				return false;
			}

			for (int x = 0; x < w; ++x)
			{
				unsigned char const *pixel = &row[static_cast<size_t>(x * bytesPerPixel)];
				outImage.setPixel(x, y, qRgb(pixel[2], pixel[1], pixel[0]));
			}
		}

		fclose(fp);
		return true;
	}

	void drawFilledCircle(QImage &image, int centerX, int centerY, int radius, QRgb color)
	{
		int const w = image.width();
		int const h = image.height();
		int const radiusSq = radius * radius;

		for (int dy = -radius; dy <= radius; ++dy)
		{
			for (int dx = -radius; dx <= radius; ++dx)
			{
				if (dx * dx + dy * dy > radiusSq)
					continue;

				int const x = centerX + dx;
				int const y = centerY + dy;
				if (x >= 0 && x < w && y >= 0 && y < h)
					image.setPixel(x, y, color);
			}
		}
	}
}

using namespace SatelliteMapGeneratorDialogNamespace;

// ======================================================================

SatelliteMapGeneratorDialog::SatelliteMapGeneratorDialog(QWidget *parent, char const *name)
: QDialog(parent, name, false, Qt::WDestructiveClose),
  m_planetLabel(0),
  m_resolutionCombo(0),
  m_filterCreatures(0),
  m_filterLairs(0),
  m_filterNpcLike(0),
  m_overlayCities(0),
  m_overlayPoi(0),
  m_progressBar(0),
  m_statusLabel(0),
  m_generateButton(0),
  m_cancelButton(0),
  m_pollTimer(0),
  m_imageSize(kDefaultImageSize),
  m_tilesPerSide(kDefaultTilesPerSide),
  m_pixelsPerTile(kDefaultImageSize / kDefaultTilesPerSide),
  m_worldMinX(-8000.f),
  m_worldMaxX(8000.f),
  m_worldMinZ(-8000.f),
  m_worldMaxZ(8000.f),
  m_captureRunning(false)
{
	setCaption("Satellite Map Generator");

	QVBoxLayout *mainLayout = new QVBoxLayout(this, 8, 8);
	NOT_NULL(mainLayout);

	m_planetLabel = new QLabel(this);
	mainLayout->addWidget(m_planetLabel);

	QLabel *resolutionLabel = new QLabel("Output resolution:", this);
	m_resolutionCombo = new QComboBox(this);
	m_resolutionCombo->insertItem("2048 (2K)");
	m_resolutionCombo->insertItem("4096 (4K)");
	m_resolutionCombo->insertItem("8192 (8K)");
	m_resolutionCombo->setCurrentItem(1);

	QHBoxLayout *resolutionLayout = new QHBoxLayout(0, 0, 4);
	resolutionLayout->addWidget(resolutionLabel);
	resolutionLayout->addWidget(m_resolutionCombo);
	mainLayout->addLayout(resolutionLayout);

	m_filterCreatures = new QCheckBox("Filter creatures (keep structures/buildouts)", this);
	m_filterLairs = new QCheckBox("Filter lairs", this);
	m_filterNpcLike = new QCheckBox("Filter NPCs and player avatars", this);
	m_overlayCities = new QCheckBox("Overlay city markers from live map data", this);
	m_overlayPoi = new QCheckBox("Overlay POI markers from live map data", this);

	m_filterCreatures->setChecked(true);
	m_filterLairs->setChecked(true);
	m_filterNpcLike->setChecked(true);
	m_overlayCities->setChecked(true);
	m_overlayPoi->setChecked(false);

	mainLayout->addWidget(m_filterCreatures);
	mainLayout->addWidget(m_filterLairs);
	mainLayout->addWidget(m_filterNpcLike);
	mainLayout->addWidget(m_overlayCities);
	mainLayout->addWidget(m_overlayPoi);

	QLabel *helpLabel = new QLabel(
		"Captures the loaded planet from a top-down orthographic view including terrain,\n"
		"water, rivers, buildouts, and static world objects. Keep this window open while\n"
		"capture runs; the view will move tile-by-tile. Output: satellite_maps/<planet>/",
		this);
	mainLayout->addWidget(helpLabel);

	m_progressBar = new QProgressBar(this);
	m_progressBar->setPercentageVisible(true);
	mainLayout->addWidget(m_progressBar);

	m_statusLabel = new QLabel("Ready.", this);
	mainLayout->addWidget(m_statusLabel);

	QHBoxLayout *buttonLayout = new QHBoxLayout(0, 0, 8);
	m_generateButton = new QPushButton("Generate Map", this);
	m_cancelButton = new QPushButton("Cancel Capture", this);
	m_cancelButton->setEnabled(false);
	buttonLayout->addWidget(m_generateButton);
	buttonLayout->addWidget(m_cancelButton);
	buttonLayout->addStretch(1);
	QPushButton *closeButton = new QPushButton("Close", this);
	buttonLayout->addWidget(closeButton);
	mainLayout->addLayout(buttonLayout);

	m_pollTimer = new QTimer(this);
	IGNORE_RETURN(connect(m_generateButton, SIGNAL(clicked()), this, SLOT(onGenerate())));
	IGNORE_RETURN(connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(onCancelCapture())));
	IGNORE_RETURN(connect(m_pollTimer, SIGNAL(timeout()), this, SLOT(onPollProgress())));
	IGNORE_RETURN(connect(closeButton, SIGNAL(clicked()), this, SLOT(reject())));

	refreshPlanetInfo();
}

// ----------------------------------------------------------------------

SatelliteMapGeneratorDialog::~SatelliteMapGeneratorDialog()
{
	if (m_captureRunning)
		SwgCameraMapCapture::cancelBatch();
}

// ----------------------------------------------------------------------

void SatelliteMapGeneratorDialog::refreshPlanetInfo()
{
	m_planetName = Game::getSceneId();

	float mapWidth = 16000.f;
	if (TerrainObject *const terrain = TerrainObject::getInstance())
	{
		float const w = terrain->getMapWidthInMeters();
		if (w > 1.f)
			mapWidth = w;
	}

	float const half = mapWidth * 0.5f;
	m_worldMinX = -half;
	m_worldMaxX = half;
	m_worldMinZ = -half;
	m_worldMaxZ = half;

	char buf[256];
	sprintf(buf, "Current planet: %s  (world extent: %.0f m)", m_planetName.c_str(), mapWidth);
	m_planetLabel->setText(buf);

	m_progressBar->setProgress(0);
}

// ----------------------------------------------------------------------

bool SatelliteMapGeneratorDialog::buildCaptureRequest(SwgCameraMapCapture::MapCaptureRequest &request) const
{
	GroundScene *const gs = dynamic_cast<GroundScene *>(Game::getScene());
	if (!gs)
	{
		QMessageBox::warning(const_cast<SatelliteMapGeneratorDialog *>(this), "Satellite Map Generator",
			"No ground scene is loaded. Load a planet before generating a map.");
		return false;
	}

	if (gs->isLoading())
	{
		QMessageBox::warning(const_cast<SatelliteMapGeneratorDialog *>(this), "Satellite Map Generator",
			"The scene is still loading. Wait for terrain and objects to finish loading.");
		return false;
	}

	if (!gs->getPlayer() && !Game::getPlayer())
	{
		QMessageBox::warning(const_cast<SatelliteMapGeneratorDialog *>(this), "Satellite Map Generator",
			"No player object is available in the current scene. Load a planet with an editor avatar before generating.");
		return false;
	}

	if (m_planetName.empty())
	{
		QMessageBox::warning(const_cast<SatelliteMapGeneratorDialog *>(this), "Satellite Map Generator",
			"Could not determine the current planet/scene id.");
		return false;
	}

	sprintf(request.outputDir, "satellite_maps/%s", m_planetName.c_str());
	request.worldMinX = m_worldMinX;
	request.worldMaxX = m_worldMaxX;
	request.worldMinZ = m_worldMinZ;
	request.worldMaxZ = m_worldMaxZ;
	request.tileWorldSize = (m_worldMaxX - m_worldMinX) / static_cast<float>(m_tilesPerSide);
	request.cameraHeight = 1200.f;
	request.settleFrames = 30;
	// Keep the God Client viewport at its configured size; resizing only the GL
	// backbuffer without resizing the GameWidget produces blank/partial captures.
	request.viewportWidth = 0;
	request.viewportHeight = 0;
	request.filterCreatures = m_filterCreatures->isChecked();
	request.filterLairs = m_filterLairs->isChecked();
	request.filterNpcLike = m_filterNpcLike->isChecked();
	request.suppressFog = true;
	request.suppressParticles = true;
	request.suppressWeather = true;
	request.useFixedWorldBounds = true;

	return true;
}

// ----------------------------------------------------------------------

bool SatelliteMapGeneratorDialog::loadTileImage(char const *basePath, QImage &outImage)
{
	char path[512];
	sprintf(path, "%s.tga", basePath);
	if (fileExists(path) && loadUncompressedTga(path, outImage))
		return true;

	sprintf(path, "%s.bmp", basePath);
	if (fileExists(path) && outImage.load(path))
		return true;

	sprintf(path, "%s.jpg", basePath);
	return fileExists(path) && outImage.load(path);
}

// ----------------------------------------------------------------------

bool SatelliteMapGeneratorDialog::stitchTiles(QImage &outImage) const
{
	outImage = QImage(m_imageSize, m_imageSize, 32);
	outImage.fill(0xff000000);

	int numCols = 0;
	int numRows = 0;
	int totalTiles = 0;
	SwgCameraMapCapture::getBatchGrid(numCols, numRows, totalTiles);
	if (totalTiles <= 0)
		totalTiles = m_tilesPerSide * m_tilesPerSide;
	if (numCols <= 0)
		numCols = m_tilesPerSide;
	if (numRows <= 0)
		numRows = m_tilesPerSide;

	for (int linearIndex = 0; linearIndex < totalTiles; ++linearIndex)
	{
		int const row = linearIndex / numCols;
		int const inner = linearIndex % numCols;
		int const col = (row % 2 == 0) ? inner : (numCols - 1 - inner);

		char basePath[512];
		SwgCameraMapCapture::getTileFileBasePath(linearIndex, basePath, sizeof(basePath));
		if (basePath[0] == '\0')
		{
			sprintf(basePath, "%s/tile_%05d_r%03d_c%03d", m_outputDir.c_str(), linearIndex, row, col);
		}

		QImage tile;
		if (!loadTileImage(basePath, tile))
		{
			char msg[640];
			sprintf(msg, "Missing or unreadable tile image:\n%s.tga", basePath);
			QMessageBox::warning(const_cast<SatelliteMapGeneratorDialog *>(this), "Satellite Map Generator", msg);
			return false;
		}

		if (tile.width() != m_pixelsPerTile || tile.height() != m_pixelsPerTile)
			tile = tile.scale(m_pixelsPerTile, m_pixelsPerTile);

		for (int y = 0; y < m_pixelsPerTile; ++y)
		{
			for (int x = 0; x < m_pixelsPerTile; ++x)
			{
				outImage.setPixel(col * m_pixelsPerTile + x, row * m_pixelsPerTile + y, tile.pixel(x, y));
			}
		}
	}

	return true;
}

// ----------------------------------------------------------------------

void SatelliteMapGeneratorDialog::overlayMapLocations(QImage &image) const
{
	if (!m_overlayCities->isChecked() && !m_overlayPoi->isChecked())
		return;

	float const worldWidth = m_worldMaxX - m_worldMinX;
	float const worldDepth = m_worldMaxZ - m_worldMinZ;
	if (worldWidth <= 0.f || worldDepth <= 0.f)
		return;

	struct CategoryDraw
	{
		uint8 category;
		QRgb color;
		int radius;
		bool enabled;
	};

	CategoryDraw categories[] =
	{
		{ PlanetMapManager::getCityCategory(),     qRgb(255, 220, 64),  6, m_overlayCities->isChecked() },
		{ PlanetMapManager::getPoiCategory(),      qRgb(96, 192, 255),  4, m_overlayPoi->isChecked() },
		{ PlanetMapManager::getWaypointCategory(), qRgb(255, 128, 64),  3, m_overlayPoi->isChecked() }
	};

	for (size_t c = 0; c < sizeof(categories) / sizeof(categories[0]); ++c)
	{
		if (!categories[c].enabled)
			continue;

		PlanetMapManagerClient::MapLocationVector locations;
		PlanetMapManagerClient::getMapLocations(categories[c].category, locations);

		for (size_t i = 0; i < locations.size(); ++i)
		{
			Vector2d const &loc = locations[i].getLocation();
			int const px = static_cast<int>((loc.x - m_worldMinX) / worldWidth * static_cast<float>(m_imageSize));
			int const py = static_cast<int>((m_worldMaxZ - loc.y) / worldDepth * static_cast<float>(m_imageSize));
			drawFilledCircle(image, px, py, categories[c].radius, categories[c].color);
		}
	}
}

// ----------------------------------------------------------------------

bool SatelliteMapGeneratorDialog::saveTGA(QImage const &image, char const *path)
{
	int const w = image.width();
	int const h = image.height();
	if (w <= 0 || h <= 0)
		return false;

	FILE *fp = fopen(path, "wb");
	if (!fp)
		return false;

	unsigned char header[18];
	memset(header, 0, sizeof(header));
	header[2] = 2;
	header[12] = static_cast<unsigned char>(w & 0xFF);
	header[13] = static_cast<unsigned char>((w >> 8) & 0xFF);
	header[14] = static_cast<unsigned char>(h & 0xFF);
	header[15] = static_cast<unsigned char>((h >> 8) & 0xFF);
	header[16] = 24;

	if (fwrite(header, 1, sizeof(header), fp) != sizeof(header))
	{
		fclose(fp);
		return false;
	}

	for (int y = h - 1; y >= 0; --y)
	{
		for (int x = 0; x < w; ++x)
		{
			QRgb const pixel = image.pixel(x, y);
			unsigned char bgr[3];
			bgr[0] = qBlue(pixel);
			bgr[1] = qGreen(pixel);
			bgr[2] = qRed(pixel);
			if (fwrite(bgr, 1, 3, fp) != 3)
			{
				fclose(fp);
				return false;
			}
		}
	}

	fclose(fp);
	return true;
}

// ----------------------------------------------------------------------

bool SatelliteMapGeneratorDialog::saveOutputImage(QImage const &image) const
{
	char path[512];
	sprintf(path, "%s/ui_map_%s.tga", m_outputDir.c_str(), m_planetName.c_str());
	if (!saveTGA(image, path))
	{
		QMessageBox::warning(const_cast<SatelliteMapGeneratorDialog *>(this), "Satellite Map Generator",
			"Failed to write the composite map image.");
		return false;
	}

	sprintf(path, "%s/ui_map_%s_preview.bmp", m_outputDir.c_str(), m_planetName.c_str());
	image.save(path, "BMP");

	char msg[640];
	sprintf(msg, "Saved ui_map_%s.tga (%dx%d) under %s/",
		m_planetName.c_str(), m_imageSize, m_imageSize, m_outputDir.c_str());
	m_statusLabel->setText(msg);
	return true;
}

// ----------------------------------------------------------------------

void SatelliteMapGeneratorDialog::onGenerate()
{
	if (m_captureRunning)
		return;

	int resolutionItem = m_resolutionCombo->currentItem();
	switch (resolutionItem)
	{
	case 0: m_imageSize = 2048; break;
	case 2: m_imageSize = 8192; break;
	default: m_imageSize = 4096; break;
	}

	m_tilesPerSide = 8;
	if (m_imageSize == 2048)
		m_tilesPerSide = 4;
	else if (m_imageSize == 8192)
		m_tilesPerSide = 16;

	m_pixelsPerTile = m_imageSize / m_tilesPerSide;
	refreshPlanetInfo();

	SwgCameraMapCapture::MapCaptureRequest request;
	if (!buildCaptureRequest(request))
		return;

	m_outputDir = request.outputDir;

	if (m_overlayCities->isChecked() || m_overlayPoi->isChecked())
		PlanetMapManagerClient::requestEntries();

	SwgCameraMapCapture::requestBatch(request);

	if (!SwgCameraMapCapture::isExternalCapturePending())
	{
		QMessageBox::warning(this, "Satellite Map Generator",
			"Could not start map capture. Try again after the scene finishes loading.");
		return;
	}

	m_captureRunning = true;
	m_generateButton->setEnabled(false);
	m_cancelButton->setEnabled(true);
	m_progressBar->setProgress(0);
	m_statusLabel->setText("Starting capture...");
	m_pollTimer->start(250);
	onPollProgress();
}

// ----------------------------------------------------------------------

void SatelliteMapGeneratorDialog::onCancelCapture()
{
	if (!m_captureRunning)
		return;

	SwgCameraMapCapture::cancelBatch();
	m_pollTimer->stop();
	m_captureRunning = false;
	m_generateButton->setEnabled(true);
	m_cancelButton->setEnabled(false);
	m_statusLabel->setText("Capture cancelled.");
}

// ----------------------------------------------------------------------

void SatelliteMapGeneratorDialog::onPollProgress()
{
	if (!m_captureRunning)
		return;

	int currentTile = 0;
	int totalTiles = 0;
	SwgCameraMapCapture::getBatchProgress(currentTile, totalTiles);

	if (SwgCameraMapCapture::isBatchActive())
	{
		if (totalTiles > 0)
		{
			int const pct = static_cast<int>(static_cast<float>(currentTile) / static_cast<float>(totalTiles) * 100.f);
			m_progressBar->setProgress(std::min(100, pct));
			char buf[128];
			sprintf(buf, "Capturing tile %d of %d...", currentTile, totalTiles);
			m_statusLabel->setText(buf);
		}
		return;
	}

	if (SwgCameraMapCapture::isExternalCapturePending())
	{
		m_progressBar->setProgress(0);
		if (SwgCameraMapCapture::isBatchActive())
			m_statusLabel->setText("Capturing...");
		else
			m_statusLabel->setText("Waiting for scene/player to start capture...");
		return;
	}

	m_pollTimer->stop();
	m_captureRunning = false;
	m_generateButton->setEnabled(true);
	m_cancelButton->setEnabled(false);
	m_progressBar->setProgress(100);

	if (!SwgCameraMapCapture::consumeBatchCompleted())
	{
		m_statusLabel->setText("Capture ended before completing.");
		return;
	}

	m_statusLabel->setText("Stitching tiles...");
	QImage composite;
	if (!stitchTiles(composite))
	{
		m_statusLabel->setText("Tile stitching failed.");
		return;
	}

	m_statusLabel->setText("Overlaying map markers...");
	overlayMapLocations(composite);

	m_statusLabel->setText("Saving output...");
	if (saveOutputImage(composite))
	{
		QMessageBox::information(this, "Satellite Map Generator",
			QString("Planet map generated successfully.\n\nOutput folder:\n") + m_outputDir.c_str());
	}
}
