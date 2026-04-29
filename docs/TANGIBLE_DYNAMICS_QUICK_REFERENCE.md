# TangibleDynamics Quick Reference Card

## System Condition
```
CONDITION_MAGIC_TANGIBLE_DYNAMIC = 0x80000000
```
Located in: `CONDITION_MAGIC_TANGIBLE_DYNAMIC`

## Force Types

### 1. PUSH - Linear Velocity
Apply movement in any coordinate space

**Parameters:**
- `velocityX, velocityY, velocityZ` - Velocity components
- `duration` - Duration in seconds (-1 = infinite)
- `space` - Coordinate space (SPACE_WORLD, SPACE_PARENT, SPACE_OBJECT)

**Example:**
```java
tangible_dynamics.applyPushForce(target, 0.0f, 5.0f, 0.0f, 3.0f, SPACE_WORLD);
// Push upward at 5 units/sec for 3 seconds
```

---

### 2. SPIN - Angular Velocity
Rotate object around its axes

**Parameters:**
- `rotYaw, rotPitch, rotRoll` - Rotation in radians per second
- `duration` - Duration in seconds (-1 = infinite)
- `aroundCenter` - Rotate around appearance center (true/false)

**Example:**
```java
float spinSpeed = (float)Math.PI; // 180°/sec
tangible_dynamics.applySpinForce(target, spinSpeed, 0.0f, 0.0f, 5.0f, false);
// Spin on Y-axis at 180°/sec for 5 seconds
```

**Common Rotation Speeds:**
```
180°/sec = π radians/sec = 3.14159
90°/sec = π/2 radians/sec = 1.57080
45°/sec = π/4 radians/sec = 0.78540
360°/sec = 2π radians/sec = 6.28318
```

---

### 3. BREATHING - Scale Pulsation
Make object grow and shrink rhythmically

**Parameters:**
- `minScale, maxScale` - Scale range (e.g., 0.8 to 1.2 = 20% variation)
- `speed` - Pulse speed (higher = faster)
- `duration` - Duration in seconds (-1 = infinite)

**Example:**
```java
tangible_dynamics.applyBreathingEffect(target, 0.8f, 1.2f, 1.5f, 10.0f);
// Pulse between 80-120% size, speed 1.5, for 10 seconds
```

**Speed Guidelines:**
```
0.5 = slow pulse (2 seconds per cycle)
1.0 = normal pulse (1 second per cycle)
1.5 = fast pulse (0.67 seconds per cycle)
2.0 = very fast pulse (0.5 seconds per cycle)
```

---

### 4. COMBINED - All Three Forces
Apply push, spin, and breathing simultaneously

**Example:**
```java
tangible_dynamics.applyCombinedForces(
    target,
    0.0f, 1.0f, 0.0f,        // push: up at 1 unit/sec
    1.57f, 0.0f, 0.0f,       // spin: 90°/sec on Y-axis
    0.9f, 1.1f, 1.0f,        // breathing: 90-110% at speed 1.0
    6.0f                      // duration: 6 seconds
);
```

---

## Cleanup Methods

### Clear Specific Forces
```java
tangible_dynamics.clearPushForce(target);
tangible_dynamics.clearSpinForce(target);
tangible_dynamics.clearBreathingEffect(target);
```

### Clear All Forces
```java
tangible_dynamics.clearAllForces(target);
```

---

## Condition Checking

### Check if Object Has Dynamics
```java
if (hasCondition(target, CONDITION_MAGIC_TANGIBLE_DYNAMIC)) {
    // Object currently has dynamics effects
}
```

---

## Movement Spaces

| Space | Usage | Reference Frame |
|-------|-------|-----------------|
| `SPACE_WORLD` | Global movement | World coordinates |
| `SPACE_PARENT` | Relative to parent | Parent's local frame |
| `SPACE_OBJECT` | Local movement | Object's local frame |

**Example:**
```java
// Push in world coordinates (global up is always positive Y)
tangible_dynamics.applyPushForce(obj, 0, 5, 0, 3, SPACE_WORLD);

// Push in object coordinates (up relative to object's orientation)
tangible_dynamics.applyPushForce(obj, 0, 5, 0, 3, SPACE_OBJECT);
```

