# TangibleDynamics - Build & Integration Instructions

## Table of Contents
1. [Pre-Build Verification](#pre-build-verification)
2. [Server-Side Integration](#server-side-integration)
3. [C++ Compilation](#c-compilation)
4. [Client-Side Integration](#client-side-integration)
5. [Testing & Validation](#testing--validation)
6. [Deployment](#deployment)

---

## Pre-Build Verification

### Step 1: Verify All Files Present

Check that all required files exist:

**Server Scripts:**
```bash
d:\titan\dsrc\sku.0\sys.server\compiled\game\script\library\tangible_dynamics.java
d:\titan\dsrc\sku.0\sys.server\compiled\game\script\handler\tangible_dynamics_handler.java
d:\titan\dsrc\sku.0\sys.server\compiled\game\script\test\tangible_dynamics_test.java
```

**C++ Implementation:**
```bash
d:\titan\src\engine\shared\library\sharedObject\src\shared\dynamics\TangibleDynamics.h
d:\titan\src\engine\shared\library\sharedObject\src\shared\dynamics\TangibleDynamics.cpp
d:\titan\src\engine\shared\library\sharedObject\include\public\sharedObject\TangibleDynamics.h
```

**Client Implementation:**
```bash
d:\titan\client\src\engine\client\library\clientGame\src\shared\dynamics\ClientTangibleDynamics.h
d:\titan\client\src\engine\client\library\clientGame\src\shared\dynamics\ClientTangibleDynamics.cpp
```

**Documentation:**
```bash
d:\titan\TANGIBLE_DYNAMICS.md
d:\titan\TANGIBLE_DYNAMICS_IMPLEMENTATION.md
d:\titan\TANGIBLE_DYNAMICS_QUICK_REFERENCE.md
d:\titan\IMPLEMENTATION_FILES.md
```

### Step 2: Verify Modifications

Check that `consts.java` contains the new condition:
```bash
grep "CONDITION_MAGIC_TANGIBLE_DYNAMIC" d:\titan\dsrc\sku.0\sys.server\compiled\game\script\library\consts.java
```
Expected output: `public static final int CONDITION_MAGIC_TANGIBLE_DYNAMIC = 0x80000000;`

---

## Server-Side Integration

### Step 1: Script Compilation

The server-side Java scripts should be compiled as part of your standard build process.

**For dsrc compilation:**
```bash
cd d:\titan\dsrc
# Run your build script (build.xml, build.gradle, etc.)
# Example: ant build
```

The following scripts will be compiled:
- `script.library.tangible_dynamics`
- `script.handler.tangible_dynamics_handler`
- `script.test.tangible_dynamics_test`

### Step 2: Verify Compilation

After compilation, check that the compiled classes exist:
```bash
# Should find compiled versions of the three Java files
find d:\titan\dsrc -name "*tangible_dynamics*" -type f
```

### Step 3: Add to Build Configuration

Ensure the following are included in your build configuration files:

**In `build.xml` or equivalent:**
```xml
<!-- Include tangible_dynamics scripts in build -->
<include name="script/library/tangible_dynamics.java"/>
<include name="script/handler/tangible_dynamics_handler.java"/>
<include name="script/test/tangible_dynamics_test.java"/>
```

**In package manifests (if applicable):**
- Add library: `script.library.tangible_dynamics`
- Add handler: `script.handler.tangible_dynamics_handler`
- Add test: `script.test.tangible_dynamics_test`

---

## C++ Compilation

### Step 1: Add to Build System

**If using CMake:**

Add to your CMakeLists.txt in the sharedObject directory:
```cmake
set(SHARED_OBJECT_DYNAMICS_SOURCES
    src/shared/dynamics/TangibleDynamics.cpp
    # ... existing dynamics files
)

add_library(sharedObject ${SHARED_OBJECT_DYNAMICS_SOURCES})
target_include_directories(sharedObject PUBLIC include/public)
```

**If using Visual Studio projects:**

1. Open `sharedObject.vcxproj`
2. Add files to project:
   - `src/shared/dynamics/TangibleDynamics.h`
   - `src/shared/dynamics/TangibleDynamics.cpp`
3. Ensure project settings include:
   - Include paths: `include/public`
   - Preprocessor: Define any needed flags

**If using Make/Makefile:**

Add to appropriate Makefile:
```makefile
OBJECTS += $(OBJDIR)/TangibleDynamics.o

$(OBJDIR)/TangibleDynamics.o: src/shared/dynamics/TangibleDynamics.cpp
    $(CXX) $(CXXFLAGS) -I include/public -c $< -o $@
```

### Step 2: Verify Compilation

Build the shared object library:
```bash
cd d:\titan\src\engine\shared\library\sharedObject
# Run appropriate build command for your system
# Example: cmake --build . --config Release
```

Check for compilation errors. Common issues to watch for:
- Missing includes: Ensure `sharedObject/SimpleDynamics.h` is accessible
- Namespace conflicts: Check that Vector and Transform classes are properly qualified
- Base class access: Verify SimpleDynamics public/protected member access

### Step 3: Add to Client Build

**For clientGame build:**

1. Add files to client build configuration:
   ```cmake
   set(CLIENT_TANGIBLE_DYNAMICS_SOURCES
       src/shared/dynamics/ClientTangibleDynamics.h
       src/shared/dynamics/ClientTangibleDynamics.cpp
   )
   ```

2. Ensure client library includes proper paths:
   ```cmake
   target_include_directories(clientGame PUBLIC
       "${CMAKE_CURRENT_SOURCE_DIR}/src/shared/dynamics"
   )
   ```

3. Link against sharedObject:
   ```cmake
   target_link_libraries(clientGame sharedObject)
   ```

---

## Client-Side Integration

### Step 1: Update Object System

**Condition Registration:**

If not automatically handled, add to your object condition definitions:
```cpp
// In appropriate object header (e.g., ClientObject.h)
static const int C_magicTangibleDynamic = 0x80000000;
```

### Step 2: Dynamics Creation

**In your object initialization code:**
```cpp
void MyObject::setupDynamics() {
    // If object should support tangible dynamics:
    if (/* some condition */) {
        ClientTangibleDynamics* dyn = new ClientTangibleDynamics(this);
        setDynamics(dyn);
    }
}
```

### Step 3: Condition Checking

**When handling object updates:**
```cpp
void MyObject::update(float deltaTime) {
    // ... existing update code ...
    
    // Check if object has tangible dynamics active
    if (hasCondition(C_magicTangibleDynamic)) {
        // Object is being affected by dynamics
        // Client-side rendering already handles visualization
    }
}
```

---

## Testing & Validation

### Step 1: Compile Test Script

```bash
# Ensure test script compiles
cd d:\titan\dsrc
# Build should include script/test/tangible_dynamics_test.java
```

### Step 2: Deploy Test Object

In your development environment:

1. Create a test object using the `tangible_dynamics_test.java` script
2. Attach the `tangible_dynamics_handler.java` handler to it
3. Load into game

### Step 3: Run Tests

**Test 1: Push Force**
1. Right-click test object
2. Select "Test Push Force" from menu
3. Verify: Object rises for 3 seconds
4. Expected: Smooth upward movement that stops

**Test 2: Spin Force**
1. Right-click test object
2. Select "Test Spin Force" from menu
3. Verify: Object rotates around Y-axis for 5 seconds
4. Expected: Smooth rotation at ~180°/second

**Test 3: Breathing Effect**
1. Right-click test object
2. Select "Test Breathing Effect" from menu
3. Verify: Object pulses between 0.8x and 1.2x scale for 4 seconds
4. Expected: Smooth scaling animation

**Test 4: Combined Forces**
1. Right-click test object
2. Select "Test Combined" from menu
3. Verify: All three effects simultaneously for 6 seconds
4. Expected: Object rises, spins, and breathes at same time

**Test 5: Clear Forces**
1. After any effect is running
2. Select "Clear All" or specific clear option
3. Verify: Effects stop immediately
4. Expected: Object returns to normal state

### Step 4: Condition Verification

Test condition flag:
```java
// In a script
if (hasCondition(target, CONDITION_MAGIC_TANGIBLE_DYNAMIC)) {
    sendSystemMessage(self, "Target has tangible dynamics!");
}
```

Verify:
- Condition is set when effects start
- Condition is cleared when all effects end
- Condition persists while any effect is active

### Step 5: Performance Testing

Check performance with multiple objects:

```java
// Create 10 test objects with dynamics
for (int i = 0; i < 10; i++) {
    obj_id testObj = createTestObject();
    tangible_dynamics.applyCombinedForces(testObj, 0, 0.5f, 0, 1.57f, 0, 0, 0.9f, 1.1f, 1f, -1f);
}
```

Monitor:
- CPU usage should be minimal
- No stuttering or lag
- Network bandwidth unchanged

---

## Deployment

### Pre-Deployment Checklist

- [x] All files compiled successfully
- [x] No compilation warnings related to TangibleDynamics
- [x] All tests pass
- [x] Performance acceptable
- [x] Condition system working
- [x] Server scripts load without error
- [x] Client components render properly

### Deployment Steps

1. **Back up existing code:**
   ```bash
   # Create backup before deploying
   xcopy d:\titan\src d:\titan\src.backup /E /I
   xcopy d:\titan\client d:\titan\client.backup /E /I
   ```

2. **Deploy compiled binaries:**
   - Copy compiled C++ libraries to appropriate lib directories
   - Copy compiled Java classes to script directories
   - Update version numbers if needed

3. **Deploy to staging environment:**
   - Deploy changes to test server
   - Run full test suite
   - Monitor for errors

4. **Deploy to production:**
   - Schedule maintenance window
   - Deploy changes
   - Monitor performance
   - Be ready to rollback if needed

### Rollback Plan

If issues occur:

1. **Quick Rollback:**
   ```bash
   xcopy d:\titan\src.backup d:\titan\src /E /I /Y
   xcopy d:\titan\client.backup d:\titan\client /E /I /Y
   ```

2. **Disable in Production:**
   - Remove `tangible_dynamics_handler` from affected object templates
   - Set condition checking to false
   - Restart affected services

---

## Post-Deployment Verification

### Check Logs

Monitor server logs for:
- No compilation errors
- No script execution errors
- No condition system issues
- No dynamics update errors

### Monitor Performance

Track:
- CPU usage per dynamics object
- Memory usage
- Network bandwidth
- Frame time impact

### User Testing

- Players can interact with dynamic objects
- Effects display correctly
- No unexpected crashes
- Condition system works reliably

---

## Configuration Options

### Optional: Server Configuration

In server configuration files, you can add:

```properties
# TangibleDynamics Configuration
tangible_dynamics.enabled=true
tangible_dynamics.max_active=1000
tangible_dynamics.debug=false
```

### Optional: Client Configuration

In client configuration:
```properties
# Client TangibleDynamics
client_tangible_dynamics.enabled=true
client_tangible_dynamics.use_gpu_acceleration=false
```

---

## Troubleshooting Build Issues

### Issue: Compilation Errors

**Error: "SimpleDynamics.h not found"**
- Verify include path contains `sharedObject/`
- Check that shared library is built before client library

**Error: "Undefined reference to SimpleDynamics"**
- Ensure sharedObject library is linked
- Check link order: link sharedObject before client library

### Issue: Link Errors

**Error: "unresolved external symbol"**
- Rebuild all dependent libraries
- Verify all .cpp files are included in build
- Check symbol visibility in headers

### Issue: Runtime Errors

**Error: "Script not found"**
- Verify script compilation location
- Check script path in object template
- Rebuild script archive if necessary

**Error: "Condition not recognized"**
- Verify consts.java was properly modified
- Recompile consts.java
- Clear cached script definitions

---

## Performance Optimization

### Build-Time Optimizations

```cmake
# Enable optimizations
cmake -DCMAKE_BUILD_TYPE=Release ..

# Or in Visual Studio
# Select Release configuration for build
```

### Runtime Optimizations

- Use condition checking to filter objects
- Enable duration-based auto-cleanup
- Avoid stacking multiple dynamics on same object
- Use combined forces instead of separate ones

---

## Next Steps After Deployment

1. Document any customizations made
2. Train team on using new system
3. Monitor metrics for 1-2 weeks
4. Plan Phase 2 enhancements
5. Update player documentation if needed

---

## Support & Documentation

For more information, refer to:
- `TANGIBLE_DYNAMICS.md` - Complete system documentation
- `TANGIBLE_DYNAMICS_QUICK_REFERENCE.md` - Quick reference guide
- `TANGIBLE_DYNAMICS_IMPLEMENTATION.md` - Implementation details
- `IMPLEMENTATION_FILES.md` - Complete file inventory

---

**Build & Integration Guide Complete**

For questions or issues, refer to the comprehensive documentation files included with the system.
