// ======================================================================
//
// BuildoutEditorWindow.cpp
// copyright 2024 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "BuildoutEditorWindow.h"
#include "BuildoutEditorWindow.moc"

#include "MainFrame.h"

#include "FileControlClient.h"

#include <qcombobox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qmessagebox.h>
#include <qpainter.h>
#include <qpushbutton.h>

#include <cstdio>
#include <cstring>

// ======================================================================
// Minimal DDS loader (DXT1 only, sufficient for UI map textures)
// ======================================================================

namespace DdsLoader
{
	struct DdsHeader
	{
		uint32 magic;
		uint32 size;
		uint32 flags;
		uint32 height;
		uint32 width;
		uint32 pitchOrLinearSize;
		uint32 depth;
		uint32 mipMapCount;
		uint32 reserved1[11];
		// pixel format
		uint32 pfSize;
		uint32 pfFlags;
		uint32 pfFourCC;
		uint32 pfRGBBitCount;
		uint32 pfRBitMask;
		uint32 pfGBitMask;
		uint32 pfBBitMask;
		uint32 pfABitMask;
		uint32 caps;
		uint32 caps2;
		uint32 caps3;
		uint32 caps4;
		uint32 reserved2;
	};

	static void decodeDxt1Block(const unsigned char * block, unsigned char * out, int stride)
	{
		uint16 c0 = static_cast<uint16>(block[0] | (block[1] << 8));
		uint16 c1 = static_cast<uint16>(block[2] | (block[3] << 8));

		unsigned char palette[4][4];

		palette[0][0] = static_cast<unsigned char>(((c0 >> 11) & 0x1F) * 255 / 31);
		palette[0][1] = static_cast<unsigned char>(((c0 >> 5)  & 0x3F) * 255 / 63);
		palette[0][2] = static_cast<unsigned char>(( c0        & 0x1F) * 255 / 31);
		palette[0][3] = 255;

		palette[1][0] = static_cast<unsigned char>(((c1 >> 11) & 0x1F) * 255 / 31);
		palette[1][1] = static_cast<unsigned char>(((c1 >> 5)  & 0x3F) * 255 / 63);
		palette[1][2] = static_cast<unsigned char>(( c1        & 0x1F) * 255 / 31);
		palette[1][3] = 255;

		if (c0 > c1)
		{
			for (int c = 0; c < 3; ++c)
			{
				palette[2][c] = static_cast<unsigned char>((2 * palette[0][c] + palette[1][c]) / 3);
				palette[3][c] = static_cast<unsigned char>((palette[0][c] + 2 * palette[1][c]) / 3);
			}
			palette[2][3] = 255;
			palette[3][3] = 255;
		}
		else
		{
			for (int c = 0; c < 3; ++c)
				palette[2][c] = static_cast<unsigned char>((palette[0][c] + palette[1][c]) / 2);
			palette[2][3] = 255;
			palette[3][0] = 0;
			palette[3][1] = 0;
			palette[3][2] = 0;
			palette[3][3] = 0;
		}

		for (int row = 0; row < 4; ++row)
		{
			unsigned char indices = block[4 + row];
			for (int col = 0; col < 4; ++col)
			{
				int idx = (indices >> (col * 2)) & 0x03;
				unsigned char * pixel = out + row * stride + col * 4;
				pixel[0] = palette[idx][0]; // R
				pixel[1] = palette[idx][1]; // G
				pixel[2] = palette[idx][2]; // B
				pixel[3] = palette[idx][3]; // A
			}
		}
	}

