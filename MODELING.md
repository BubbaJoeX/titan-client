# MODELING.md - Maya 8 Asset Creation Tutorial for SWG

## Table of Contents

1. [Introduction](#introduction)
2. [Prerequisites](#prerequisites)
3. [Understanding File Types](#understanding-file-types)
4. [Plugin Installation](#plugin-installation)
5. [Project Setup](#project-setup)
6. [Creating Static Meshes (.msh)](#creating-static-meshes-msh)
7. [Creating Skeletal Meshes (.mgn)](#creating-skeletal-meshes-mgn)
8. [Creating Skeletal Appearance Templates (.sat)](#creating-skeletal-appearance-templates-sat)
9. [Working with Animations](#working-with-animations)
10. [Naming Conventions](#naming-conventions)
11. [Common Pitfalls](#common-pitfalls)
12. [Troubleshooting](#troubleshooting)

---

## Introduction

This guide provides a comprehensive walkthrough for creating game assets for Star Wars Galaxies using Maya 8 and the SWG Maya Exporter plugin. Whether you're creating static environmental props, skeletal characters, or animated creatures, this tutorial will help you understand the entire asset creation pipeline.

**What You'll Learn:**

- How to set up Maya 8 for SWG asset creation
- The purpose and workflow for .sat, .mgn, and .msh files
- Best practices for modeling, rigging, and exporting assets
- How to troubleshoot common export issues

---

## Prerequisites

### Software Requirements

- **Autodesk Maya 8** (required - this plugin is specifically built for Maya 8)
- **Visual Studio 2013** (for building the plugin from source, if needed)
- **Windows OS** (the plugin is Windows-only)

### Knowledge Requirements

- Basic understanding of Maya interface and modeling
- Familiarity with 3D modeling concepts (polygons, UVs, normals)
- Understanding of skeletal rigging for character work
- Basic knowledge of file management and directory structures

### System Setup

- Ensure you have write permissions to your export directories
- Recommended: Set up version control for your Maya scene files
- Configure your Maya project with proper source and destination folders

---

## Understanding File Types

The SWG Maya Exporter works with three primary file types, each serving a specific purpose in the game engine:

### .msh (Mesh Appearance - Static Mesh)

**Purpose:** Static, non-deforming geometry for environmental objects and props.

**Use Cases:**

- Buildings and structures
- Environmental props (rocks, trees, furniture)
- Static decorative elements
- Collision meshes for environments

**Characteristics:**

- No skeletal deformation
- Can include multiple LOD (Level of Detail) levels
- Supports shader assignments and textures
- Optimized for rendering performance

### .mgn (Mesh Generator - Skeletal Mesh)

**Purpose:** Deformable geometry bound to a skeleton for animated characters and objects.

**Use Cases:**

- Character bodies and clothing
- Creature meshes
- Animated objects (doors, machinery with moving parts)
- Anything requiring skeletal animation

**Characteristics:**

- Bound to skeleton bones via skin weights
- Supports up to 4 bone influences per vertex (configurable)
- Can include blend shapes (morph targets) for facial animation
- Supports occlusion zones for optimization
- Includes LOD support via .lmg (LOD Mesh Generator)

### .sat (Skeletal Appearance Template)

**Purpose:** Container file that packages together skeletons, meshes, and animation references.

**Use Cases:**

- Complete character definitions
- Assembling multiple mesh parts into a single character
- Defining which animations work with which character
- Creating customizable character appearances

**Characteristics:**

- References one or more .skt (skeleton template) files
- References one or more .mgn (mesh generator) files
- References .asg (animation state graph) for animation control
- Acts as the "master" file for a complete animated character
- Enables mix-and-match customization systems

---

## Plugin Installation

### Step 1: Locate the Plugin

After building the project (see main README.md), the Maya Exporter plugin will be located at:

```
compile/win32/MayaExporter/Release/MayaExporter.mll
```

### Step 2: Install the Plugin

1. **Copy the Plugin:**
  - Copy `MayaExporter.mll` to one of Maya's plugin directories:
    - User directory: `C:\Documents and Settings\[username]\My Documents\maya\8.0\plug-ins\`
    - Maya installation: `C:\Program Files\Autodesk\Maya8.0\bin\plug-ins\`
2. **Load the Plugin in Maya:**
  - Open Maya 8
  - Go to `Window > Settings/Preferences > Plug-in Manager`
  - Locate `MayaExporter.mll` in the list
  - Check both `Loaded` and `Auto load` boxes
  - Click `Close`
3. **Verify Installation:**
  - In Maya's Script Editor, type: `exportCommand;`
  - Press Enter
  - If successful, an export dialog should appear

### Step 3: Configure Export Directories

Set up your export paths using MEL commands:

```mel
// Set the base directory for all exports
setBaseDirectory "C:/swg/data";

// This will automatically create subdirectories:
// - C:/swg/data/exported/appearance/
// - C:/swg/data/exported/appearance/mesh/
// - C:/swg/data/exported/appearance/skeleton/
// - C:/swg/data/exported/shader/
// - C:/swg/data/exported/texture/
```

---

## Project Setup

### Directory Structure

Organize your Maya project following this recommended structure:

```
MyProject/
├── scenes/          # Maya scene files (.ma, .mb)
├── sourceimages/    # Texture source files
├── textures/        # Compiled textures
├── exported/        # Export destination
│   ├── appearance/
│   │   ├── mesh/    # .msh and .mgn files
│   │   ├── skeleton/ # .skt files
│   │   └── *.sat    # SAT files
│   ├── shader/      # Shader templates
│   └── texture/     # Exported textures
└── reference/       # Reference images/models
```

### Maya Scene Setup

1. **Set Project:**
  ```
   File > Project > Set...
  ```
   Navigate to your project root folder.
2. **Configure Units:**
  ```
   Window > Settings/Preferences > Preferences > Settings
  ```
  - Linear: `meter` (SWG uses meters)
  - Angular: `degree`
  - Time: `film (24 fps)` or `NTSC (30 fps)` (check with your animation requirements)
3. **Grid Settings:**
  ```
   Display > Grid > Options
  ```
  - Length and Width: `12 meters` (or as needed)
  - Grid lines every: `1 meter`
  - Subdivisions: `4`

---

## Creating Static Meshes (.msh)

### Step 1: Modeling Guidelines

**Polygon Count:**

- Keep polygon counts reasonable for real-time rendering
- Aim for 1,000-5,000 polys for most props
- Use LOD (Level of Detail) for larger objects

**Topology:**

- Use **triangulated polygons** (the exporter converts quads to tris)
- Ensure all faces have consistent normals
- Avoid n-gons (polygons with more than 4 sides)

**Scale:**

- Work in meters (Maya units should be set to meters)
- Average character height: ~1.8 meters
- Check scale against reference objects

### Step 2: Naming Conventions

Maya objects should follow this naming pattern:

```
objectname_msh
```

Example:

```
crate_wooden_msh
table_cantina_msh
rock_desert_01_msh
```

**Rules:**

- Use underscores `_` to separate words
- End with `_msh` suffix
- Avoid special characters
- Use lowercase for consistency

### Step 3: UV Mapping

1. **Create UVs:**
  ```
   Window > UV Texture Editor
  ```
  - Unwrap your model
  - Ensure UVs are within 0-1 space
  - Avoid overlapping UVs (unless intentional for tiling)
2. **Multiple UV Sets:**
  - SWG supports up to 8 UV sets per mesh
  - Create additional UV sets for detail maps or lightmaps:
    ```
    Create UVs > UV Set > Create UV Set > [name]
    ```
3. **Check UVs:**
  - All faces must have UVs assigned
  - UVs should not have gaps or extreme distortion
  - Use Maya's `UV Texture Editor > Display > Checkerboard` to verify

### Step 4: Assign Shaders

1. **Create a Lambert or Phong shader:**
  ```
   Window > Rendering Editors > Hypershade
  ```
  - Create a new `Lambert` or `Phong` shader
  - Name it descriptively (e.g., `wood_crate_shader`)
2. **Assign textures:**
  - In the shader's attributes, add texture files to:
    - Color/Diffuse channel
    - Normal/Bump channel (if needed)
    - Specular channel (if needed)
3. **SOE Custom Attributes:**
  The exporter uses custom attributes on shaders for special features:
4. **Assign to mesh:**
  - Select faces or entire mesh
  - Right-click shader in Hypershade
  - Choose `Assign Material To Selection`

### Step 5: Export the Static Mesh

1. **Select the mesh:**
  - In the outliner or viewport, select your `objectname_msh` object
2. **Open the export dialog:**
  ```
   exportCommand;
  ```
3. **Choose Export Type:**
  - Select `Static Mesh (MSH/LOD)` radio button
4. **Set export options:**
  - **Branch:** `current` (or your desired data branch)
  - **Interactive:** Checked (if you want to select the object)
  - Click `OK`
5. **Result:**
  - The mesh will export to: `exported/appearance/mesh/objectname.msh`
  - Shader templates export to: `exported/shader/`
  - Textures export to: `exported/texture/`
  - A `.log` file is created for tracking reexports

### Step 6: LOD (Level of Detail) Setup

For objects that need multiple detail levels:

1. **Create LOD hierarchy:**
  ```
   objectname_lod/
   ├── objectname_l0/    # Highest detail
   │   └── objectname_msh
   ├── objectname_l1/    # Medium detail
   │   └── objectname_msh
   └── objectname_l2/    # Lowest detail
       └── objectname_msh
  ```
2. **Naming Convention:**
  - Root node ends with `_lod`
  - Each detail level: `_l0`, `_l1`, `_l2`, etc.
  - Mesh nodes still end with `_msh`
3. **Export:**
  - Select the root `_lod` node
  - Run `exportCommand;`
  - Choose `Static Mesh (MSH/LOD)`
  - The exporter automatically detects and processes all LOD levels

---

## Creating Skeletal Meshes (.mgn)

### Step 1: Create or Import Skeleton

You need a skeleton before creating a skeletal mesh.

**Import an existing skeleton:**

```mel
// Reference an existing skeleton
file -r -namespace "skel" "path/to/skeleton_file.mb";
```

**Or create a new skeleton:**

1. ```
  Skeleton > Joint Tool
   ```
2. Create your bone hierarchy
3. Name bones following SWG conventions (see Naming Conventions section)

### Step 2: Model the Mesh

**Guidelines:**

- Model around the skeleton in bind pose
- Keep topology deformation-friendly (good edge flow around joints)
- Consider poly count:
  - Player characters: 3,000-8,000 polys
  - Creatures: Varies by importance (1,000-15,000 polys)
  - Clothing/attachments: 500-2,000 polys

**Naming:**

```
charactername_meshpart_msh
```

Examples:

```
human_male_body_msh
human_male_head_msh
wookiee_torso_msh
```

### Step 3: Skin the Mesh to Skeleton

1. **Select mesh, then skeleton root:**
  - First: Select the mesh
  - Shift+Select: Select the skeleton root joint
2. **Bind Skin:**
  ```
   Skin > Bind Skin > Smooth Bind > Options
  ```
   **Recommended settings:**
  - Bind to: `Selected Joints`
  - Bind Method: `Closest distance`
  - Max Influences: `4` (SWG engine limitation)
  - Dropoff Rate: `4.0`
3. **Paint Weights:**
  ```
   Skin > Edit Smooth Skin > Paint Skin Weights Tool
  ```
   **Tips:**
  - Keep influences per vertex to 4 or fewer
  - Smooth transitions between bone influences
  - Test deformation by rotating joints
  - Pay special attention to joint areas (shoulders, elbows, knees)

### Step 4: UV Mapping

Follow the same UV guidelines as static meshes:

- Unwrap UVs in the bind pose
- Support for multiple UV sets
- Ensure all faces have UVs

### Step 5: Shader Assignment

Same process as static meshes, but consider:

- Body parts often use different shaders
- Clothing may use customizable shaders

**Customizable Shaders (for player customization):**

```mel
// Add customization attributes to shader
addAttr -ln "soe_cc_name_0" -dt "string" yourShader;
setAttr -type "string" yourShader.soe_cc_name_0 "skinColor";

addAttr -ln "soe_cc_private_0" -at "bool" yourShader;
setAttr yourShader.soe_cc_private_0 0;

addAttr -ln "soe_cc_pal_0" -dt "string" yourShader;
setAttr -type "string" yourShader.soe_cc_pal_0 "palette/skin_tones.pal";
```

### Step 6: Occlusion Zones (Optional)

Occlusion zones hide body parts when clothing is equipped:

```mel
// Add occlusion attribute to mesh parent transform
// Select the mesh's transform node, then:
addAttr -ln "SOE_OCCLUDES" -dt "string" meshTransform;
setAttr -type "string" meshTransform.SOE_OCCLUDES "chest,abdomen";
```

**Common zones:**

- `head`, `face`, `hair`
- `chest`, `abdomen`, `back`
- `arm_upper_l`, `arm_upper_r`
- `arm_lower_l`, `arm_lower_r`
- `hand_l`, `hand_r`
- `leg_upper_l`, `leg_upper_r`
- `leg_lower_l`, `leg_lower_r`
- `foot_l`, `foot_r`

### Step 7: Blend Shapes (Facial Animation - Optional)

For facial animation or morphing:

1. **Create blend shape targets:**
  - Duplicate your base mesh
  - Modify vertices to create expressions
  - Name: `meshname_expression` (e.g., `head_smile`, `head_blink`)
2. **Apply Blend Shape Deformer:**
  ```
   // Select targets, then base mesh
   Deform > Create Blend Shape > Options
  ```
3. **The exporter automatically detects blend shapes**

### Step 8: Export the Skeletal Mesh

1. **Select the mesh:**
  - Select your skinned mesh object
2. **Run export command:**
  ```
   exportCommand;
  ```
3. **Choose Export Type:**
  - Select `Mesh Generator (MGN)` radio button
4. **Set Options:**
  - **Node:** (auto-filled if mesh selected)
  - **Branch:** `current`
  - Click `OK`
5. **Result:**
  - Exports to: `exported/appearance/mesh/meshname.mgn`
  - Associated shader templates and textures also export
  - Creates `.log` file

---

## Creating Skeletal Appearance Templates (.sat)

A SAT file combines skeleton(s), mesh(es), and animation references into a complete character.

### Step 1: Prerequisites

Before creating a SAT, you need:

- **Skeleton file(s):** `.skt` files (from `exportSkeleton` command)
- **Mesh Generator file(s):** `.mgn` files (from previous section)
- **Animation State Graph (optional):** `.asg` file

### Step 2: Understanding SAT Structure

A typical SAT contains:

```
charactername.sat
├── References skeleton: "appearance/skeleton/humanoid_m.skt"
├── References mesh 1: "appearance/mesh/human_male_body.mgn"
├── References mesh 2: "appearance/mesh/human_male_head_m01.mgn"
└── References ASG: "animation/humanoid_male.asg"
```

### Step 3: Interactive SAT Export

This is the easiest method for beginners:

1. **Prepare the scene:**
  - Have all your meshes in the scene, properly skinned
  - Meshes should reference the same skeleton
2. **Select meshes:**
  - In the Outliner, select all skinned meshes to include in the SAT
  - Shift+select to select multiple
3. **Run export command:**
  ```
   exportCommand;
  ```
4. **Choose SAT & MGN:**
  - Select `SAT & MGN` radio button
  - Click `OK`
5. **SAT Export Dialog appears:**
  - **SAT File Name:** Enter the name (e.g., `human_male_01`)
  - The exporter automatically:
    - Detects the skeleton from skin binding
    - Exports MGN files for each selected mesh
    - Generates skeleton references
    - Creates the SAT
6. **Result:**
  - SAT file: `exported/appearance/human_male_01.sat`
  - MGN files: `exported/appearance/mesh/[meshnames].mgn`
  - Skeleton: `exported/appearance/skeleton/[skeletonname].skt`

### Step 4: Command-Line SAT Export

For more control, use MEL commands:

```mel
exportSatFile
    -outputfile "human_male_custom"
    -skeleton "appearance/skeleton/humanoid_m.skt:root"
    -mesh "appearance/mesh/human_male_body.mgn"
    -mesh "appearance/mesh/human_male_head_m02.mgn"
    -asg "animation/humanoid_male.asg"
    -branch "current";
```

**Parameters:**

- `-outputfile`: SAT filename (without .sat extension)
- `-skeleton`: Skeleton template reference path
  - Format: `path/to/skeleton.skt:attachment_bone`
  - `attachment_bone` is optional (for attached items)
- `-mesh`: Mesh generator reference (can specify multiple)
- `-asg`: Animation state graph (optional)
- `-branch`: Data branch (usually "current")

### Step 5: Multiple Skeletons (Attachments)

For items that attach to characters (weapons, accessories):

```mel
exportSatFile
    -outputfile "sword_katana"
    -skeleton "appearance/skeleton/humanoid_m.skt:r_hand"
    -mesh "appearance/mesh/weapon_sword_katana.mgn"
    -branch "current";
```

The `:r_hand` specifies which bone the item attaches to.

### Step 6: Verify the Export

Check the export log:

```
exported/appearance/human_male_01.sat.log
```

This file contains:

- Source Maya file
- Export command used
- Skeleton references
- Mesh references
- Export timestamp

---

## Working with Animations

### Creating Animations

**Note:** Animation export creates `.ans` (animation) files, not covered in depth here as this guide focuses on modeling/meshing.

**Quick Overview:**

1. **Animation List:**
  - Animations are managed via custom attributes on skeleton
  - Use `addAnimation` command to register animation ranges
2. **Export Animation:**
  ```
   exportKeyframeSkeletalAnimation -interactive;
  ```
3. **Animation State Graph (.asg):**
  - Defines how animations transition
  - Created through separate tools
  - Referenced by SAT files

---

## Naming Conventions

### General Rules

1. **Use lowercase** for all names (avoid capitals)
2. **Use underscores** `_` to separate words (no spaces or hyphens)
3. **Be descriptive** but concise
4. **Follow type suffixes** (shown below)

### Node Type Suffixes


| Node Type      | Suffix        | Example                 |
| -------------- | ------------- | ----------------------- |
| Static Mesh    | `_msh`        | `crate_wooden_msh`      |
| Skeletal Mesh  | (mesh name)   | `human_male_body`       |
| LOD Group      | `_lod`        | `building_cantina_lod`  |
| Skeleton Joint | (descriptive) | `r_shoulder`, `l_elbow` |
| Collision Mesh | `_cmesh`      | `wall_collision_cmesh`  |
| Floor          | `_floor`      | `cantina_floor_floor`   |
| Component      | `_cmp`        | `building_cantina_cmp`  |


### File Naming Examples

**Static Meshes:**

```
prop_chair_wooden_01.msh
building_house_small_tatooine.msh
rock_desert_large_03.msh
tree_palm_tall.msh
```

**Skeletal Meshes:**

```
human_male_body.mgn
human_female_head_01.mgn
wookiee_body.mgn
creature_rancor_head.mgn
clothing_shirt_formal_m.mgn
```

**Skeletons:**

```
humanoid_male.skt
humanoid_female.skt
creature_quadruped.skt
```

**SAT Files:**

```
human_male_01.sat
wookiee_npc_warrior.sat
creature_rancor_wild.sat
```

### Skeleton Bone Naming

Follow a consistent hierarchy:

```
root                    # Root bone
├── pelvis
│   ├── spine_lower
│   │   ├── spine_upper
│   │   │   ├── r_clavicle
│   │   │   │   ├── r_shoulder
│   │   │   │   │   ├── r_elbow
│   │   │   │   │   │   ├── r_wrist
│   │   │   │   │   │   │   └── r_hand
│   │   │   ├── l_clavicle
│   │   │   │   └── (mirror of right)
│   │   │   └── neck
│   │   │       └── head
│   ├── r_hip
│   │   ├── r_knee
│   │   │   ├── r_ankle
│   │   │   │   └── r_foot
│   └── l_hip
│       └── (mirror of right)
```

**Bone naming tips:**

- Prefix with `r_` for right, `l_` for left
- Use anatomical terms when possible
- Maintain symmetry in naming

---

## Common Pitfalls

### 1. Non-Triangulated Geometry

**Problem:** N-gons or quads can cause export errors or unexpected results.

**Solution:**

```mel
// Select mesh, then:
Mesh > Triangulate
```

### 2. History on Mesh

**Problem:** Construction history can cause export problems.

**Solution:**

```mel
// Select mesh, then:
Edit > Delete by Type > History
```

### 3. Frozen Transformations

**Problem:** Un-frozen transforms can cause misaligned exports.

**Solution:**

```mel
// Before skinning, freeze transforms:
Modify > Freeze Transformations
```

### 4. Too Many Bone Influences

**Problem:** Vertices influenced by more than 4 bones cause errors.

**Solution:**

```mel
// In Paint Weights tool:
// - Set Max Influences: 4
// - Use Prune Weights to remove small influences
Skin > Edit Smooth Skin > Prune Small Weights
```

### 5. Missing UVs

**Problem:** Exporter fails if any faces lack UVs.

**Solution:**

```mel
// Check for missing UVs:
Window > UV Texture Editor
// Select all faces, check if UVs appear
// If missing, create automatic UV projection:
UV Texture Editor > Create UVs > Automatic Mapping
```

### 6. Incorrect File Paths

**Problem:** Shader references textures with absolute paths instead of relative.

**Solution:**

- Always use relative paths in texture file nodes
- Textures should be in project's `sourceimages/` folder
- Use forward slashes `/` not backslashes `\`

### 7. Non-Unique Names

**Problem:** Maya objects with duplicate names cause export confusion.

**Solution:**

```mel
// Check for duplicates:
Window > Outliner
// Look for names with "|" characters (indicates non-unique)
// Rename duplicates to be unique
```

### 8. Scale Issues

**Problem:** Incorrect unit setup leads to oversized or undersized models.

**Solution:**

- Set Maya to meters: `Window > Settings/Preferences > Preferences > Settings`
- Linear: `meter`
- Verify reference scale (average human: ~1.8m tall)

---

## Troubleshooting

### Export Fails Immediately

**Check:**

1. Plugin loaded? (`Window > Settings/Preferences > Plug-in Manager`)
2. Export directories exist and writable?
3. Object selected correctly?
4. Correct export type chosen?

### Mesh Exports but Looks Wrong In-Game

**Check:**

1. Normals facing correct direction?
  ```mel
   Display > Polygons > Face Normals
   // If inverted:
   Mesh > Normals > Reverse
  ```
2. UVs in 0-1 range?
3. Transforms frozen before skinning?
4. Correct shader assignments?

### Skinning Errors on Export

**Check:**

1. Mesh actually bound to skeleton?
2. Max influences per vertex ≤ 4?
3. All bones properly named?
4. No duplicate bone names?
5. Skeleton hierarchy is clean (no cycles)?

### Textures Not Exporting

**Check:**

1. Texture paths are relative?
2. Source images exist in `sourceimages/` folder?
3. Shader has texture assigned to correct channels?
4. File node paths use forward slashes `/`?

### SAT Export Fails

**Check:**

1. All referenced MGN files exist?
2. Skeleton template exists?
3. Meshes are skinned to the same skeleton?
4. File paths are correct and relative?

### "Unknown Node Type" Errors

**Check:**

1. Is the mesh named with proper suffix (`_msh`)?
2. Does the hierarchy follow conventions?
3. Are there any unknown/custom nodes in the hierarchy?

---

## Additional Resources

### Reexporting Assets

The exporter creates `.log` files that enable automatic reexporting:

```mel
// Reexport using the log file
reexport "path/to/file.msh.log";
```

This is useful when:

- Texture files change
- You need to batch re-export multiple assets
- Working in a production pipeline

### Batch Exporting

For exporting multiple assets:

```mel
// Example batch export script
string $meshes[] = `ls -type transform "*_msh"`;
for ($mesh in $meshes) {
    select -r $mesh;
    exportStaticMesh -node $mesh -branch "current";
}
```

### Viewing Exported Assets

While not covered in this guide, exported assets can be viewed using the `Viewer` application included in the tools package.

### Documentation References

- Main README: `/README.md`
- SWG Source Wiki: `https://github.com/swg-source/swg-main/wiki`
- For code changes: See project documentation in repository

---

## Conclusion

This guide covered the essential workflow for creating game assets for Star Wars Galaxies using Maya 8. Remember:

1. **Start Simple:** Begin with basic static meshes before moving to complex skeletal rigs
2. **Follow Conventions:** Naming and structure matter for the game engine
3. **Test Early:** Export early and often to catch issues
4. **Keep Organized:** Maintain clean file structure and naming
5. **Ask for Help:** Join the SWG community Discord for support

Happy modeling, and may the Force be with you!

---

**Document Version:** 1.0  
**Last Updated:** 2026  
**Plugin Version:** MayaExporter 3.009  
**Compatible Maya Version:** Maya 8