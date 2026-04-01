// ======================================================================
//
// InstallationTurretCamera.h
//
// Ground installation gunner: first zoom level at barrel; mouse wheel zooms out to third person (10 m max).
//
// ======================================================================

#ifndef INCLUDED_InstallationTurretCamera_H
#define INCLUDED_InstallationTurretCamera_H

#include "clientObject/GameCamera.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Watcher.h"
#include "sharedMath/Vector.h"

class CreatureObject;
class GroundScene;
class MessageQueue;

class InstallationTurretCamera : public GameCamera
{
public:

	InstallationTurretCamera();
	virtual ~InstallationTurretCamera();

	virtual void setActive(bool active);
	virtual float alter(float elapsedTime);

	void setMessageQueue(MessageQueue const *queue);
	void setTurret(Object const *turret);

	Object const *getTurret() const;

	/** World point along the aim ray through the screen reticle (render-target center), at the given distance from the camera. */
	bool computeReticleAimWorldEnd(float distanceMeters, Vector &outEnd_w) const;

private:

	InstallationTurretCamera(InstallationTurretCamera const &);
	InstallationTurretCamera &operator=(InstallationTurretCamera const &);

	void updateAimWorldPoint(GroundScene *groundScene, CreatureObject const *player, Vector const &aimWorldEnd, float elapsedTime);

private:

	MessageQueue const *m_queue;
	ConstWatcher<Object> m_turret;
	float m_yaw;
	float m_pitch;
	float m_aimSendCooldown;
	bool m_hasLastQueuedAim;
	Vector m_lastQueuedAimWorld;
	int m_zoomSetting;
	float m_smoothedPullBackM;
};

inline Object const *InstallationTurretCamera::getTurret() const
{
	return m_turret;
}

#endif