	static bool load(const char * path, QImage & outImage)
	{
		FILE * fp = fopen(path, "rb");
		if (!fp)
			return false;

		DdsHeader hdr;
		memset(&hdr, 0, sizeof(hdr));

		if (fread(&hdr, 1, sizeof(hdr), fp) != sizeof(hdr))
		{
			fclose(fp);
			return false;
		}

		if (hdr.magic != 0x20534444) // "DDS "
		{
			fclose(fp);
			return false;
		}

		int w = static_cast<int>(hdr.width);
		int h = static_cast<int>(hdr.height);

		if (w <= 0 || h <= 0 || w > 4096 || h > 4096)
		{
			fclose(fp);
			return false;
		}

		uint32 fourCC = hdr.pfFourCC;
		bool isDxt1 = (fourCC == 0x31545844); // "DXT1"

		if (!isDxt1)
		{
			// Try uncompressed BGRA
			if ((hdr.pfFlags & 0x40) && hdr.pfRGBBitCount == 32)
			{
				int dataSize = w * h * 4;
				std::vector<unsigned char> raw(static_cast<size_t>(dataSize));
				if (fread(&raw[0], 1, static_cast<size_t>(dataSize), fp) != static_cast<size_t>(dataSize))
				{
					fclose(fp);
					return false;
				}
				fclose(fp);

				outImage.create(w, h, 32);
				for (int y = 0; y < h; ++y)
				{
					for (int x = 0; x < w; ++x)
					{
						int srcIdx = (y * w + x) * 4;
						unsigned char b = raw[static_cast<size_t>(srcIdx)];
						unsigned char g = raw[static_cast<size_t>(srcIdx + 1)];
						unsigned char r = raw[static_cast<size_t>(srcIdx + 2)];
						outImage.setPixel(x, y, qRgb(r, g, b));
					}
				}
				return true;
			}
			fclose(fp);
			return false;
		}

		// DXT1 decode
		int blocksX = (w + 3) / 4;
		int blocksY = (h + 3) / 4;
		int compressedSize = blocksX * blocksY * 8;

		std::vector<unsigned char> compressed(static_cast<size_t>(compressedSize));
		if (fread(&compressed[0], 1, static_cast<size_t>(compressedSize), fp) != static_cast<size_t>(compressedSize))
		{
			fclose(fp);
			return false;
		}
		fclose(fp);

		int paddedW = blocksX * 4;
		int paddedH = blocksY * 4;
		std::vector<unsigned char> pixels(static_cast<size_t>(paddedW * paddedH * 4), 0);

		for (int by = 0; by < blocksY; ++by)
		{
			for (int bx = 0; bx < blocksX; ++bx)
			{
				int blockIdx = (by * blocksX + bx) * 8;
				int pixelOffset = (by * 4 * paddedW + bx * 4) * 4;
				decodeDxt1Block(&compressed[static_cast<size_t>(blockIdx)],
					&pixels[static_cast<size_t>(pixelOffset)],
					paddedW * 4);
			}
		}

		outImage.create(w, h, 32);
		for (int y = 0; y < h; ++y)
		{
			for (int x = 0; x < w; ++x)
			{
				int srcIdx = (y * paddedW + x) * 4;
				unsigned char r = pixels[static_cast<size_t>(srcIdx)];
				unsigned char g = pixels[static_cast<size_t>(srcIdx + 1)];
				unsigned char b = pixels[static_cast<size_t>(srcIdx + 2)];
				outImage.setPixel(x, y, qRgb(r, g, b));
			}
		}

		return true;
	}
}

// ======================================================================
// BuildoutAreaWidget
// ======================================================================

BuildoutAreaWidget::BuildoutAreaWidget(QWidget * parent, const char * name)
: QWidget(parent, name),
  m_planetName(),
  m_areas(),
  m_mapImage(),
  m_showEvents(false),
  m_mapMinX(-8192.0f),
  m_mapMinZ(-8192.0f),
  m_mapMaxX(8192.0f),
  m_mapMaxZ(8192.0f),
  m_worldMinX(-8192.0f),
  m_worldMinZ(-8192.0f),
  m_worldMaxX(8192.0f),
  m_worldMaxZ(8192.0f)
{
	setBackgroundColor(QColor(10, 10, 10));
}

// ----------------------------------------------------------------------

BuildoutAreaWidget::~BuildoutAreaWidget()
{
}

// ----------------------------------------------------------------------

void BuildoutAreaWidget::setPlanetName(const std::string & planet)
{
	m_planetName = planet;
	update();
}

// ----------------------------------------------------------------------

