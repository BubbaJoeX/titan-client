//========================================================================
//
// TangibleObject.h
//
// Copyright 2001 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
//========================================================================

#ifndef INCLUDED_TangibleObject_H
#define INCLUDED_TangibleObject_H

// ======================================================================

#include "Archive/AutoDeltaMap.h"
#include "Archive/AutoDeltaSet.h"
#include "Archive/AutoDeltaVariableCallback.h"
#include "Archive/AutoDeltaVector.h"
#include "clientGame/ClientObject.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/NetworkIdArchive.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Watcher.h"
#include "sharedGame/SharedTangibleObjectTemplate.h"
#include "sharedMathArchive/VectorArchive.h"
#include "sharedObject/CachedNetworkId.h"
#include "sharedObject/CachedNetworkIdArchive.h"
#include "sharedUtility/CallbackReceiver.h"

class CollisionProperty;
class CustomizationData;
class Footprint;
class InteriorLayoutReaderWriter;
class ManufactureSchematicObject;
class NetworkId;
class ParticleEffectAppearance;
class Player;
class SlotDescriptor;
class SharedTangibleObjectTemplate;
class SlotId;
class Texture;
class Vector;

// ======================================================================
/**
  *  A TangibleObject is an object that has a physical representation in the world.  It has physical properties such
  *  as weight and volume.  Most everything that a character interacts with is a tangible item.
  */

class TangibleObject : public ClientObject, public CallbackReceiver
{
	friend class TangibleController;

public:

// these MUST be reflected in: 
// //depot/swg/current/dsrc/sku.0/sys.server/compiled/game/object/tangible_object_template.tdf
// //depot/swg/current/dsrc/sku.0/sys.server/compiled/game/script/base_class.java
// //depot/swg/current/src/engine/client/library/clientGame/src/shared/object/TangibleObject.h
// //depot/swg/current/src/engine/server/library/serverGame/src/shared/object/TangibleObject.h
	enum Conditions
	{
		C_onOff                = 0x00000001,
		C_vendor               = 0x00000002,
		C_insured              = 0x00000004,
		C_conversable          = 0x00000008,
		C_hibernating          = 0x00000010,
		C_magicItem            = 0x00000020,
		C_aggressive           = 0x00000040,
		C_wantSawAttackTrigger = 0x00000080,
		C_invulnerable         = 0x00000100,
		C_disabled             = 0x00000200,
		C_uninsurable          = 0x00000400,
		C_interesting          = 0x00000800,
		C_mount                = 0x00001000,    //   Set programmatically by mount system.  Do not set this in the template.
		C_crafted              = 0x00002000,    //   Set programmatically by crafting system.  Do not set this in the template.
		C_wingsOpened          = 0x00004000,    //   Set programmatically by wing system.  Do not set this in the template.
		C_spaceInteresting     = 0x00008000,
		C_docking              = 0x00010000,    //   Set programmatically by docking system.  Do not set this in the template.
		C_destroying           = 0x00020000,    //   Set programmatically by ship system.  Do not set this in the template.
		C_commable             = 0x00040000,
		C_dockable             = 0x00080000,
		C_eject                = 0x00100000,
		C_inspectable          = 0x00200000,
		C_transferable         = 0x00400000,
		C_inflightTutorial     = 0x00800000,
		C_spaceCombatMusic     = 0x01000000,    //   Set programmatically by the AI system.  Do not set this in the template.
		C_encounterLocked      = 0x02000000,
		C_spawnedCreature      = 0x04000000,
		C_holidayInteresting   = 0x08000000,
		C_locked               = 0x10000000,
		C_magicPaintingUrl     = 0x20000000,
		C_magicVideoPlayer     = 0x40000000,
	};

public:

	struct Messages
	{
		struct AppearanceData;
		struct RemoteTextureUrl;
		struct RemoteTextureMode;
		struct RemoteTextureDisplayMode;
		struct RemoteTextureScrollH;
		struct RemoteTextureScrollV;
		struct RemoteStreamUrl;
		struct RemoteStreamTimestamp;
		struct RemoteStreamLoop;
		struct RemoteStreamAspect;
		struct RemoteStreamStartTime;
		struct RemoteEmitterParentId;
		struct RemoteEmitterVolume;
		struct DamageTaken
		{
			typedef std::pair<TangibleObject *, int> Payload;
		};
		struct ConditionModified;
		struct MaxHitPointsModified;
	};

