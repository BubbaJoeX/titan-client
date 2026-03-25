// ======================================================================
//
// InstallationTurretCamera.cpp
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/InstallationTurretCamera.h"

#include "clientGame/CreatureObject.h"
#include "clientGame/Game.h"
#include "clientGame/GroundScene.h"
#include "clientGame/InstallationObject.h"
#include "clientGame/TurretObject.h"
#include "sharedFoundation/GameControllerMessage.h"
#include "sharedFoundation/NetworkId.h"
#include "sharedFoundation/MessageQueue.h"
#include "sharedObject/AlterResult.h"
#include "sharedObject/CellProperty.h"
#include "sharedMath/Transform.h"
#include "sharedMath/Vector.h"

namespace InstallationTurretCameraNamespace
{
	float const cs_pitchLimit = PI * 0.5f; // +/- 90 degrees (180 total)
	float const cs_defaultEyeY = 1.6f;

	bool findInstallationTurretParts(Object const *const installationObj, TurretObject *&outTurretObject, Object *&outBarrel)
	{
		outTurretObject = 0;
		outBarrel = 0;
		if (!installationObj)
			return false;

		Object *const writable = const_cast<Object *>(installationObj);
		InstallationObject *const installationObject = dynamic_cast<InstallationObject *>(writable);
		if (!installationObject)
			return false;

		int const childCount = installationObject->getNumberOfChildObjects();
		for (int i = 0; i < childCount; ++i)
		{
			TurretObject *const turretObject = dynamic_cast<TurretObject *>(installationObject->getChildObject(i));
			if (turretObject)
			{
				outTurretObject = turretObject;
				if (turretObject->getNumberOfChildObjects() > 0)
					outBarrel = turretObject->getChildObject(0);
				return true;
			}
		}
		return false;
	}

	void trackChildTurretTowardAim(Object const *const installationObj, Vector const &aimWorldEnd, float const elapsedTime)
	{
		if (!installationObj || elapsedTime <= 0.f)
			return;

		TurretObject *turretObject = 0;
		Object *barrel = 0;
		if (!findInstallationTurretParts(installationObj, turretObject, barrel))
			return;

		turretObject->trackTowardWorldPosition(aimWorldEnd, elapsedTime);
	}
}

using namespace InstallationTurretCameraNamespace;

InstallationTurretCamera::InstallationTurretCamera() :
	GameCamera(),
	m_queue(0),
	m_turret(0),
	m_yaw(0.f),
	m_pitch(0.f),
	m_aimSendCooldown(0.f),
	m_hasLastQueuedAim(false),
	m_lastQueuedAimWorld()
{
}

InstallationTurretCamera::~InstallationTurretCamera()
{
	m_queue = 0;
}

void InstallationTurretCamera::setActive(bool const active)
{
	if (active)
	{
		m_yaw = 0.f;
		m_pitch = 0.f;
		m_hasLastQueuedAim = false;
		m_aimSendCooldown = 0.f;
	}
	GameCamera::setActive(active);
}

void InstallationTurretCamera::setMessageQueue(MessageQueue const *const queue)
{
	m_queue = queue;
}

void InstallationTurretCamera::setTurret(Object const *const turret)
{
	if (m_turret.getPointer() != turret)
	{
		m_yaw = 0.f;
		m_pitch = 0.f;
		m_hasLastQueuedAim = false;
		m_aimSendCooldown = 0.f;
	}
	m_turret = turret;
}

void InstallationTurretCamera::updateAimWorldPoint(GroundScene *const groundScene, CreatureObject const *const player, Vector const &aimWorldEnd, float const elapsedTime)
{
	if (!groundScene || !player)
	{
		return;
	}

	Object const *const turretObj = m_turret.getPointer();
	if (!turretObj)
	{
		return;
	}

	NetworkId const turretId = turretObj->getNetworkId();
	if (!turretId.isValid())
	{
		return;
	}

	m_aimSendCooldown -= elapsedTime;

	Vector const delta(aimWorldEnd - m_lastQueuedAimWorld);
	bool const aimMoved = !m_hasLastQueuedAim || delta.magnitudeSquared() > CONST_REAL(0.25f);
	if (m_aimSendCooldown > 0.f && !aimMoved)
	{
		return;
	}

	if (!aimMoved)
	{
		return;
	}

	m_aimSendCooldown = 0.12f;
	m_hasLastQueuedAim = true;
	m_lastQueuedAimWorld = aimWorldEnd;
	groundScene->queueTurretGunnerAim(turretId, aimWorldEnd);
}

