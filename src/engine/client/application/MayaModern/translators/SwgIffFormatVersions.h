#pragma once

/**
 * IFF version matrix for SwgMayaEditor — keep in sync with the live game client loaders.
 *
 * MESH (static mesh .msh, top-level FORM MESH)
 *   MeshAppearanceTemplate::load() — clientObject/MeshAppearanceTemplate.cpp
 *   Inner version: TAG_0002, TAG_0003, TAG_0004, TAG_0005 only.
 *   - 0002 / 0003: FORM 000n → SPS → CNTR+RADI (legacy sphere) → exit; then extents + hardpoints (+ floors)
 *     at MESH level outside the inner form.
 *   - 0004 / 0005: FORM 000n → APPR (see below) → SPS → exit inner form.
 *
 * APPR (appearance block inside MESH/0004 or MESH/0005)
 *   AppearanceTemplate::load() — sharedObject/AppearanceTemplate.cpp
 *   Inner: TAG_0001, TAG_0002, TAG_0003.
 *   - 0001: extents, hardpoints
 *   - 0002: extents, hardpoints, floors (FLOR)
 *   - 0003: extents, collision extents, hardpoints, floors
 *
 * SPS (shader primitive set inside MESH, any version path)
 *   ShaderPrimitiveSetTemplate::load_sps() — clientGraphics/ShaderPrimitiveSetTemplate.cpp
 *   Inner: TAG_0000 or TAG_0001.
 *   - 0000: INDX uses int32 indices (loadStaticIndexBuffer32).
 *   - 0001: INDX uses uint16 indices (loadStaticIndexBuffer16).
 *   - Optional SIDX after INDX when hasSortedIndices: int32 nArrays; each array = float direction
 *     + index buffer (same width as INDX: int32 per index for 0000, uint16 for 0001). MshTranslator always consumes SIDX.
 *   - Export (StaticMeshWriter): INFO sets hasSortedIndices=false; no SIDX chunk (no Maya-authored sorted arrays).
 *
 * SKMG (.mgn skeletal mesh generator)
 *   SkeletalMeshGeneratorTemplate — clientSkeletalAnimation/SkeletalMeshGeneratorTemplate.cpp
 *   Inner: TAG_0002, TAG_0003, TAG_0004 (ImportSkeletalMesh / MgnTranslator align here).
 *   Optional FORM TRTS after per-shader PSDT: INFO (headerCount, entryCount), then headerCount × CHUNK TRT
 *   (template name string, affectedShaderCount, then shaderIndex int32 + shaderTextureTag uint32 per affected shader).
 *   Bindings are stored on the import root transform as string attr swgTrtBindings (MgnTranslator, ImportSkeletalMesh).
 *
 * PRTO (.pob portal object)
 *   ImportPob / ExportPob — FORM PRTO inner TAG_0000 .. TAG_0004 (portal geometry IDTL vs PRTL).
 *
 * FLOR / floor collision
 *   FlrTranslator — FORM FLOR inner TAG_0006 (MayaModern/translators/flr.cpp).
 */