	typedef stdvector<CachedNetworkId>::fwd TargetIdList;

public:

	static void            install                           ();
	static TangibleObject *findClosestInRangeChair(NetworkId const &searchCellId, Vector const &searchPosition_w);

public:

	explicit            TangibleObject                    (const SharedTangibleObjectTemplate* newTemplate);
	virtual            ~TangibleObject                    ();

	static TangibleObject * getTangibleObject(NetworkId const & networkId);
	static TangibleObject const * asTangibleObject(Object const * object);
	static TangibleObject * asTangibleObject(Object * object);

	virtual TangibleObject *       asTangibleObject();
	virtual TangibleObject const * asTangibleObject() const;

	virtual float       alter(float elapsedTime);
	virtual void        doPostContainerChangeProcessing   ();

	virtual void        endBaselines                      ();

	virtual void        addToWorld                        ();
	virtual void        removeFromWorld                   ();

	// access functions
	void                changeAppearance                  (const char * objectTemplateName);
	void                changeAppearance                  (const SharedTangibleObjectTemplate & objectTemplate);
	const std::string & getAppearanceData                 () const;
	void                setAppearanceData                 (const std::string & newAppearanceData);

	void				setUntargettableOverride		  (bool untargettable) const;
	virtual bool        isTargettable                     () const;
	bool                isVisible                         () const;
	bool                isInvulnerable                    () const;
	int                 getCount                          (void) const;
	int                 getCondition                      (void) const;
	bool                hasCondition                      (int condition) const;
	std::string const & getRemoteStreamAspect             () const;

	static bool         getVideoPlaybackInfo              (TangibleObject const * obj, __int64 & outTimeMs, __int64 & outLengthMs);
	static bool         seekVideoPlayback                 (TangibleObject const * obj, __int64 timeMs);

	bool isInCombat() const;
	void setIsInCombat(bool inCombat);

	//-- client-side testing only
	void                setCount                          (int count);
	void                setCondition                      (int condition);
	void                clearCondition                    (int condition);

	// @todo move to ClientObject
	void                         setSlotInfo              (const std::string &slotDescriptorName, const std::string &arrangementDescriptorName) const;

	bool                               isCrafted               (void) const;
	const ManufactureSchematicObject * getCraftingManfSchematic(void) const;
	void                               startCrafting           (void);
	void                               stopCrafting            (void);

	CustomizationData *          fetchCustomizationData   ();
	const CustomizationData *    fetchCustomizationData   () const;

	int                          getDamageTaken           () const;
	int                          getMaxHitPoints          () const;
	void                         clientSetDamageTaken     (int);
	void                         clientSetMaxHitPoints    (int);

	virtual uint32               getPvpFlags              () const;
	virtual int                  getPvpType               () const;
	virtual uint32               getPvpFaction            () const;
	void                         setPvpFlags              (uint32 flags);

	bool                         isPlayer                 () const;
	bool                         isAttackable             () const;
	bool                         canAttackPlayer          () const;
	bool                         isEnemy                  () const;
	bool                         isSameFaction            (const TangibleObject & other) const;
	bool                         canHelp                  () const;

	bool                         canSitOn                 () const;
	Unicode::String              buildChairParameterString() const;

	Footprint                   *getFootprint             ();

	float                        getDamageLevel () const;
	void                         setTestDamageLevel (float testDamageLevel);

	void                         getRequiredCertifications(stdvector<std::string>::fwd & results) const;

	uint32                       getOverriddenMapColor() const;

	virtual InteriorLayoutReaderWriter const * getInteriorLayout() const;
	void addClientOnlyInteriorLayoutObject(Object * object);

	bool isPassiveRevealPlayerCharacter(NetworkId const & target) const;

	virtual void getObjectInfo(stdmap<std::string, stdmap<std::string, Unicode::String>::fwd >::fwd & propertyMap) const;

protected:

