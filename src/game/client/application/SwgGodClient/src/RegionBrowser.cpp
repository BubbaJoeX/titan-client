// ======================================================================
//
// RegionBrowser.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"

#include "RegionBrowser.h"
#include "RegionBrowser.moc"

#include "UnicodeUtils.h"
#include "unicodeArchive/UnicodeArchive.h"

#include "sharedFoundation/NetworkId.h"
#include "sharedFoundation/NetworkIdArchive.h"

#include "sharedNetworkMessages/MessageRegionListCircleResponse.h"
#include "sharedNetworkMessages/MessageRegionListRectResponse.h"

#include "RegionRenderer.h"
#include "ServerCommander.h"

#include <qcheckbox.h>
#include <qlistview.h>
#include <qtimer.h>

#include <algorithm>
#include <vector>

namespace
{
	class RegionListViewItem : public QListViewItem
	{
	public:
		RegionListViewItem(QListView *parent, QString const &text, std::string const &regionKey, bool isLeafRegion)
		: QListViewItem(parent, text), m_regionKey(regionKey), m_isLeafRegion(isLeafRegion) {}

		RegionListViewItem(QListViewItem *parent, QString const &text, std::string const &regionKey, bool isLeafRegion)
		: QListViewItem(parent, text), m_regionKey(regionKey), m_isLeafRegion(isLeafRegion) {}

		bool isLeafRegion() const { return m_isLeafRegion; }
		std::string const &regionKey() const { return m_regionKey; }

	private:
		std::string m_regionKey;
		bool m_isLeafRegion;
	};

	void splitRegionPath(std::string const &raw, std::vector<std::string> &segments)
	{
		segments.clear();
		std::string cur;
		for (size_t i = 0; i < raw.size(); ++i)
		{
			char const c = raw[i];
			if (c == '/' || c == '\\' || c == ':')
			{
				if (!cur.empty())
				{
					segments.push_back(cur);
					cur.clear();
				}
				continue;
			}
			cur += c;
		}
		if (!cur.empty())
			segments.push_back(cur);
		if (segments.empty() && !raw.empty())
			segments.push_back(raw);
	}

	struct RegionEntrySort
	{
		bool operator()(RegionRenderer::RegionListEntry const &a, RegionRenderer::RegionListEntry const &b) const
		{
			if (a.planet != b.planet)
				return a.planet < b.planet;
			if (a.rawName != b.rawName)
				return a.rawName < b.rawName;
			if (a.isRect != b.isRect)
				return a.isRect > b.isRect;
			return a.storageKey < b.storageKey;
		}
	};

	RegionListViewItem *findOrCreateFolder(QListView *root, QString const &label)
	{
		for (QListViewItem *c = root->firstChild(); c; c = c->nextSibling())
		{
			if (c->text(0) == label)
				return dynamic_cast<RegionListViewItem *>(c);
		}
		return new RegionListViewItem(root, label, std::string(), false);
	}

	RegionListViewItem *findOrCreateFolder(QListViewItem *parent, QString const &label)
	{
		for (QListViewItem *c = parent->firstChild(); c; c = c->nextSibling())
		{
			if (c->text(0) == label)
				return dynamic_cast<RegionListViewItem *>(c);
		}
		return new RegionListViewItem(parent, label, std::string(), false);
	}
}

// ======================================================================

