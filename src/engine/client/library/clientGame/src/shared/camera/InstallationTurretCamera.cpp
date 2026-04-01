// ======================================================================
//
// InstallationTurretCamera.cpp
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/InstallationTurretCamera.h"

#include "clientGraphics/Graphics.h"

#include "clientGame/CreatureObject.h"
#include "clientGame/Game.h"
#include "clientGame/GroundScene.h"
#include "clientGame/InstallationObject.h"
#include "clientGame/TurretObject.h"
#include "sharedFoundation/GameControllerMessage.h"
#include "sharedFoundation/NetworkId.h"
#include "sharedFoundation/MessageQueue.h"
#include "clientGame/ClientWorld.h"
#include "sharedCollision/CollideParameters.h"
#include "sharedCollision/CollisionInfo.h"
#include "sharedObject/AlterResult.h"
#include "sharedObject/CellProperty.h"
#include "sharedFoundation/FloatMath.h"
#include "sharedFoundation/Misc.h"
#include "sharedMath/Transform.h"
#include "sharedMath/Vector.h"

namespace InstallationTurretCameraNamespace
{
	// 179° total vertical arc => ±89.5° from level.
	float const cs_pitchLimitRad = 89.5f * PI_OVER_180;
	float const cs_defaultEyeY = 1.6f;
	// Zoom: level 0 = at barrel; wheel steps out to 10 m (third person).
	int const cs_zoomLevelMax = 10;
	float const cs_maxPullBackM = 10.f;

	bool findInstallationTurretParts(Object const *const installationObj, TurretObject *&outTurretObject, Object const *&outBarrel)
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
				outBarrel = turretObject->getBarrelObject();
				return true;
			}
		}
		return false;
	}

	void trackChildTurretTowardAim(TurretObject *const turretObject, Vector const &aimWorldEnd, float const elapsedTime)
	{
		if (!turretObject || elapsedTime <= 0.f)
			return;

		turretObject->deferGunnerAimToward(aimWorldEnd, elapsedTime);
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
	m_lastQueuedAimWorld(),
	m_zoomSetting(0),
	m_smoothedPullBackM(0.f)
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
		m_zoomSetting = 0;
		m_smoothedPullBackM = 0.f;
	}
	GameCamera::setActive(active);
}

void InstallationTurretCamera::setMessageQueue(MessageQueue const *const queue)
{
	m_queue = queue;
}

bool InstallationTurretCamera::computeReticleAimWorldEnd(float const distanceMeters, Vector &outEnd_w) const
{
	Vector const origin_w(getPosition_w());
	// HUD crosshair is typically at full render-target center; reverseProject accounts for camera viewport inset.
	int const sx = Graphics::getCurrentRenderTargetWidth() / 2;
	int const sy = Graphics::getCurrentRenderTargetHeight() / 2;
	Vector dir_w(rotate_o2w(reverseProjectInScreenSpace(sx, sy)));
	if (!dir_w.normalize())
	{
		return false;
	}
	outEnd_w = origin_w + dir_w * distanceMeters;
	return true;
}

void InstallationTurretCamera::setTurret(Object const *const turret)
{
	if (m_turret.getPointer() != turret)
	{
		m_yaw = 0.f;
		m_pitch = 0.f;
		m_hasLastQueuedAim = false;
		m_aimSendCooldown = 0.f;
		m_zoomSetting = 0;
		m_smoothedPullBackM = 0.f;
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
		case CM_cameraZoom:
		case CM_mouseWheel:
			if (value > 0.f)
				m_zoomSetting = std::max(m_zoomSetting - 1, 0);
			else
				m_zoomSetting = std::min(m_zoomSetting + 1, cs_zoomLevelMax);
			break;
		default:
			break;
		}
	}

	// Unbounded yaw (full horizontal); avoid [0,2pi) wrap — discontinuities read as spin near the seam.
	m_pitch = clamp(-cs_pitchLimitRad, m_pitch, cs_pitchLimitRad);

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
		Object const *barrel = 0;
		Transform camera_p(Transform::IF_none);

		// Scope orientation = user yaw/pitch in *installation* space only. Do not multiply look after the
		// live turret/barrel chain — that frame slews to track aim and causes drift, gimbal spin when
		// pitching, and reticle slide. Eye position still follows the articulated muzzle path.
		Transform look(Transform::identity);
		look.yaw_l(m_yaw);
		look.pitch_l(m_pitch);
		camera_p.multiply(install_o2p, look);

		if (findInstallationTurretParts(turret, turretObject, barrel) && barrel)
		{
			Transform const turretObj_o2i(turretObject->getTransform_o2p());
			Transform const barrel_o2t(barrel->getTransform_o2p());
			Transform barrelChain(Transform::IF_none);
			barrelChain.multiply(turretObj_o2i, barrel_o2t);
			Transform upToEye(Transform::IF_none);
			upToEye.multiply(barrelChain, eye_o2t);
			Transform eyeInParent(Transform::IF_none);
			eyeInParent.multiply(install_o2p, upToEye);
			camera_p.setPosition_p(eyeInParent.getPosition_p());
		}
		else
		{
			Transform eyeInParent(Transform::IF_none);
			eyeInParent.multiply(install_o2p, eye_o2t);
			camera_p.setPosition_p(eyeInParent.getPosition_p());
		}

		setTransform_o2p(camera_p);

		float const targetPullM = clamp(0.f, static_cast<float>(m_zoomSetting), cs_maxPullBackM);
		m_smoothedPullBackM = linearInterpolate(m_smoothedPullBackM, targetPullM, std::min(1.f, elapsedTime * 12.f));

		Vector const start_w(getPosition_w());
		Vector forward_w(getObjectFrameK_w());
		if (forward_w.normalize() && m_smoothedPullBackM > 0.001f)
		{
			Vector const end_w(start_w - forward_w * m_smoothedPullBackM);
			float pullM = m_smoothedPullBackM;
			CollisionInfo collisionResult;
			Object const *const excludeObj = m_turret.getPointer();
			if (ClientWorld::collide(getParentCell(), start_w, end_w, CollideParameters::cms_default, collisionResult, ClientWorld::CF_allCamera, excludeObj))
			{
				float const lineDistance = start_w.magnitudeBetween(end_w);
				if (lineDistance > 0.001f)
				{
					float const t = clamp(0.f, (start_w.magnitudeBetween(collisionResult.getPoint()) / lineDistance) - (0.25f / lineDistance), 1.f);
					pullM = Vector::linearInterpolate(start_w, end_w, t).magnitudeBetween(start_w);
				}
			}
			setPosition_w(start_w - forward_w * pullM);
		}

		// Aim through reticle (screen center); matches crosshair rather than raw camera +K (fixes vertical offset).
		static float const s_aimDistance = 256.f;
		Vector aimEnd;
		if (computeReticleAimWorldEnd(s_aimDistance, aimEnd))
		{
			if (turretObject)
				trackChildTurretTowardAim(turretObject, aimEnd, elapsedTime);
			GroundScene *const groundScene = dynamic_cast<GroundScene *>(Game::getScene());
			if (playerCreature && groundScene)
				updateAimWorldPoint(groundScene, playerCreature, aimEnd, elapsedTime);
		}

	CellProperty::setPortalTransitionsEnabled(true);

	float alterResult = GameCamera::alter(elapsedTime);
	AlterResult::incorporateAlterResult(alterResult, AlterResult::cms_alterNextFrame);
	return alterResult;
}