	const stdset<int>::fwd&      getComponents () const;
	virtual void                 conditionModified        (int oldCondition, int newCondition);
	virtual void                 maxHitPointsModified     (int oldMaxHitPoints, int newMaxHitPoints);
	virtual void                 filterLocalizedName      (Unicode::String & localizedName) const;

private:
	const SlotDescriptor        *getSlotDescriptor        () const;
	const ArrangementDescriptor *getArrangementDescriptor () const;

	struct Callbacks
	{
		template <typename S, typename U> struct DefaultCallback
		{
			void modified (TangibleObject & target, const U & old, const U & value, bool local) const;
		};

		typedef DefaultCallback<Messages::DamageTaken,    int>              DamageTaken;
		typedef DefaultCallback<Messages::AppearanceData, std::string>      AppearanceData;
		typedef DefaultCallback<Messages::RemoteTextureUrl, std::string>    RemoteTextureUrl;
		typedef DefaultCallback<Messages::RemoteTextureMode, std::string>   RemoteTextureMode;
		typedef DefaultCallback<Messages::RemoteTextureDisplayMode, std::string> RemoteTextureDisplayMode;
		typedef DefaultCallback<Messages::RemoteTextureScrollH, std::string> RemoteTextureScrollH;
		typedef DefaultCallback<Messages::RemoteTextureScrollV, std::string> RemoteTextureScrollV;
		typedef DefaultCallback<Messages::RemoteStreamUrl, std::string>     RemoteStreamUrl;
		typedef DefaultCallback<Messages::RemoteStreamTimestamp, std::string> RemoteStreamTimestamp;
		typedef DefaultCallback<Messages::RemoteStreamLoop, std::string>     RemoteStreamLoop;
		typedef DefaultCallback<Messages::RemoteStreamAspect, std::string>   RemoteStreamAspect;
		typedef DefaultCallback<Messages::RemoteStreamStartTime, std::string> RemoteStreamStartTime;
		typedef DefaultCallback<Messages::RemoteEmitterParentId, std::string> RemoteEmitterParentId;
		typedef DefaultCallback<Messages::RemoteEmitterVolume, std::string> RemoteEmitterVolume;
		typedef DefaultCallback<Messages::ConditionModified, int>           ConditionModified;
		typedef DefaultCallback<Messages::MaxHitPointsModified, int>        MaxHitPointsModified;
	};

	friend Callbacks::AppearanceData;
	friend Callbacks::RemoteTextureUrl;
	friend Callbacks::RemoteTextureMode;
	friend Callbacks::RemoteTextureDisplayMode;
	friend Callbacks::RemoteTextureScrollH;
	friend Callbacks::RemoteTextureScrollV;
	friend Callbacks::RemoteStreamUrl;
	friend Callbacks::RemoteStreamTimestamp;
	friend Callbacks::RemoteStreamLoop;
	friend Callbacks::RemoteStreamAspect;
	friend Callbacks::RemoteStreamStartTime;
	friend Callbacks::RemoteEmitterParentId;
	friend Callbacks::RemoteEmitterVolume;
	friend Callbacks::ConditionModified;
	friend Callbacks::MaxHitPointsModified;

private:

	void                          handleConditionModified(int oldCondition, int newCondition);

	void                          setChildWingsOpened(bool wingsOpened);

	void                          appearanceDataModified(const std::string & value);
	void                          remoteTextureUrlModified(const std::string & value);
	void                          remoteTextureModeModified(const std::string & value);
	void                          remoteTextureDisplayModeModified(const std::string & value);
	void                          remoteTextureScrollHModified(const std::string & value);
	void                          remoteTextureScrollVModified(const std::string & value);
	void                          updateRemoteImageTexture();
	void                          updateGifAnimation(float elapsedTime);
	void                          clearRemoteImageTexture();