RegionBrowser::RegionBrowser(QWidget *theParent, const char *theName)
: BaseRegionBrowser (theParent, theName),
  MessageDispatch::Receiver (),
  m_timer(NULL),
  m_visible(false)
{
	connectToMessage ("MessageRegionListCircleResponse");
	connectToMessage ("MessageRegionListRectResponse");

	m_timer = new QTimer(this, "RegionRendererTimer");
	connect(m_timer, SIGNAL(timeout()), this, SLOT(slotTimerTimeOut()));

	// UIC used to connect these to BaseRegionBrowser stub slots; bind to RegionBrowser so filters work.
	if (m_pvpCheckBox)
		QObject::connect(m_pvpCheckBox, SIGNAL(toggled(bool)), this, SLOT(onPvPCheck(bool)));
	if (m_municipalCheckBox)
		QObject::connect(m_municipalCheckBox, SIGNAL(toggled(bool)), this, SLOT(onMunicipalCheck(bool)));
	if (m_buildableCheckBox)
		QObject::connect(m_buildableCheckBox, SIGNAL(toggled(bool)), this, SLOT(onBuildableCheck(bool)));
	if (m_geographicCheckBox)
		QObject::connect(m_geographicCheckBox, SIGNAL(toggled(bool)), this, SLOT(onGeographicCheck(bool)));
	if (m_difficultyCheckBox)
		QObject::connect(m_difficultyCheckBox, SIGNAL(toggled(bool)), this, SLOT(onDifficultyCheck(bool)));
	if (m_spawnableCheckBox)
		QObject::connect(m_spawnableCheckBox, SIGNAL(toggled(bool)), this, SLOT(onSpawnableCheck(bool)));
	if (m_missionCheckBox)
		QObject::connect(m_missionCheckBox, SIGNAL(toggled(bool)), this, SLOT(onMissionCheck(bool)));

	if (m_regionTree)
	{
		m_regionTree->addColumn("Regions");
		m_regionTree->setRootIsDecorated(true);
		m_regionTree->setSelectionMode(QListView::Single);
		m_regionTree->setColumnWidthMode(0, QListView::Maximum);
		IGNORE_RETURN(connect(m_regionTree, SIGNAL(selectionChanged(QListViewItem *)), this, SLOT(onRegionTreeSelection(QListViewItem *))));
	}
}

// ======================================================================

RegionBrowser::~RegionBrowser()
{
}

//-----------------------------------------------------------------------

void RegionBrowser::receiveMessage(const MessageDispatch::Emitter & source, const MessageDispatch::MessageBase & message)
{
	UNREF(source);

	if (message.isType ("MessageRegionListCircleResponse"))
	{
		Archive::ReadIterator ri = NON_NULL (safe_cast<const GameNetworkMessage *>(&message))->getByteStream().begin();
		const MessageRegionListCircleResponse circlersp(ri);
		int const radiusI = static_cast<int>(circlersp.getRadius() + 0.5f);
		m_regionRenderer->updateCircleRegion(circlersp.getWorldX(), circlersp.getWorldZ(), radiusI, Unicode::wideToNarrow(circlersp.getName()), circlersp.getPlanet(), circlersp.getPvP(), circlersp.getBuildable() != 0, circlersp.getSpawnable(), circlersp.getMunicipal() != 0, circlersp.getGeographical(), circlersp.getMinDifficulty(), circlersp.getMaxDifficulty(), circlersp.getMission());
		rebuildRegionTree();
	}
	else if (message.isType ("MessageRegionListRectResponse"))
	{
		Archive::ReadIterator ri = NON_NULL (safe_cast<const GameNetworkMessage *>(&message))->getByteStream().begin();
		const MessageRegionListRectResponse rectrsp(ri);
		m_regionRenderer->updateRectRegion(rectrsp.getWorldX(), rectrsp.getWorldZ(), rectrsp.getURWorldX(), rectrsp.getURWorldZ(), Unicode::wideToNarrow(rectrsp.getName()), rectrsp.getPlanet(), rectrsp.getPvP(), rectrsp.getBuildable() != 0, rectrsp.getSpawnable(), rectrsp.getMunicipal() != 0, rectrsp.getGeographical(), rectrsp.getMinDifficulty(), rectrsp.getMaxDifficulty(), rectrsp.getMission());
		rebuildRegionTree();
	}
}

//-----------------------------------------------------------------------

void RegionBrowser::showEvent(QShowEvent *e)
{
    BaseRegionBrowser::showEvent(e);
		m_timer->start(1);
		m_visible = true;
		// Apply checkbox state (defaults are off) so the map matches the toolbar without a toggle.
		if (m_regionRenderer)
		{
			if (m_pvpCheckBox)
				m_regionRenderer->filterOnPvP(m_pvpCheckBox->isChecked());
			if (m_municipalCheckBox)
				m_regionRenderer->filterOnMunicipal(m_municipalCheckBox->isChecked());
			if (m_buildableCheckBox)
				m_regionRenderer->filterOnBuildable(m_buildableCheckBox->isChecked());
			if (m_geographicCheckBox)
				m_regionRenderer->filterOnGeographical(m_geographicCheckBox->isChecked());
			if (m_difficultyCheckBox)
				m_regionRenderer->filterOnDifficulty(m_difficultyCheckBox->isChecked());
			if (m_spawnableCheckBox)
				m_regionRenderer->filterOnSpawnable(m_spawnableCheckBox->isChecked());
			if (m_missionCheckBox)
				m_regionRenderer->filterOnMission(m_missionCheckBox->isChecked());
		}
		rebuildRegionTree();
}

