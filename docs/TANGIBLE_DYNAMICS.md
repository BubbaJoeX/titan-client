## TangibleDynamics Implementation Documentation

### Overview
TangibleDynamics is a comprehensive physics system for tangible objects in Titan SWG that allows dynamic effects through three independent force channels:
1. **Push/Shove** - Apply linear velocity in any coordinate space
2. **Spinning** - Apply angular velocity for rotation
3. **Breathing** - Pulsating scale animation

All three can be applied simultaneously or independently, creating rich interactive object behavior.

### Architecture

#### Component Structure

```
/dsrc/sku.0/sys.server/compiled/game/script/
├── library/
│   ├── consts.java (CONDITION_MAGIC_TANGIBLE_DYNAMIC = 0x80000000)
│   └── tangible_dynamics.java (Public API for effects)
└── handler/
    └── tangible_dynamics_handler.java (Message handler)

/src/engine/shared/library/sharedObject/
├── src/shared/dynamics/
│   ├── TangibleDynamics.h
│   └── TangibleDynamics.cpp
└── include/public/sharedObject/
    └── TangibleDynamics.h (forwarding header)

/client/src/engine/client/library/clientGame/
└── src/shared/dynamics/
    ├── ClientTangibleDynamics.h
    └── ClientTangibleDynamics.cpp
```

#### Design Patterns

**Server-Side:**
- Message-driven architecture using `OnTangibleDynamics` handler
- Object variables store dynamics state for persistence
- Condition flag `CONDITION_MAGIC_TANGIBLE_DYNAMIC` marks active objects
- Duration-based auto-cleanup via `messageTo` callbacks

**Client-Side:**
- Mirrors server implementation for visual consistency
- Inherits from base `Dynamics` class
- Integrated with Object's transform system
- Supports recursive scale application

**C++ Shared Library:**
- Base class for both server and client implementations
- Composition-based design (multiple forces in single dynamics object)
- Flexible duration system (-1 = infinite, >= 0 = duration in seconds)

### API Reference

#### Server-Side (tangible_dynamics.java)

```java
// Apply push/shove force
tangible_dynamics.applyPushForce(obj_id target, float velocityX, float velocityY, 
                                 float velocityZ, float duration, int space);
// space: SPACE_WORLD, SPACE_PARENT, SPACE_OBJECT
// duration: -1 for infinite

// Apply spin/rotation force
tangible_dynamics.applySpinForce(obj_id target, float rotYaw, float rotPitch, 
                                float rotRoll, float duration, boolean aroundCenter);
// angles in radians per second
// aroundCenter: true = rotate around appearance sphere center

// Apply breathing/pulsing effect
tangible_dynamics.applyBreathingEffect(obj_id target, float minScale, float maxScale, 
                                      float speed, float duration);
// minScale/maxScale: scale multipliers
// speed: pulse speed (higher = faster)

// Apply all three forces simultaneously
tangible_dynamics.applyCombinedForces(obj_id target, float pushX, float pushY, float pushZ,
                                     float spinYaw, float spinPitch, float spinRoll,
                                     float breatheMin, float breatheMax, float breatheSpeed,
                                     float duration);

// Clear individual forces
tangible_dynamics.clearPushForce(obj_id target);
tangible_dynamics.clearSpinForce(obj_id target);
tangible_dynamics.clearBreathingEffect(obj_id target);

// Clear all forces
tangible_dynamics.clearAllForces(obj_id target);
```

#### C++ API (TangibleDynamics.h / ClientTangibleDynamics.h)

```cpp
// Create dynamics instance
TangibleDynamics* dynamics = new TangibleDynamics(owner_object);

// Push force
void setPushForce(const Vector& velocity, float duration = -1.0f, 
                 MovementSpace space = MS_world);
void clearPushForce();
Vector getPushForce() const;
float getPushForceDuration() const;

// Spin force
void setSpinForce(const Vector& rotationRadiansPerSecond, float duration = -1.0f);
void clearSpinForce();
Vector getSpinForce() const;
float getSpinForceDuration() const;
void setSpinAroundAppearanceCenter(bool spinAroundAppearanceCenter);

// Breathing effect
void setBreathingEffect(float minimumScale, float maximumScale, float speed, 
                       float duration = -1.0f);
void clearBreathingEffect();
float getBreathingMinScale() const;
float getBreathingMaxScale() const;
float getBreathingSpeed() const;
float getBreathingDuration() const;

// Combined
void setCombinedForces(const Vector& pushVelocity, const Vector& spinAngular, 
                      float minScale, float maxScale, float breatheSpeed, 
                      float duration = -1.0f);
void clearAllForces();

// State queries
ForceMode getCurrentForceMode() const; // FM_none, FM_push, FM_spin, FM_breathing, FM_combined
bool isActive() const;

// Base class
virtual float alter(float elapsedTime);
```

### Usage Examples