	void                          remoteStreamUrlModified(const std::string & value);
	void                          remoteStreamTimestampModified(const std::string & value);
	void                          remoteStreamLoopModified(const std::string & value);
	void                          remoteStreamAspectModified(const std::string & value);
	void                          remoteStreamStartTimeModified(const std::string & value);
	void                          remoteEmitterParentIdModified(const std::string & value);
	void                          remoteEmitterVolumeModified(const std::string & value);
	void                          updateRemoteVideoStream();
	void                          clearRemoteVideoStream();
	void                          updateVideoEmitterAudio();
	void                          clearVideoEmitterAudio();

	void                          updateInterestingAttachedObject(int objectCondition);

	void                          performCallback();

	// disabled
	                              TangibleObject          ();
	                              TangibleObject          (const TangibleObject& rhs);
	TangibleObject &              operator=               (const TangibleObject& rhs);

	void						  AddObjectEffect(std::string const & label, std::string const & effectFile, std::string const & hardpoint, Vector const & offset, float const scale);
	void						  RemoveObjectEffect(std::string const & label);
	void						  VerifyObjectEffects();

	void						  OnObjectEffectInsert(std::string const & key, std::pair<std::string, std::pair<std::string, std::pair<Vector, float> > > const & value);
	void						  OnObjectEffectErased(std::string const & key, std::pair<std::string, std::pair<std::string, std::pair<Vector, float> > > const & value);
	void						  OnObjectEffectModified(std::string const & key, std::pair<std::string, std::pair<std::string, std::pair<Vector, float> > > const & oldValue, std::pair<std::string, std::pair<std::string, std::pair<Vector, float> > > const & newValue);

private:

	class CraftingToolSyncUi;

	Archive::AutoDeltaVariableCallback<std::string, Callbacks::AppearanceData, TangibleObject> m_appearanceData;
	Archive::AutoDeltaVariableCallback<std::string, Callbacks::RemoteTextureUrl, TangibleObject> m_remoteTextureUrl;
	Archive::AutoDeltaVariableCallback<std::string, Callbacks::RemoteTextureMode, TangibleObject> m_remoteTextureMode;
	Archive::AutoDeltaVariableCallback<std::string, Callbacks::RemoteTextureDisplayMode, TangibleObject> m_remoteTextureDisplayMode;
	Archive::AutoDeltaVariableCallback<std::string, Callbacks::RemoteTextureScrollH, TangibleObject> m_remoteTextureScrollH;
	Archive::AutoDeltaVariableCallback<std::string, Callbacks::RemoteTextureScrollV, TangibleObject> m_remoteTextureScrollV;
	Archive::AutoDeltaVariableCallback<std::string, Callbacks::RemoteStreamUrl, TangibleObject> m_remoteStreamUrl;
	Archive::AutoDeltaVariableCallback<std::string, Callbacks::RemoteStreamTimestamp, TangibleObject> m_remoteStreamTimestamp;
	Archive::AutoDeltaVariableCallback<std::string, Callbacks::RemoteStreamLoop, TangibleObject> m_remoteStreamLoop;
	Archive::AutoDeltaVariableCallback<std::string, Callbacks::RemoteStreamAspect, TangibleObject> m_remoteStreamAspect;
	Archive::AutoDeltaVariableCallback<std::string, Callbacks::RemoteStreamStartTime, TangibleObject> m_remoteStreamStartTime;
	Archive::AutoDeltaVariableCallback<std::string, Callbacks::RemoteEmitterParentId, TangibleObject> m_remoteEmitterParentId;
	Archive::AutoDeltaVariableCallback<std::string, Callbacks::RemoteEmitterVolume, TangibleObject> m_remoteEmitterVolume;
	Archive::AutoDeltaVariableCallback<int,         Callbacks::DamageTaken, TangibleObject>    m_damageTaken;
	Archive::AutoDeltaVariableCallback<int, Callbacks::MaxHitPointsModified, TangibleObject>   m_maxHitPoints;

	Archive::AutoDeltaSet<int>                m_components;      // component table ids of visible components attached to this object

	Archive::AutoDeltaVariable<boolean>       m_visible;
	Archive::AutoDeltaVariable<boolean>		  m_inCombat;
	Archive::AutoDeltaVariable<int>           m_count;         // generic counter
	Archive::AutoDeltaVariableCallback<int, Callbacks::ConditionModified, TangibleObject> m_condition;     // object condition bit flags