void BuildoutAreaWidget::setMapImage(const QImage & image)
{
	m_mapImage = image;
	update();
}

// ----------------------------------------------------------------------

void BuildoutAreaWidget::setMapBounds(float minX, float minZ, float maxX, float maxZ)
{
	m_mapMinX = minX;
	m_mapMinZ = minZ;
	m_mapMaxX = maxX;
	m_mapMaxZ = maxZ;
}

// ----------------------------------------------------------------------

void BuildoutAreaWidget::setBuildoutAreas(const std::vector<BuildoutArea> & areas)
{
	m_areas = areas;
	recomputeWorldBounds();
	update();
}

// ----------------------------------------------------------------------

void BuildoutAreaWidget::setShowEvents(bool showEvents)
{
	m_showEvents = showEvents;
	recomputeWorldBounds();
	update();
}

// ----------------------------------------------------------------------

bool BuildoutAreaWidget::getShowEvents() const
{
	return m_showEvents;
}

// ----------------------------------------------------------------------

void BuildoutAreaWidget::recomputeWorldBounds()
{
	// Start with map bounds as the baseline so the map image always fills the view
	m_worldMinX = m_mapMinX;
	m_worldMinZ = m_mapMinZ;
	m_worldMaxX = m_mapMaxX;
	m_worldMaxZ = m_mapMaxZ;

	// Expand if any visible areas fall outside the map bounds
	for (size_t i = 0; i < m_areas.size(); ++i)
	{
		if (m_areas[i].isEvent != m_showEvents)
			continue;

		if (m_areas[i].x1 < m_worldMinX) m_worldMinX = m_areas[i].x1;
		if (m_areas[i].z1 < m_worldMinZ) m_worldMinZ = m_areas[i].z1;
		if (m_areas[i].x2 > m_worldMaxX) m_worldMaxX = m_areas[i].x2;
		if (m_areas[i].z2 > m_worldMaxZ) m_worldMaxZ = m_areas[i].z2;
	}
}

// ----------------------------------------------------------------------

const std::vector<BuildoutAreaWidget::BuildoutArea> & BuildoutAreaWidget::getBuildoutAreas() const
{
	return m_areas;
}

// ----------------------------------------------------------------------

int BuildoutAreaWidget::getSelectedCount() const
{
	int count = 0;
	for (size_t i = 0; i < m_areas.size(); ++i)
	{
		if (m_areas[i].selected && m_areas[i].isEvent == m_showEvents)
			++count;
	}
	return count;
}

// ----------------------------------------------------------------------

std::vector<std::string> BuildoutAreaWidget::getSelectedAreaNames() const
{
	std::vector<std::string> names;
	for (size_t i = 0; i < m_areas.size(); ++i)
	{
		if (m_areas[i].selected && m_areas[i].isEvent == m_showEvents)
			names.push_back(m_areas[i].name);
	}
	return names;
}

// ----------------------------------------------------------------------

std::vector<std::string> BuildoutAreaWidget::getAllSelectedAreaNames() const
{
	std::vector<std::string> names;
	for (size_t i = 0; i < m_areas.size(); ++i)
	{
		if (m_areas[i].selected)
			names.push_back(m_areas[i].name);
	}
	return names;
}

// ----------------------------------------------------------------------