---

## Common Patterns

### Explosion Knockback
```java
Vector direction = target.getLocation().subtract(explosionOrigin);
direction.normalize();
float force = 8.0f;

tangible_dynamics.applyPushForce(
    target,
    direction.x * force,
    direction.y * force,
    direction.z * force,
    2.0f,  // 2 second knockback
    SPACE_WORLD
);
```

### Magical Levitation
```java
tangible_dynamics.applyCombinedForces(
    target,
    0.0f, 0.5f, 0.0f,        // gentle upward drift
    3.14f, 0.0f, 0.0f,       // spinning
    0.95f, 1.05f, 0.8f,      // subtle breathing
    -1.0f                     // infinite duration
);
```

### Pulsing Magical Object
```java
tangible_dynamics.applyBreathingEffect(
    target,
    0.8f,   // shrink to 80%
    1.2f,   // grow to 120%
    2.0f,   // fast pulsing
    -1.0f   // infinite
);
```

### Spinning Top
```java
tangible_dynamics.applySpinForce(
    target,
    0.0f,              // no yaw
    10.0f,             // fast pitch (about 3 full rotations/sec)
    0.0f,              // no roll
    -1.0f,             // infinite
    false              // normal rotation
);
```

### Gentle Bobbing
```java
// Combine slow breathing with slight upward push
tangible_dynamics.applyCombinedForces(
    target,
    0.0f, 0.1f, 0.0f,         // very gentle upward drift
    0.0f, 0.0f, 0.0f,         // no spinning
    0.98f, 1.02f, 0.3f,       // very subtle pulsing
    -1.0f                      // infinite
);
```

---

## Duration Notes

| Duration | Effect |
|----------|--------|
| `-1.0f` | Permanent (must clear manually) |
| `0.0f` | Immediate cleanup |
| `1.0f` | 1 second |
| `5.0f` | 5 seconds |
| `60.0f` | 1 minute |
| `3600.0f` | 1 hour |

---

## Server Integration Checklist

- [ ] Add `tangible_dynamics_handler.java` script to object template
- [ ] Import `script.library.tangible_dynamics` in your script
- [ ] Call appropriate apply methods when needed
- [ ] Check `CONDITION_MAGIC_TANGIBLE_DYNAMIC` to filter active objects
- [ ] Call cleanup methods when effects should stop
- [ ] Test with `tangible_dynamics_test.java`

---

## Performance Tips

1. Use condition checking to avoid filtering all objects
2. Set appropriate durations to auto-cleanup instead of manual clearing
3. Combined forces are more efficient than separate dynamics
4. Server-side updates only occur when forces are active

---

## Math Helper

Convert degrees to radians:
```java
float radians = (float)Math.toRadians(degrees);
// Example: (float)Math.toRadians(180) = 3.14159
```

Common conversions:
```
30° = 0.5236 rad
45° = 0.7854 rad
60° = 1.0472 rad
90° = 1.5708 rad
180° = 3.1416 rad
270° = 4.7124 rad
360° = 6.2832 rad
```

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Effect not applying | Verify handler script is attached |
| Effect stops early | Check duration parameter |
| Object moves wrong direction | Verify coordinate space (SPACE_WORLD vs SPACE_OBJECT) |
| No condition flag set | Check if effect parameter failed validation |
| Client doesn't see effect | Verify client-side dynamics is attached |

---

## API Location Map

```
Server API:        script.library.tangible_dynamics.*
Condition:         CONDITION_MAGIC_TANGIBLE_DYNAMIC
Handler:           handler.tangible_dynamics_handler
Test Script:       script.test.tangible_dynamics_test

C++ Shared:        TangibleDynamics (src/engine/shared/library/sharedObject/)
Client:            ClientTangibleDynamics (client/src/engine/client/library/clientGame/)
```

---

**Last Updated**: 2025-03-04
**Version**: 1.0 - Phase 1 Complete