	int16                                     m_accumulatedDamageTaken;
	uint32                                    m_pvpFlags;
	Archive::AutoDeltaVariable<int>           m_pvpType;
	Archive::AutoDeltaVariable<uint32>        m_pvpFactionId;

	float                                     m_lastDamageLevel;
	float                                     m_testDamageLevel;
	bool                                      m_lastOnOffStatus : 1;
	bool                                      m_isReceivingCallbacks : 1;
	Watcher<Object>                           m_interestingAttachedObject;
	mutable bool                              m_untargettableOverride : 1;

	typedef stdvector<Watcher<Object> >::fwd ClientOnlyInteriorLayoutObjectList;
	ClientOnlyInteriorLayoutObjectList * m_clientOnlyInteriorLayoutObjectList;

	typedef SharedTangibleObjectTemplate::ClientVisabilityFlags VisabilityFlag;
	VisabilityFlag m_visabilityFlag;

	std::map<std::string, Object *> m_objectEffects;

	// tracks player character objects that have passively revealed this object
	Archive::AutoDeltaSet<NetworkId> m_passiveRevealPlayerCharacter;

	Archive::AutoDeltaVariable<uint32>        m_mapColorOverride;

	// Access list of users if the item is condition locked.
	Archive::AutoDeltaSet<NetworkId> m_accessList;
	Archive::AutoDeltaSet<int>       m_guildAccessList;

	// The map of particle effects that should be continually playing on this object. Label, Appearance file, hardpoint, offset, scale
	Archive::AutoDeltaMap<std::string, std::pair<std::string, std::pair<std::string, std::pair<Vector, float> > >, TangibleObject> m_effectsMap;
};

// ======================================================================

inline void TangibleObject::Callbacks::AppearanceData::modified(TangibleObject & target, const std::string& oldValue, const std::string & value, bool isLocal) const
{
	UNREF(oldValue);
	UNREF(isLocal);
	target.appearanceDataModified(value);
}

//----------------------------------------------------------------------

inline void TangibleObject::Callbacks::RemoteTextureUrl::modified(TangibleObject & target, const std::string & oldValue, const std::string & value, bool isLocal) const
{
	UNREF(oldValue);
	UNREF(isLocal);
	target.remoteTextureUrlModified(value);
}

//----------------------------------------------------------------------

inline void TangibleObject::Callbacks::RemoteTextureMode::modified(TangibleObject & target, const std::string & oldValue, const std::string & value, bool isLocal) const
{
	UNREF(oldValue);
	UNREF(isLocal);
	target.remoteTextureModeModified(value);
}

//----------------------------------------------------------------------

inline void TangibleObject::Callbacks::RemoteTextureDisplayMode::modified(TangibleObject & target, const std::string & oldValue, const std::string & value, bool isLocal) const
{
	UNREF(oldValue);
	UNREF(isLocal);
	target.remoteTextureDisplayModeModified(value);
}

//----------------------------------------------------------------------

inline void TangibleObject::Callbacks::RemoteTextureScrollH::modified(TangibleObject & target, const std::string & oldValue, const std::string & value, bool isLocal) const
{
	UNREF(oldValue);
	UNREF(isLocal);
	target.remoteTextureScrollHModified(value);
}

//----------------------------------------------------------------------

inline void TangibleObject::Callbacks::RemoteTextureScrollV::modified(TangibleObject & target, const std::string & oldValue, const std::string & value, bool isLocal) const
{
	UNREF(oldValue);
	UNREF(isLocal);
	target.remoteTextureScrollVModified(value);
}

//----------------------------------------------------------------------

inline void TangibleObject::Callbacks::RemoteStreamUrl::modified(TangibleObject & target, const std::string & oldValue, const std::string & value, bool isLocal) const
{
	UNREF(oldValue);
	UNREF(isLocal);
	target.remoteStreamUrlModified(value);
}

//----------------------------------------------------------------------

