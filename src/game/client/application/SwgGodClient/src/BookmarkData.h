// ======================================================================
//
// BookmarkData.h
// copyright(c) 2001 Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_BookmarkData_H
#define INCLUDED_BookmarkData_H

#include "clientGame/FreeCamera.h"
#include "Singleton/Singleton.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/NetworkId.h"

// ======================================================================

/**
* BookmarkData is a singleton for storing information about the user's currently
* assigned bookmarks.  Camera bookmarks belong to the current scene: they are saved
* per scene (QSettings group CameraBookmarks) whenever they change, and reloaded by
* setScene when the GroundScene changes.  Object bookmarks are session-only, since
* network ids do not outlive the session.
*
*/

class BookmarkData : public Singleton<BookmarkData>, public MessageDispatch::Emitter
{

public:

	struct Messages
	{
		static const char * const CAMERA_BOOKMARKS_CHANGED;
		static const char * const OBJECT_BOOKMARKS_CHANGED;
	};

	struct CameraBookmark
	{
		FreeCamera::Info info;
		std::string      name;
	};

	struct ObjectBookmark
	{
		CameraBookmark   cameraBookmark;
		NetworkId        id;
	};

	struct CameraBookmarkContainer;
	struct ObjectBookmarkContainer;

	BookmarkData();
	~BookmarkData();

	void                      addCameraBookmark(const std::string & name, const FreeCamera::Info & info);
	void                      removeCameraBookmark(size_t index);
	size_t                    getNumCameraBookmarks() const;
	const CameraBookmark&     getCameraBookmark(size_t index) const;

	void                      addObjectBookmark(const NetworkId & id, const std::string & name, const FreeCamera::Info & info);
	void                      removeObjectBookmark(size_t index);
	size_t                    getNumObjectBookmarks() const;
	const ObjectBookmark&     getObjectBookmark(size_t index) const;

	/// Switches the camera bookmarks to those saved for \a sceneId.
	void                      setScene(const std::string & sceneId);

private:
	//disabled
	BookmarkData(const BookmarkData & rhs);
	BookmarkData& operator=(const BookmarkData & rhs);

	void                      saveCameraBookmarks() const;
	void                      loadCameraBookmarks();

private:
	CameraBookmarkContainer* m_cameras;
	ObjectBookmarkContainer* m_objects;
	std::string              m_sceneId;
};

// ======================================================================

#endif