void BuildoutAreaWidget::paintEvent(QPaintEvent * event)
{
	UNREF(event);

	QPainter painter(this);

	painter.fillRect(rect(), QBrush(QColor(10, 10, 10)));

	// Draw map background positioned according to map bounds within view bounds
	if (!m_mapImage.isNull())
	{
		int mapSx1, mapSy1, mapSx2, mapSy2;
		worldToScreen(m_mapMinX, m_mapMinZ, mapSx1, mapSy1);
		worldToScreen(m_mapMaxX, m_mapMaxZ, mapSx2, mapSy2);

		if (mapSx1 > mapSx2) { int t = mapSx1; mapSx1 = mapSx2; mapSx2 = t; }
		if (mapSy1 > mapSy2) { int t = mapSy1; mapSy1 = mapSy2; mapSy2 = t; }

		int mapW = mapSx2 - mapSx1;
		int mapH = mapSy2 - mapSy1;

		if (mapW > 0 && mapH > 0)
		{
			QImage scaled = m_mapImage.smoothScale(mapW, mapH);
			painter.drawImage(mapSx1, mapSy1, scaled);
		}
	}

	// Draw buildout area grid overlay (only areas matching current filter)
	for (size_t i = 0; i < m_areas.size(); ++i)
	{
		const BuildoutArea & area = m_areas[i];

		if (area.isEvent != m_showEvents)
			continue;

		int sx1, sy1, sx2, sy2;
		worldToScreen(area.x1, area.z1, sx1, sy1);
		worldToScreen(area.x2, area.z2, sx2, sy2);

		if (sx1 > sx2) { int t = sx1; sx1 = sx2; sx2 = t; }
		if (sy1 > sy2) { int t = sy1; sy1 = sy2; sy2 = t; }

		int w = sx2 - sx1;
		int h = sy2 - sy1;

		if (area.selected)
		{
			QRect areaRect(sx1, sy1, w, h);
			if (m_showEvents)
				painter.fillRect(areaRect, QBrush(QColor(200, 140, 60)));
			else
				painter.fillRect(areaRect, QBrush(QColor(60, 200, 60)));
			painter.setPen(QPen(QColor(100, 255, 100), 2u));
		}
		else
		{
			if (m_showEvents)
				painter.setPen(QPen(QColor(220, 180, 100), 1u));
			else
				painter.setPen(QPen(QColor(180, 180, 180), 1u));
		}

		painter.drawRect(sx1, sy1, w, h);

		if (w < 4 || h < 4)
			continue;

		QColor textColor = area.selected ? QColor(255, 255, 255) : QColor(200, 200, 200);
		painter.setPen(textColor);

		std::string label = area.name;
		if (!m_showEvents && label.find(m_planetName + "_") == 0)
			label = label.substr(m_planetName.size() + 1);

		int textW = painter.fontMetrics().width(label.c_str());
		if (textW < w && painter.fontMetrics().height() < h)
		{
			int textX = sx1 + (w - textW) / 2;
			int textY = sy1 + (h + painter.fontMetrics().ascent()) / 2;
			painter.drawText(textX, textY, label.c_str());
		}
	}

	// In planetary mode, render event area names as small labels within their grid cells
	if (!m_showEvents)
	{
		QFont origFont = painter.font();
		QFont smallFont = origFont;
		smallFont.setPointSize(smallFont.pointSize() - 2);
		if (smallFont.pointSize() < 6)
			smallFont.setPointSize(6);
		painter.setFont(smallFont);

		for (size_t i = 0; i < m_areas.size(); ++i)
		{
			const BuildoutArea & area = m_areas[i];
			if (!area.isEvent)
				continue;

			int sx1, sy1, sx2, sy2;
			worldToScreen(area.x1, area.z1, sx1, sy1);
			worldToScreen(area.x2, area.z2, sx2, sy2);

			if (sx1 > sx2) { int t = sx1; sx1 = sx2; sx2 = t; }
			if (sy1 > sy2) { int t = sy1; sy1 = sy2; sy2 = t; }

			int w = sx2 - sx1;
			int h = sy2 - sy1;

			if (w < 4 || h < 4)
				continue;

			painter.setPen(QColor(255, 200, 80));
			int textW = painter.fontMetrics().width(area.name.c_str());
			if (textW < w)
			{
				int textX = sx1 + (w - textW) / 2;
				int textY = sy2 - painter.fontMetrics().descent() - 2;
				painter.drawText(textX, textY, area.name.c_str());
			}
		}

		painter.setFont(origFont);
	}

	// Planet name and mode overlay
	painter.setPen(QColor(255, 255, 255));
	QFont boldFont = painter.font();
	boldFont.setBold(true);
	boldFont.setPointSize(boldFont.pointSize() + 2);
	painter.setFont(boldFont);

	std::string overlayText;
	if (m_planetName.empty())
		overlayText = "No planet selected";
	else if (m_showEvents)
		overlayText = m_planetName + " (Event)";
	else
		overlayText = m_planetName + " (Planetary)";

	painter.drawText(6, painter.fontMetrics().ascent() + 4, overlayText.c_str());
}

