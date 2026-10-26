// ======================================================================
//
// BookmarkData.cpp
// copyright(c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "BookmarkData.h"

#include <qsettings.h>

#include <cstdio>

// ======================================================================

const char* const  BookmarkData::Messages::CAMERA_BOOKMARKS_CHANGED = "BookmarkData::Messages::CAMERA_BOOKMARKS_CHANGED";
const char* const BookmarkData::Messages::OBJECT_BOOKMARKS_CHANGED  = "BookmarkData::Messages::OBJECT_BOOKMARKS_CHANGED";

//-----------------------------------------------------------------

struct BookmarkData::CameraBookmarkContainer
{
	typedef std::vector<CameraBookmark> Container;
	BookmarkData::CameraBookmarkContainer::Container  container;
};

//-----------------------------------------------------------------

struct BookmarkData::ObjectBookmarkContainer
{
	typedef std::vector<ObjectBookmark> Container;
	typedef BookmarkData::ObjectBookmarkContainer::Container::iterator Iterator;
	BookmarkData::ObjectBookmarkContainer::Container  container;
};

//-----------------------------------------------------------------

BookmarkData::BookmarkData()
: Singleton <BookmarkData>(),
  MessageDispatch::Emitter(),
  m_cameras(0),
  m_objects(0),
  m_sceneId()
{
	m_cameras = new CameraBookmarkContainer;
	m_objects = new ObjectBookmarkContainer;
}

//----------------------------------------------------------------------

BookmarkData::~BookmarkData()
{
	delete m_cameras;
	delete m_objects;
	m_cameras = 0;
	m_objects = 0;
}

//-----------------------------------------------------------------

void BookmarkData::addCameraBookmark(const std::string & name, const FreeCamera::Info & info) 
{
	CameraBookmark k;
	k.info = info;

	if(!name.empty() && name [name.size()-1] == '\n')
		k.name = name.substr(0, name.size() - 1);
	else
		k.name = name;

	m_cameras->container.push_back(k);
	saveCameraBookmarks();

	MessageDispatch::MessageBase m(Messages::CAMERA_BOOKMARKS_CHANGED);
	emitMessage(m);
}

//-----------------------------------------------------------------

void BookmarkData::removeCameraBookmark(size_t index)
{
	DEBUG_FATAL(index > m_cameras->container.size(),("out of range\n"));
	IGNORE_RETURN(m_cameras->container.erase(m_cameras->container.begin() + index));
	saveCameraBookmarks();
	MessageDispatch::MessageBase m(Messages::CAMERA_BOOKMARKS_CHANGED);
	emitMessage(m);
}

//-----------------------------------------------------------------

size_t BookmarkData::getNumCameraBookmarks() const
{
	return m_cameras->container.size();
}

//-----------------------------------------------------------------

const BookmarkData::CameraBookmark &    BookmarkData::getCameraBookmark(size_t index) const
{
	DEBUG_FATAL(index > m_cameras->container.size(),("out of range\n"));
	return m_cameras->container [index];
}

//-----------------------------------------------------------------

void  BookmarkData::addObjectBookmark(const NetworkId & id, const std::string & name, const FreeCamera::Info & info)
{
	for(ObjectBookmarkContainer::Iterator it = m_objects->container.begin(); it != m_objects->container.end(); ++it)
	{
		if((*it).id == id)
		{
			//-- simply change the data
			(*it).cameraBookmark.info = info;
			MessageDispatch::MessageBase m(Messages::OBJECT_BOOKMARKS_CHANGED);
			return;		
		}
	}

	ObjectBookmark k;
	k.cameraBookmark.info = info;

	if(!name.empty() && name [name.size()-1] == '\n')
		k.cameraBookmark.name = name.substr(0, name.size() - 1);
	else
		k.cameraBookmark.name = name;

	k.id                    = id;
	m_objects->container.push_back(k);
	MessageDispatch::MessageBase m(Messages::OBJECT_BOOKMARKS_CHANGED);
	emitMessage(m);
}

//-----------------------------------------------------------------

void BookmarkData::removeObjectBookmark(size_t index)
{
	DEBUG_FATAL(index > m_objects->container.size(),("out of range\n"));
	IGNORE_RETURN(m_objects->container.erase(m_objects->container.begin() + index));
	MessageDispatch::MessageBase m(Messages::OBJECT_BOOKMARKS_CHANGED);
	emitMessage(m);
}

//-----------------------------------------------------------------

size_t BookmarkData::getNumObjectBookmarks() const
{
	return m_objects->container.size();
}

//-----------------------------------------------------------------

const BookmarkData::ObjectBookmark& BookmarkData::getObjectBookmark(size_t index) const
{
	DEBUG_FATAL(index >= m_objects->container.size(),("out of range\n"));
	return m_objects->container [index];
}


void BookmarkData::setScene(const std::string & sceneId)
{
	if(sceneId == m_sceneId)
		return;

	m_sceneId = sceneId;
	loadCameraBookmarks();

	MessageDispatch::MessageBase m(Messages::CAMERA_BOOKMARKS_CHANGED);
	emitMessage(m);
}


void BookmarkData::saveCameraBookmarks() const
{
	if(m_sceneId.empty())
		return;

	QSettings settings;
	settings.insertSearchPath(QSettings::Windows, "/SOE/SwgGodClient");
	settings.beginGroup(QString("CameraBookmarks/") + m_sceneId.c_str());

	int const count = static_cast<int>(m_cameras->container.size());
	IGNORE_RETURN(settings.writeEntry("count", count));
	for(int i = 0; i < count; ++i)
	{
		const CameraBookmark & k = m_cameras->container [static_cast<size_t>(i)];
		QString camera;
		camera.sprintf("%f %f %f %f %f %f", k.info.translate.x, k.info.translate.y, k.info.translate.z, k.info.distance, k.info.yaw, k.info.pitch);
		IGNORE_RETURN(settings.writeEntry(QString("%1/name").arg(i), QString(k.name.c_str())));
		IGNORE_RETURN(settings.writeEntry(QString("%1/camera").arg(i), camera));
	}

	settings.endGroup();
}


void BookmarkData::loadCameraBookmarks()
{
	m_cameras->container.clear();
	if(m_sceneId.empty())
		return;

	QSettings settings;
	settings.insertSearchPath(QSettings::Windows, "/SOE/SwgGodClient");
	settings.beginGroup(QString("CameraBookmarks/") + m_sceneId.c_str());

	int const count = settings.readNumEntry("count", 0);
	for(int i = 0; i < count; ++i)
	{
		QString const name(settings.readEntry(QString("%1/name").arg(i), QString()));
		QString const camera(settings.readEntry(QString("%1/camera").arg(i), QString()));

		CameraBookmark k;
		if(sscanf(camera.latin1(), "%f %f %f %f %f %f", &k.info.translate.x, &k.info.translate.y, &k.info.translate.z, &k.info.distance, &k.info.yaw, &k.info.pitch) != 6)
			continue;

		k.name = name.latin1() ? name.latin1() : "";
		m_cameras->container.push_back(k);
	}

	settings.endGroup();
}

// ======================================================================
