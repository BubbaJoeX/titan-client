// ==================================================================
//
// OcclusionZoneSet.h
// copyright 2001 Sony Online Entertainment
//
// ==================================================================

#ifndef OCCLUSION_ZONE_SET
#define OCCLUSION_ZONE_SET

// ==================================================================

#include <memory>
#include <vector>
#include <set>

class CrcLowerString;

// ======================================================================
/**
 * Class that manages sets of occlusion zones.
 *
 * This class is used internally by the CompositeMesh and MeshGenerator
 * classes within the character system.  This class has nothing to do
 * with global scene occlusion.  This class and name came about before
 * we had a global scene-level occlusion system.
 */

class OcclusionZoneSet
    {
public:
    
    typedef std::vector<std::shared_ptr<CrcLowerString> >  CrcLowerStringVector;
    typedef std::vector<int>                                 IntVector;

public:
    
    static void install();
    
    static void                  registerOcclusionZones(const CrcLowerStringVector &occlusionZoneNames, IntVector &occlusionZoneIds);
    static const CrcLowerString &getOcclusionZoneName(int occlusionZoneId);

public:
    
    OcclusionZoneSet();
    ~OcclusionZoneSet();
    
    void  addZone(int zoneId);
    bool  hasZone(int zoneId) const;
    
    void  clear();
    void  insertSet(const OcclusionZoneSet &set);
    
    bool  allZonesPresent(const OcclusionZoneSet &testZones) const;
    bool  allZonesPresent(const IntVector &testZones) const;

private:
    
    static void remove();

private:
    
    typedef std::set<int>  IntSet;

private:
    
    IntSet *m_occlusionZones;

private:
    
    // disable these
    OcclusionZoneSet(const OcclusionZoneSet&);
    OcclusionZoneSet &operator =(const OcclusionZoneSet&);
    
    };

// ==================================================================

#endif