//-----------------------------------------------------------------------

void RegionBrowser::hideEvent(QHideEvent *e)
{
    BaseRegionBrowser::hideEvent(e);
		m_timer->stop();
		m_visible = false;
}

//-----------------------------------------------------------------------

bool RegionBrowser::isVisible()
{
	return m_visible;
}

//-----------------------------------------------------------------------

void RegionBrowser::slotTimerTimeOut()
{
	ServerCommander::getInstance().getRegionsList();
	m_timer->start(5000);
}

//-----------------------------------------------------------------------

void RegionBrowser::onMissionCheck(bool checked)
{
	m_regionRenderer->filterOnMission(checked);
}

//-----------------------------------------------------------------------

void RegionBrowser::onBuildableCheck(bool checked)
{
	m_regionRenderer->filterOnBuildable(checked);
}

//-----------------------------------------------------------------------

void RegionBrowser::onDifficultyCheck(bool checked)
{
	m_regionRenderer->filterOnDifficulty(checked);
}

//-----------------------------------------------------------------------

void RegionBrowser::onGeographicCheck(bool checked)
{
	m_regionRenderer->filterOnGeographical(checked);
}

//-----------------------------------------------------------------------

void RegionBrowser::onMunicipalCheck(bool checked)
{
	m_regionRenderer->filterOnMunicipal(checked);
}

//-----------------------------------------------------------------------

void RegionBrowser::onPvPCheck(bool checked)
{
	m_regionRenderer->filterOnPvP(checked);
}

//-----------------------------------------------------------------------

void RegionBrowser::onSpawnableCheck(bool checked)
{
	m_regionRenderer->filterOnSpawnable(checked);
}

//-----------------------------------------------------------------------

std::map<std::string, RegionRenderer::Region*> RegionBrowser::getVisibleRegions()
{
	return m_regionRenderer->getVisibleRegions();
}

//-----------------------------------------------------------------------

void RegionBrowser::onRegionTreeSelection(QListViewItem *item)
{
	if (!m_regionRenderer)
		return;
	RegionListViewItem *const ri = dynamic_cast<RegionListViewItem *>(item);
	if (ri && ri->isLeafRegion() && !ri->regionKey().empty())
	{
		m_regionRenderer->setHighlightedStorageKey(ri->regionKey());
		m_regionRenderer->focusCameraOnStorageKey(ri->regionKey());
	}
	else
		m_regionRenderer->setHighlightedStorageKey("");
}

//-----------------------------------------------------------------------

void RegionBrowser::rebuildRegionTree()
{
	if (!m_regionTree || !m_regionRenderer)
		return;

	m_regionTree->clear();

	std::vector<RegionRenderer::RegionListEntry> entries;
	m_regionRenderer->listAllRegions(entries);
	std::sort(entries.begin(), entries.end(), RegionEntrySort());

	for (size_t e = 0; e < entries.size(); ++e)
	{
		RegionRenderer::RegionListEntry const &ent = entries[e];
		std::vector<std::string> segments;
		splitRegionPath(ent.rawName, segments);

		QString const planetLabel = QString(ent.planet.c_str());
		RegionListViewItem *planetItem = findOrCreateFolder(m_regionTree, planetLabel);

		QListViewItem *parent = planetItem;
		if (segments.size() > 1)
		{
			for (size_t s = 0; s + 1 < segments.size(); ++s)
			{
				QString const segLabel = QString(segments[s].c_str());
				RegionListViewItem *folder = findOrCreateFolder(parent, segLabel);
				parent = folder;
			}
		}

		QString leafBase = QString(segments.empty() ? ent.rawName.c_str() : segments.back().c_str());
		QString const leafText = leafBase + (ent.isRect ? " [rect]" : " [circ]");
		IGNORE_RETURN(new RegionListViewItem(parent, leafText, ent.storageKey, true));
	}
}

// ======================================================================