// ----------------------------------------------------------------------

void BuildoutAreaWidget::mousePressEvent(QMouseEvent * event)
{
	int idx = findAreaAtPoint(event->x(), event->y());
	if (idx >= 0)
	{
		m_areas[static_cast<size_t>(idx)].selected = !m_areas[static_cast<size_t>(idx)].selected;
		update();
		emit selectionChanged(getSelectedCount());
	}
}

// ----------------------------------------------------------------------

int BuildoutAreaWidget::findAreaAtPoint(int px, int py) const
{
	for (size_t i = 0; i < m_areas.size(); ++i)
	{
		const BuildoutArea & area = m_areas[i];

		if (area.isEvent != m_showEvents)
			continue;

		int sx1, sy1, sx2, sy2;
		worldToScreen(area.x1, area.z1, sx1, sy1);
		worldToScreen(area.x2, area.z2, sx2, sy2);

		if (sx1 > sx2) { int t = sx1; sx1 = sx2; sx2 = t; }
		if (sy1 > sy2) { int t = sy1; sy1 = sy2; sy2 = t; }

		if (px >= sx1 && px <= sx2 && py >= sy1 && py <= sy2)
			return static_cast<int>(i);
	}
	return -1;
}

// ----------------------------------------------------------------------

void BuildoutAreaWidget::getSquareViewport(int & ox, int & oy, int & side) const
{
	int w = width();
	int h = height();
	side = (w < h) ? w : h;
	ox = (w - side) / 2;
	oy = (h - side) / 2;
}

// ----------------------------------------------------------------------

void BuildoutAreaWidget::worldToScreen(float wx, float wz, int & sx, int & sy) const
{
	float rangeX = m_worldMaxX - m_worldMinX;
	float rangeZ = m_worldMaxZ - m_worldMinZ;

	if (rangeX <= 0.0f) rangeX = 1.0f;
	if (rangeZ <= 0.0f) rangeZ = 1.0f;

	int ox, oy, side;
	getSquareViewport(ox, oy, side);

	float sideF = static_cast<float>(side);
	sx = ox + static_cast<int>(((wx - m_worldMinX) / rangeX) * sideF);
	sy = oy + static_cast<int>(((m_worldMaxZ - wz) / rangeZ) * sideF);
}

// ======================================================================
// BuildoutEditorWindow (Buildout Manager)
// ======================================================================

