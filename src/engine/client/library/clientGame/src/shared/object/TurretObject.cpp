//===================================================================
//
// TurretObject.cpp
// asommers
//
// copyright 2002, sony online entertainment
//
//===================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/TurretObject.h"

#include "clientGame/ClientWeaponObjectTemplate.h"
#include "clientGame/Game.h"
#include "clientGame/Projectile.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedFoundation/FloatMath.h"
#include "sharedMath/Transform.h"
#include "sharedObject/AlterResult.h"

#include <float.h>

//===================================================================
// TurretObjectNamespace
//===================================================================

namespace TurretObjectNamespace
{
	bool ms_debugFire;

	bool ts_finiteVec(Vector const &v)
	{
		return _finite(static_cast<double>(v.x)) != 0 && _finite(static_cast<double>(v.y)) != 0 && _finite(static_cast<double>(v.z)) != 0;
	}

	bool ts_finiteTransform(Transform const &t)
	{
		Transform::matrix_t const &m = t.getMatrix();
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 4; ++c)
				if (!_finite(static_cast<double>(m[r][c])))
					return false;
		return true;
	}
}

using namespace TurretObjectNamespace;

//===================================================================
// STATIC PUBLIC TurretObject
//===================================================================

void TurretObject::install ()
{
	InstallTimer const installTimer("TurretObject::install");

#ifdef _DEBUG
	DebugFlags::registerFlag (ms_debugFire, "ClientGame/TurretObject", "debugFire");
#endif

	ExitChain::add (remove, "TurretObject::remove");
}

//-------------------------------------------------------------------

void TurretObject::remove ()
{
#ifdef _DEBUG
	DebugFlags::unregisterFlag (ms_debugFire);
#endif
}

//===================================================================
// PUBLIC TurretObject
//===================================================================

TurretObject::TurretObject (const float yawMaximumRadiansPerSecond) :
	Object (),
	m_yawMaximumRadiansPerSecond (yawMaximumRadiansPerSecond),
	m_barrel (0),
	m_pitchMinimumRadians (0.f),
	m_pitchMaximumRadians (0.f),
	m_pitchMaximumRadiansPerSecond (0.f),
	m_weaponObjectTemplate (0),
	m_muzzleTransform_o2p (),
	m_speed (0.f),
	m_expirationTime (0.f),
	m_projectile (0),
	m_target (0),
	m_debugFireTimer (2.f),
	m_hasDeferredGunnerAim (false),
	m_deferredGunnerAimWorld (Vector::zero),
	m_deferredGunnerAimElapsed (0.f)
{
}

//-------------------------------------------------------------------

TurretObject::~TurretObject ()
{
	delete m_barrel;
	m_barrel = 0;

	if (m_weaponObjectTemplate)
	{
		m_weaponObjectTemplate->releaseReference ();
		m_weaponObjectTemplate = 0;
	}

	m_projectile = 0;
}

//-------------------------------------------------------------------

void TurretObject::setBarrel (Object* const object, const float pitchMinimumRadians, const float pitchMaximumRadians, const float pitchMaximumRadiansPerSecond)
{
	if (m_barrel)
		delete m_barrel;

	m_barrel = object;
	m_pitchMinimumRadians = pitchMinimumRadians;
	m_pitchMaximumRadians = pitchMaximumRadians;
	m_pitchMaximumRadiansPerSecond = pitchMaximumRadiansPerSecond;
}

//-------------------------------------------------------------------

void TurretObject::setWeapon (const ClientWeaponObjectTemplate* const weaponObjectTemplate, const Transform& muzzleTransform_o2p, const float speed, const float expirationTime)
{
	m_weaponObjectTemplate = weaponObjectTemplate;
	if (m_weaponObjectTemplate)
		m_weaponObjectTemplate->addReference ();

	m_muzzleTransform_o2p = muzzleTransform_o2p;
	m_speed = speed;
	m_expirationTime = expirationTime;
}

//-------------------------------------------------------------------

const Object* TurretObject::fire (const Object* const target, const bool hit)
{
	m_target = target;

	if (m_projectile)
	{
		delete m_projectile;
		m_projectile = 0;
	}

	if (m_weaponObjectTemplate)
		m_projectile = m_weaponObjectTemplate->createProjectile (!hit, false);

	return m_projectile;
}

//-------------------------------------------------------------------

bool TurretObject::getMuzzleTransform_o2Installation(Transform &out) const
{
	if (!m_barrel)
		return false;

	Transform muzzle_o2turret;
	muzzle_o2turret.multiply(m_barrel->getTransform_o2p(), m_muzzleTransform_o2p);
	out.multiply(getTransform_o2p(), muzzle_o2turret);
	return true;
}

//-------------------------------------------------------------------

float TurretObject::getWeaponProjectileSpeed() const
{
	return m_speed;
}

//-------------------------------------------------------------------

float TurretObject::getWeaponProjectileExpireTime() const
{
	return m_expirationTime;
}

//-------------------------------------------------------------------

void TurretObject::deferGunnerAimToward(Vector const &worldPosition, float const elapsedTime)
{
	if (elapsedTime <= 0.f || !ts_finiteVec(worldPosition))
		return;

	m_hasDeferredGunnerAim = true;
	m_deferredGunnerAimWorld = worldPosition;
	m_deferredGunnerAimElapsed = elapsedTime;
}