float InstallationTurretCamera::alter(float const elapsedTime)
{
	if (!isActive())
	{
		return AlterResult::cms_alterNextFrame;
	}

	Object const *const turret = m_turret.getPointer();
	if (!turret)
	{
		return AlterResult::cms_alterNextFrame;
	}

	NOT_NULL(m_queue);
	for (int i = 0; i < m_queue->getNumberOfMessages(); ++i)
	{
		int message = 0;
		float value = 0.f;
		m_queue->getMessage(i, &message, &value);

		switch (message)
		{
		case CM_cameraYawMouse:
			m_yaw += value;
			break;
		case CM_cameraPitchMouse:
			m_pitch += value;
			break;
		default:
			break;
		}
	}

	m_pitch = clamp(-cs_pitchLimit, m_pitch, cs_pitchLimit);

	CellProperty *const cellProperty = turret->getParentCell();
	setParentCell(cellProperty);

	CreatureObject const *const playerCreature = Game::getPlayerCreature();
	float eyeX = 0.f;
	float eyeY = cs_defaultEyeY;
	float eyeZ = 0.f;
	if (playerCreature)
	{
		eyeX = playerCreature->getTurretGunnerEyeOffX();
		eyeY = playerCreature->getTurretGunnerEyeOffY();
		eyeZ = playerCreature->getTurretGunnerEyeOffZ();
	}

	Transform eye_o2t(Transform::IF_none);
	eye_o2t = Transform::identity;
	eye_o2t.setPosition_p(Vector(eyeX, eyeY, eyeZ));

	CellProperty::setPortalTransitionsEnabled(false);
		Transform const install_o2p(turret->getTransform_o2p());

		TurretObject *turretObject = 0;
		Object *barrel = 0;
		Transform camera_p(Transform::IF_none);

		if (findInstallationTurretParts(turret, turretObject, barrel) && barrel)
		{
			// Eye offsets are in barrel-local space (turret at identity yaw / barrel at rest = "north" baseline).
			Transform const turretObj_o2i(turretObject->getTransform_o2p());
			Transform const barrel_o2t(barrel->getTransform_o2p());
			Transform barrelChain(Transform::IF_none);
			barrelChain.multiply(turretObj_o2i, barrel_o2t);
			Transform upToEye(Transform::IF_none);
			upToEye.multiply(barrelChain, eye_o2t);
			Transform look(Transform::IF_none);
			look.yaw_l(m_yaw);
			look.pitch_l(m_pitch);
			Transform cam_o2i(Transform::IF_none);
			cam_o2i.multiply(upToEye, look);
			camera_p.multiply(install_o2p, cam_o2i);
		}
		else
		{
			Transform eye_p(Transform::IF_none);
			eye_p.multiply(install_o2p, eye_o2t);
			Transform yawPitch(Transform::IF_none);
			yawPitch.yaw_l(m_yaw);
			yawPitch.pitch_l(m_pitch);
			camera_p.multiply(eye_p, yawPitch);
		}

		// Aim point matches gunner yaw/pitch (same basis as the view). Drives server aim + TurretObject slew.
		static float const s_aimDistance = 256.f;
		Vector const forward(camera_p.getLocalFrameK_p());
		Vector forwardNorm(forward);
		if (forwardNorm.normalize())
		{
			Vector const aimEnd(camera_p.getPosition_p() + forwardNorm * s_aimDistance);
			trackChildTurretTowardAim(turret, aimEnd, elapsedTime);
			GroundScene *const groundScene = dynamic_cast<GroundScene *>(Game::getScene());
			if (playerCreature && groundScene)
				updateAimWorldPoint(groundScene, playerCreature, aimEnd, elapsedTime);
		}

		setTransform_o2p(camera_p);
	CellProperty::setPortalTransitionsEnabled(true);

	float alterResult = GameCamera::alter(elapsedTime);
	AlterResult::incorporateAlterResult(alterResult, AlterResult::cms_alterNextFrame);
	return alterResult;
}