BuildoutEditorWindow::BuildoutEditorWindow(QWidget * parent, const char * name)
: QDialog(parent, name, false),
  m_areaWidget(0),
  m_saveButton(0),
  m_reloadButton(0),
  m_modeButton(0),
  m_statusLabel(0),
  m_selectionLabel(0),
  m_planetCombo(0),
  m_currentPlanet()
{
	setCaption("Buildout Manager");
	setFixedSize(520, 620);

	QVBoxLayout * mainLayout = new QVBoxLayout(this, 0, 0);

	QHBoxLayout * topLayout = new QHBoxLayout(0, 4, 4);
	QLabel * planetLabel = new QLabel("Planet:", this);
	m_planetCombo = new QComboBox(this);
	m_planetCombo->insertItem("tatooine");
	m_planetCombo->insertItem("naboo");
	m_planetCombo->insertItem("corellia");
	m_planetCombo->insertItem("dantooine");
	m_planetCombo->insertItem("dathomir");
	m_planetCombo->insertItem("endor");
	m_planetCombo->insertItem("lok");
	m_planetCombo->insertItem("rori");
	m_planetCombo->insertItem("talus");
	m_planetCombo->insertItem("yavin4");
	m_planetCombo->insertItem("kashyyyk_main");
	m_planetCombo->insertItem("mustafar");
	m_modeButton = new QPushButton("Planetary", this);
	topLayout->addWidget(planetLabel);
	topLayout->addWidget(m_planetCombo);
	topLayout->addWidget(m_modeButton);
	topLayout->addStretch(1);
	mainLayout->addLayout(topLayout);

	m_areaWidget = new BuildoutAreaWidget(this, "buildoutAreaWidget");
	m_areaWidget->setFixedSize(512, 512);
	mainLayout->addWidget(m_areaWidget, 0, Qt::AlignHCenter);

	m_statusLabel = new QLabel("Select a planet and click buildout areas to select them.", this);
	mainLayout->addWidget(m_statusLabel);

	m_selectionLabel = new QLabel("Selection: (none)", this);
	mainLayout->addWidget(m_selectionLabel);

	QHBoxLayout * buttonLayout = new QHBoxLayout(0, 4, 6);
	buttonLayout->addStretch(1);
	m_reloadButton = new QPushButton("Reload Area", this);
	m_saveButton   = new QPushButton("Save Selected Buildouts", this);
	m_saveButton->setEnabled(false);
	m_reloadButton->setEnabled(false);
	buttonLayout->addWidget(m_reloadButton);
	buttonLayout->addWidget(m_saveButton);
	mainLayout->addLayout(buttonLayout);

	IGNORE_RETURN(connect(m_saveButton,    SIGNAL(clicked()), this, SLOT(onSave())));
	IGNORE_RETURN(connect(m_reloadButton,  SIGNAL(clicked()), this, SLOT(onReloadArea())));
	IGNORE_RETURN(connect(m_modeButton,    SIGNAL(clicked()), this, SLOT(onToggleMode())));
	IGNORE_RETURN(connect(m_planetCombo,   SIGNAL(activated(const QString &)), this, SLOT(onPlanetChanged(const QString &))));
	IGNORE_RETURN(connect(m_areaWidget,    SIGNAL(selectionChanged(int)), this, SLOT(onSelectionChanged(int))));
}

// ----------------------------------------------------------------------

BuildoutEditorWindow::~BuildoutEditorWindow()
{
}

// ----------------------------------------------------------------------

void BuildoutEditorWindow::onPlanetChanged(const QString & planet)
{
	m_currentPlanet = planet.latin1();
	loadPlanetMapImage(m_currentPlanet);
	loadPlanetBuildouts(m_currentPlanet);
}

// ----------------------------------------------------------------------

void BuildoutEditorWindow::onSelectionChanged(int count)
{
	m_saveButton->setEnabled(count > 0);
	m_reloadButton->setEnabled(count > 0);
	m_statusLabel->setText(QString("%1 buildout area(s) selected.").arg(count));

	std::vector<std::string> allSelected = m_areaWidget->getAllSelectedAreaNames();
	if (allSelected.empty())
	{
		m_selectionLabel->setText("Selection: (none)");
	}
	else
	{
		QString selText = "Selection: ";
		for (size_t i = 0; i < allSelected.size(); ++i)
		{
			if (i > 0)
				selText += ", ";
			selText += allSelected[i].c_str();
		}
		m_selectionLabel->setText(selText);
	}
}

// ----------------------------------------------------------------------

void BuildoutEditorWindow::onToggleMode()
{
	bool nowEvents = !m_areaWidget->getShowEvents();
	m_areaWidget->setShowEvents(nowEvents);
	updateModeButton();
	onSelectionChanged(m_areaWidget->getSelectedCount());
}

// ----------------------------------------------------------------------

void BuildoutEditorWindow::updateModeButton()
{
	if (m_areaWidget->getShowEvents())
		m_modeButton->setText("Event");
	else
		m_modeButton->setText("Planetary");
}

// ----------------------------------------------------------------------