//-------------------------------------------------------------------

Object const *TurretObject::getBarrelObject() const
{
	return m_barrel;
}

//-------------------------------------------------------------------

void TurretObject::trackTowardWorldPosition(Vector const &worldPosition, float const elapsedTime, float const rateScale)
{
	if (elapsedTime <= 0.f)
		return;

	if (m_projectile)
		return;

	if (!ts_finiteVec(worldPosition) || !ts_finiteTransform(getTransform_o2p()))
		return;

	float const scale = rateScale < 1.f ? 1.f : rateScale;
	float yawMax = m_yawMaximumRadiansPerSecond * scale * elapsedTime;
	float pitchMax = m_pitchMaximumRadiansPerSecond * scale * elapsedTime;
	// Some .cdf turrets use 0 deg/s in data; still allow responsive gunner aiming.
	if (yawMax < 1e-6f)
		yawMax = convertDegreesToRadians(720.f) * elapsedTime;
	if (pitchMax < 1e-6f)
		pitchMax = convertDegreesToRadians(720.f) * elapsedTime;

	{
		Vector const facing_o = rotateTranslate_w2o(worldPosition);
		if (facing_o != Vector::zero)
		{
			float const yaw = clamp(-yawMax, facing_o.theta(), yawMax);
			if (_finite(static_cast<double>(yaw)) != 0)
				yaw_o(yaw);
		}
	}

	if (m_barrel && ts_finiteTransform(m_barrel->getTransform_o2p()))
	{
		Vector const facing_o = m_barrel->rotateTranslate_w2o(worldPosition);
		if (facing_o != Vector::zero)
		{
			float const pitch = clamp(-pitchMax, facing_o.phi(), pitchMax);
			if (_finite(static_cast<double>(pitch)) != 0)
				m_barrel->pitch_o(pitch);
		}
	}
}

//-------------------------------------------------------------------

float TurretObject::alter (float elapsedTime)
{
	if (m_hasDeferredGunnerAim)
	{
		Vector const aimWorld = m_deferredGunnerAimWorld;
		float const aimElapsed = m_deferredGunnerAimElapsed;
		m_hasDeferredGunnerAim = false;
		trackTowardWorldPosition(aimWorld, aimElapsed);
	}

	const float result = Object::alter (elapsedTime);
	if (result == AlterResult::cms_kill) //lint !e777 // Testing floats for equality // It's okay, we're using constants.
		return AlterResult::cms_kill;

	if (m_projectile)
	{
		if (!m_target)
		{
			delete m_projectile;
			m_projectile = 0;
		}
		else
		{
			const Vector targetPosition_w = m_target->getAppearance () ? m_target->getPosition_w () + Vector::unitY * m_target->getAppearanceSphereRadius () : m_target->getPosition_w ();

			//-- turret face target
			{
				const Vector facing_o = rotateTranslate_w2o (targetPosition_w);
				if (facing_o != Vector::zero)
				{
					const float maximumYawThisFrame = m_yawMaximumRadiansPerSecond * elapsedTime;
					const float yaw = clamp (-maximumYawThisFrame, facing_o.theta (), maximumYawThisFrame);
					yaw_o (yaw);
				}
			}

			//-- barrel face target
			{
				if (m_barrel)
				{
					//-- barrel face target
					{
						const Vector facing_o = m_barrel->rotateTranslate_w2o (targetPosition_w);
						if (facing_o != Vector::zero)
						{
							const float maximumPitchThisFrame = m_pitchMaximumRadiansPerSecond * elapsedTime;
							const float pitch = clamp (-maximumPitchThisFrame, facing_o.phi (), maximumPitchThisFrame);
							m_barrel->pitch_o (pitch);
						}
					}

					//-- are we ready to fire?
					{
						bool shouldFire = false;

						Vector facing_o = m_barrel->rotateTranslate_w2o (targetPosition_w);
						if (facing_o == Vector::zero)
							shouldFire = true;
						else
						{
							if (!facing_o.normalize ())
								shouldFire = true;
							else
								if (facing_o.dot (Vector::unitZ) > 0.99f)
									shouldFire = true;
						}

						if (shouldFire)
						{
							Transform muzzleHardpoint;
							muzzleHardpoint.multiply (m_barrel->getTransform_o2w (), m_muzzleTransform_o2p);

							m_projectile->setExpirationTime (m_expirationTime);
							m_projectile->setFacing (getParentCell (), muzzleHardpoint.getPosition_p (), m_target->getPosition_w ());
							m_projectile->setSpeed (m_speed);
							m_projectile->setTarget (m_target);
							m_projectile->addToWorld ();
							m_projectile = 0;
						}
					}
				}
			}
		}
	}
	else
	{
#ifdef _DEBUG
		if (ms_debugFire)
		{
			if (m_debugFireTimer.updateZero (elapsedTime))
				fire (Game::getPlayer (), Random::random (1) == 0);
		}
#endif
	}

	return AlterResult::cms_alterNextFrame;
}

//===================================================================