inline void TangibleObject::Callbacks::RemoteStreamTimestamp::modified(TangibleObject & target, const std::string & oldValue, const std::string & value, bool isLocal) const
{
	UNREF(oldValue);
	UNREF(isLocal);
	target.remoteStreamTimestampModified(value);
}

//----------------------------------------------------------------------

inline void TangibleObject::Callbacks::RemoteStreamLoop::modified(TangibleObject & target, const std::string & oldValue, const std::string & value, bool isLocal) const
{
	UNREF(oldValue);
	UNREF(isLocal);
	target.remoteStreamLoopModified(value);
}

//----------------------------------------------------------------------

inline void TangibleObject::Callbacks::RemoteStreamAspect::modified(TangibleObject & target, const std::string & oldValue, const std::string & value, bool isLocal) const
{
	UNREF(oldValue);
	UNREF(isLocal);
	target.remoteStreamAspectModified(value);
}

//----------------------------------------------------------------------

inline void TangibleObject::Callbacks::RemoteStreamStartTime::modified(TangibleObject & target, const std::string & oldValue, const std::string & value, bool isLocal) const
{
	UNREF(oldValue);
	UNREF(isLocal);
	target.remoteStreamStartTimeModified(value);
}

//----------------------------------------------------------------------

inline void TangibleObject::Callbacks::RemoteEmitterParentId::modified(TangibleObject & target, const std::string & oldValue, const std::string & value, bool isLocal) const
{
	UNREF(oldValue);
	UNREF(isLocal);
	target.remoteEmitterParentIdModified(value);
}

//----------------------------------------------------------------------

inline void TangibleObject::Callbacks::RemoteEmitterVolume::modified(TangibleObject & target, const std::string & oldValue, const std::string & value, bool isLocal) const
{
	UNREF(oldValue);
	UNREF(isLocal);
	target.remoteEmitterVolumeModified(value);
}

//----------------------------------------------------------------------

inline void TangibleObject::Callbacks::ConditionModified::modified (TangibleObject& target, const int& oldValue, const int& newValue, const bool ) const
{
	target.conditionModified (oldValue, newValue);
}

//----------------------------------------------------------------------

inline void TangibleObject::Callbacks::MaxHitPointsModified::modified (TangibleObject& target, const int& oldValue, const int& newValue, const bool ) const
{
	target.maxHitPointsModified (oldValue, newValue);
}

//----------------------------------------------------------------------

inline std::string const & TangibleObject::getRemoteStreamAspect() const
{
	return m_remoteStreamAspect.get();
}

//----------------------------------------------------------------------

inline const std::string & TangibleObject::getAppearanceData() const
{
	return m_appearanceData.get();
}

//----------------------------------------------------------------------

inline bool TangibleObject::isVisible( ) const
{
	return m_visible.get() != 0;
}

// ----------------------------------------------------------------------

inline bool TangibleObject::isInvulnerable() const
{
	return hasCondition(C_invulnerable);
}

// ----------------------------------------------------------------------

inline const ArrangementDescriptor *TangibleObject::getArrangementDescriptor() const
{
	return m_arrangementDescriptor;
}

//----------------------------------------------------------------------

inline void TangibleObject::setPvpFlags(uint32 flags)
{
	m_pvpFlags = flags;
}

//----------------------------------------------------------------------

inline int TangibleObject::getCount(void) const
{
	return m_count.get();
}

//----------------------------------------------------------------------

inline int TangibleObject::getCondition(void) const
{
	return m_condition.get();
}

//----------------------------------------------------------------------

inline bool TangibleObject::hasCondition(int condition) const
{
	return (m_condition.get() & condition) != 0;
}

//----------------------------------------------------------------------

inline bool TangibleObject::isCrafted(void) const
{
	return hasCondition(C_crafted);
}

//----------------------------------------------------------------------

inline bool TangibleObject::isPassiveRevealPlayerCharacter(NetworkId const & target) const
{
	return m_passiveRevealPlayerCharacter.contains(target);
}

//----------------------------------------------------------------------

inline uint32 TangibleObject::getOverriddenMapColor() const
{
	return m_mapColorOverride.get();
}

// ======================================================================

#endif	// INCLUDED_TangibleObject_H