void BuildoutEditorWindow::onSave()
{
	std::vector<std::string> selected = m_areaWidget->getSelectedAreaNames();
	if (selected.empty())
	{
		QMessageBox::warning(this, "Buildout Manager", "No buildout areas selected.");
		return;
	}

	QString msg = QString("Save %1 buildout area(s) for %2?\n\n").arg(static_cast<int>(selected.size())).arg(m_currentPlanet.c_str());
	for (size_t i = 0; i < selected.size(); ++i)
		msg += QString("  - %1\n").arg(selected[i].c_str());

	int result = QMessageBox::question(this, "Save Buildouts", msg, QMessageBox::Yes, QMessageBox::No);
	if (result != QMessageBox::Yes)
		return;

	m_statusLabel->setText("Saving buildout areas...");

	int successCount = 0;
	for (size_t i = 0; i < selected.size(); ++i)
	{
		std::string buildoutPath = "data/sku.0/sys.server/compiled/game/buildout/" + m_currentPlanet + "/" + selected[i] + ".iff";

		bool sent = FileControlClient::requestSendAsset(buildoutPath);
		if (sent)
		{
			++successCount;
			MainFrame::getInstance().textToConsole(
				("BuildoutManager: Sent " + selected[i] + " on " + m_currentPlanet).c_str());
		}
		else
		{
			MainFrame::getInstance().textToConsole(
				("BuildoutManager: FAILED to send " + selected[i] + " on " + m_currentPlanet).c_str());
		}
	}

	if (successCount > 0)
	{
		FileControlClient::requestReloadAsset(
			"data/sku.0/sys.server/compiled/game/buildout/" + m_currentPlanet + "/");
	}

	m_statusLabel->setText(QString("Saved %1/%2 buildout area(s).")
		.arg(successCount).arg(static_cast<int>(selected.size())));
	emit statusMessage("Buildout areas saved and distributed.");
}

// ----------------------------------------------------------------------

void BuildoutEditorWindow::onReloadArea()
{
	std::vector<std::string> selected = m_areaWidget->getSelectedAreaNames();
	if (selected.empty())
	{
		QMessageBox::warning(this, "Buildout Manager", "No buildout areas selected.");
		return;
	}

	QString msg = QString("Reload %1 buildout area(s) on %2?\n\nThis will tell the server to reload the buildout data and respawn all objects in the selected areas.")
		.arg(static_cast<int>(selected.size())).arg(m_currentPlanet.c_str());

	int result = QMessageBox::question(this, "Reload Buildout Areas", msg, QMessageBox::Yes, QMessageBox::No);
	if (result != QMessageBox::Yes)
		return;

	m_statusLabel->setText("Reloading buildout areas on server...");

	int reloadCount = 0;
	for (size_t i = 0; i < selected.size(); ++i)
	{
		std::string buildoutPath = "data/sku.0/sys.server/compiled/game/buildout/" + m_currentPlanet + "/" + selected[i] + ".iff";

		bool ok = FileControlClient::requestReloadAsset(buildoutPath);
		if (ok)
		{
			++reloadCount;
			MainFrame::getInstance().textToConsole(
				("BuildoutManager: Reload requested for " + selected[i] + " on " + m_currentPlanet).c_str());
		}
		else
		{
			MainFrame::getInstance().textToConsole(
				("BuildoutManager: Reload FAILED for " + selected[i] + " on " + m_currentPlanet).c_str());
		}
	}

	m_statusLabel->setText(QString("Reload requested for %1/%2 area(s).")
		.arg(reloadCount).arg(static_cast<int>(selected.size())));
	emit statusMessage("Buildout reload requested.");
}

// ----------------------------------------------------------------------