#### Example 1: Push an object (explosion knockback)
```java
// Push object away from explosion origin
Vector direction = target.getLocation().subtract(explosionCenter);
direction.normalize();
float knockbackForce = 5.0f; // units per second

tangible_dynamics.applyPushForce(target, 
    direction.x * knockbackForce,
    direction.y * knockbackForce,
    direction.z * knockbackForce,
    2.0f,  // duration: 2 seconds
    tangible_dynamics.SPACE_WORLD);
```

#### Example 2: Spin an object
```java
// Make object spin on Y axis (yaw)
float degreesPerSecond = 180.0f;
float radiansPerSecond = (float)Math.toRadians(degreesPerSecond);

tangible_dynamics.applySpinForce(target,
    radiansPerSecond,  // yaw
    0.0f,             // pitch
    0.0f,             // roll
    5.0f,             // duration: 5 seconds
    false);           // around center
```

#### Example 3: Breathing effect (magical object)
```java
// Object grows and shrinks (min 0.8x, max 1.2x scale)
tangible_dynamics.applyBreathingEffect(target,
    0.8f,   // minimum scale
    1.2f,   // maximum scale
    1.0f,   // speed
    -1.0f); // infinite duration
```

#### Example 4: Combined effect (magical artifact)
```java
// Levitating magical object spinning, bobbing, and pulsing
tangible_dynamics.applyCombinedForces(target,
    0.0f,    // push x (no push)
    0.5f,    // push y (gentle rise)
    0.0f,    // push z (no push)
    0.0f,    // spin yaw
    3.14f,   // spin pitch (rotation around x-axis, pi radians/sec)
    2.0f,    // spin roll
    0.9f,    // breathing min
    1.1f,    // breathing max
    0.5f,    // breathing speed
    -1.0f);  // infinite duration
```

#### Example 5: Cleanup
```java
// Remove all effects
tangible_dynamics.clearAllForces(target);

// Or selectively clear
tangible_dynamics.clearPushForce(target);
tangible_dynamics.clearSpinForce(target);
// breathing still active...
```

### Integration Guide

#### For Server Objects

1. **Attach the handler script** to any object that should support dynamics:
   ```xml
   <!-- In object template or script attach -->
   <script name="handler.tangible_dynamics_handler"/>
   ```

2. **Use the library in your scripts**:
   ```java
   import script.library.tangible_dynamics;
   
   public class my_special_object extends script.base_script {
       public int OnInitialize(obj_id self) throws InterruptedException {
           // Apply effects when object spawns
           tangible_dynamics.applySpinForce(self, 1.57f, 0.0f, 0.0f, -1.0f, false);
           return SCRIPT_CONTINUE;
       }
   }
   ```

3. **Condition tracking**:
   - The system automatically sets `CONDITION_MAGIC_TANGIBLE_DYNAMIC` when effects are applied
   - Clears the condition when all forces are removed
   - Check with: `hasCondition(target, CONDITION_MAGIC_TANGIBLE_DYNAMIC)`

#### For Client Objects

1. **Condition detection**:
   ```cpp
   if (object->hasCondition(C_magicTangibleDynamic)) {
       // Object has dynamics effects
   }
   ```

2. **Direct dynamics application** (for client-side effects):
   ```cpp
   Object* obj = // ... get object
   ClientTangibleDynamics* dynamics = new ClientTangibleDynamics(obj);
   obj->setDynamics(dynamics);
   
   // Apply effects
   dynamics->setSpinForce(Vector(0.0f, 3.14f, 0.0f), 5.0f);
   ```

### Performance Considerations

1. **Duration-based cleanup**: Objects automatically stop receiving updates after duration expires
2. **Condition filtering**: Use condition checks to quickly identify affected objects
3. **Composition efficiency**: Single dynamics object handles all three forces vs. stacking multiple dynamics
4. **Client-side mirroring**: Network synchronization happens via condition flag

### Technical Notes

**Coordinate Spaces:**
- `SPACE_WORLD`: Velocity applied in world coordinates (global reference frame)
- `SPACE_PARENT`: Velocity applied in parent object's coordinate system
- `SPACE_OBJECT`: Velocity applied in object's local coordinates

**Scale Clamping:**
- Breathing effect automatically clamps to min/max range
- Direction reverses at boundaries for smooth pulsing
- Base scale is captured when effect starts

**Rotation Modes:**
- Normal: Rotations around object's center
- Around Appearance Center: Useful for orbiting effects around visual center

**Duration System:**
- `-1.0f` = infinite duration (no auto-cleanup)
- `0.0f` to `N.0f` = duration in seconds, auto-clears on expiry
- Cleanup happens via automatic callback on server side

### Thread Safety

The system is designed for single-threaded game loop execution. All modifications should occur in message handlers or update phases.

### Future Enhancements

Potential Phase 2 features:
- Velocity/rotation acceleration/deceleration curves
- Damping factors for velocity decay
- Hierarchical dynamics (parent-child force propagation)
- Client-side prediction for network optimization
- Effect stacking/composition presets
- Visual FX triggers on effect application