bool BuildoutEditorWindow::loadPlanetMapImage(const std::string & planet)
{
	float mapMinX = -8192.0f;
	float mapMinZ = -8192.0f;
	float mapMaxX =  8192.0f;
	float mapMaxZ =  8192.0f;

	if (planet == "kashyyyk_main")
	{
		mapMinX = -4096.0f; mapMinZ = -4096.0f;
		mapMaxX =  4096.0f; mapMaxZ =  4096.0f;
	}
	else if (planet == "kashyyyk_dead_forest" || planet == "kashyyyk_hunting"
		|| planet == "kashyyyk_north_dungeons" || planet == "kashyyyk_rryatt_trail"
		|| planet == "kashyyyk_south_dungeons")
	{
		mapMinX = -2048.0f; mapMinZ = -2048.0f;
		mapMaxX =  2048.0f; mapMaxZ =  2048.0f;
	}
	else if (planet == "mustafar")
	{
		mapMinX = -8192.0f; mapMinZ = -8192.0f;
		mapMaxX =  8192.0f; mapMaxZ =  8192.0f;
	}

	m_areaWidget->setMapBounds(mapMinX, mapMinZ, mapMaxX, mapMaxZ);

	std::string mapFile = "data/sku.0/sys.client/compiled/game/texture/ui_map_" + planet + ".dds";

	QImage img;

	if (DdsLoader::load(mapFile.c_str(), img))
	{
		m_areaWidget->setMapImage(img);
		return true;
	}

	std::string altPath = "../../" + mapFile;
	if (DdsLoader::load(altPath.c_str(), img))
	{
		m_areaWidget->setMapImage(img);
		return true;
	}

	m_areaWidget->setMapImage(QImage());
	MainFrame::getInstance().textToConsole(
		("BuildoutManager: Could not load map for " + planet).c_str());
	return false;
}

// ----------------------------------------------------------------------

void BuildoutEditorWindow::loadPlanetBuildouts(const std::string & planet)
{
	m_areaWidget->setPlanetName(planet);

	std::vector<BuildoutAreaWidget::BuildoutArea> areas;

	std::string tabPath = "dsrc/sku.0/sys.shared/compiled/game/datatables/buildout/areas_" + planet + ".tab";

	FILE * fp = fopen(tabPath.c_str(), "r");
	if (!fp)
	{
		std::string altPath = "../../" + tabPath;
		fp = fopen(altPath.c_str(), "r");
	}

	if (fp)
	{
		char line[4096];

		// Skip header row (column names) and type row
		if (fgets(line, sizeof(line), fp)) {} // column names
		if (fgets(line, sizeof(line), fp)) {} // type definitions

		while (fgets(line, sizeof(line), fp))
		{
			// Strip trailing newline/carriage return
			size_t len = strlen(line);
			while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
				line[--len] = '\0';

			if (len == 0)
				continue;

			// Parse tab-separated: area\tx1\tz1\tx2\tz2\t...
			char areaName[256] = {0};
			float x1 = 0.0f, z1 = 0.0f, x2 = 0.0f, z2 = 0.0f;

			char * tok = strtok(line, "\t");
			if (!tok || !*tok) continue;
			strncpy(areaName, tok, sizeof(areaName) - 1);

			tok = strtok(0, "\t");
			if (tok) x1 = static_cast<float>(atof(tok));
			tok = strtok(0, "\t");
			if (tok) z1 = static_cast<float>(atof(tok));
			tok = strtok(0, "\t");
			if (tok) x2 = static_cast<float>(atof(tok));
			tok = strtok(0, "\t");
			if (tok) z2 = static_cast<float>(atof(tok));

			if (x1 == 0.0f && z1 == 0.0f && x2 == 0.0f && z2 == 0.0f)
				continue;

			std::string nameStr(areaName);
			std::string prefix = planet + "_";
			bool isPlanetary = (nameStr.find(prefix) == 0);

			BuildoutAreaWidget::BuildoutArea area;
			area.selected = false;
			area.name     = areaName;
			area.x1       = x1;
			area.z1       = z1;
			area.x2       = x2;
			area.z2       = z2;
			area.isEvent  = !isPlanetary;
			areas.push_back(area);
		}

		fclose(fp);
	}
	else
	{
		MainFrame::getInstance().textToConsole(
			("BuildoutManager: Could not open areas tab for " + planet).c_str());
	}

	m_areaWidget->setBuildoutAreas(areas);

	int planetaryCount = 0;
	int eventCount = 0;
	for (size_t i = 0; i < areas.size(); ++i)
	{
		if (areas[i].isEvent)
			++eventCount;
		else
			++planetaryCount;
	}

	m_statusLabel->setText(QString("Loaded %1 planetary, %2 event buildout areas for %3.")
		.arg(planetaryCount).arg(eventCount).arg(planet.c_str()));
}

// ======================================================================
