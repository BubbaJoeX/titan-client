# SWG Asset Toolchain - MGN (Mesh Generator) Format Handler
# Copyright (c) Titan Project
#
# Handles .mgn (SkeletalMeshGeneratorTemplate) files - skinned meshes.
# Reference: D:/titan/client/src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SkeletalMeshGeneratorTemplate.cpp

from __future__ import annotations
from typing import List, Dict, Optional, Tuple, Any, TYPE_CHECKING
from dataclasses import dataclass, field
from pathlib import Path

from ..core.iff import Iff, IffError
from ..core.tag import Tag
from ..core.types import Vector3, Quaternion, PackedArgb
from ..core.registry import FormatHandler, ImportResult, ExportResult, register_handler

if TYPE_CHECKING:
    import bpy

__all__ = ['MgnHandler', 'MeshGeneratorData', 'PerShaderMgnData', 'BlendTargetData']


# =============================================================================
# Tags
# =============================================================================

TAG_SKMG = Tag('SKMG')
TAG_INFO = Tag('INFO')
TAG_SKTM = Tag('SKTM')
TAG_XFNM = Tag('XFNM')
TAG_POSN = Tag('POSN')
TAG_NORM = Tag('NORM')
TAG_DOT3 = Tag('DOT3')
TAG_TWHD = Tag('TWHD')
TAG_TWDT = Tag('TWDT')
TAG_BLTS = Tag('BLTS')
TAG_BLT  = Tag('BLT ')
TAG_OZN  = Tag('OZN ')
TAG_FOZC = Tag('FOZC')
TAG_OZC  = Tag('OZC ')
TAG_PSDT = Tag('PSDT')
TAG_PRIM = Tag('PRIM')
TAG_ITL  = Tag('ITL ')
TAG_OITL = Tag('OITL')
TAG_HPTS = Tag('HPTS')
TAG_TRTS = Tag('TRTS')
TAG_TRT  = Tag('TRT ')
TAG_NAME = Tag('NAME')
TAG_PIDX = Tag('PIDX')
TAG_NIDX = Tag('NIDX')
TAG_VDCL = Tag('VDCL')
TAG_TXCI = Tag('TXCI')
TAG_TCSD = Tag('TCSD')
TAG_TCSF = Tag('TCSF')

# Version tags
TAG_0001 = Tag('0001')
TAG_0002 = Tag('0002')
TAG_0003 = Tag('0003')
TAG_0004 = Tag('0004')


# =============================================================================
# Data Structures
# =============================================================================

@dataclass
class TransformWeightData:
    """Bone weight data for a vertex."""
    transform_index: int = 0
    weight: float = 0.0


@dataclass
class BlendVectorData:
    """Delta vector for blend shapes."""
    index: int = 0
    delta: Vector3 = field(default_factory=Vector3)


@dataclass
class HardpointTargetData:
    """Hardpoint blend target data."""
    hardpoint_index: int = 0
    delta_position: Vector3 = field(default_factory=Vector3)
    delta_rotation: Quaternion = field(default_factory=Quaternion)


@dataclass
class BlendTargetData:
    """Blend shape/morph target."""
    name: str = ""
    variable_path: str = ""
    positions: List[BlendVectorData] = field(default_factory=list)
    normals: List[BlendVectorData] = field(default_factory=list)
    dot3_vectors: List[BlendVectorData] = field(default_factory=list)
    hardpoint_targets: List[HardpointTargetData] = field(default_factory=list)


@dataclass
class OcclusionZoneData:
    """Occlusion zone for mesh hiding."""
    zone_name: str = ""
    combination_id: int = 0


@dataclass
class HardpointMgnData:
    """Hardpoint in mesh generator."""
    name: str = ""
    parent_joint_name: str = ""
    position: Vector3 = field(default_factory=Vector3)
    rotation: Quaternion = field(default_factory=Quaternion)


@dataclass
class PerShaderMgnData:
    """Per-shader data for mesh generator."""
    shader_name: str = ""
    vertex_count: int = 0
    position_indices: List[int] = field(default_factory=list)
    normal_indices: List[int] = field(default_factory=list)
    dot3_indices: List[int] = field(default_factory=list)
    diffuse_colors: List[PackedArgb] = field(default_factory=list)
    tex_coord_sets: List[List[Tuple[float, ...]]] = field(default_factory=list)
    triangle_indices: List[int] = field(default_factory=list)
    occluded_triangles: Dict[int, List[int]] = field(default_factory=dict)


@dataclass
class MeshGeneratorData:
    """Complete mesh generator data."""
    version: str = "0004"
    skeleton_template_name: str = ""
    transform_names: List[str] = field(default_factory=list)
    positions: List[Vector3] = field(default_factory=list)
    normals: List[Vector3] = field(default_factory=list)
    dot3_vectors: List[Tuple[float, float, float, float]] = field(default_factory=list)
    transform_weights: List[List[TransformWeightData]] = field(default_factory=list)
    blend_targets: List[BlendTargetData] = field(default_factory=list)
    occlusion_zones: List[OcclusionZoneData] = field(default_factory=list)
    fully_occluded_zones: List[int] = field(default_factory=list)
    per_shader_data: List[PerShaderMgnData] = field(default_factory=list)
    hardpoints: List[HardpointMgnData] = field(default_factory=list)
    unknown_chunks: Dict[str, bytes] = field(default_factory=dict)


# =============================================================================
# MGN Parser
# =============================================================================

class MgnParser:
    """Parser for MGN (SkeletalMeshGeneratorTemplate) files."""
    
    def __init__(self):
        self.warnings: List[str] = []
        self.unknown_chunks: Dict[str, bytes] = {}
    
    def parse(self, iff: Iff) -> MeshGeneratorData:
        """Parse a mesh generator file."""
        data = MeshGeneratorData()
        
        iff.enter_form(TAG_SKMG)
        
        version_tag = iff.get_current_name()
        data.version = str(version_tag)
        
        if version_tag == TAG_0004:
            self._parse_version_0004(iff, data)
        elif version_tag == TAG_0003:
            self._parse_version_0003(iff, data)
        elif version_tag == TAG_0002:
            self._parse_version_0002(iff, data)
        else:
            self.warnings.append(f"Unknown MGN version: {version_tag}")
            self._parse_version_0004(iff, data)
        
        iff.exit_form()
        
        data.unknown_chunks = self.unknown_chunks
        return data
    
    def _parse_version_0004(self, iff: Iff, data: MeshGeneratorData) -> None:
        """Parse version 0004."""
        iff.enter_form(TAG_0004)
        
        # INFO chunk
        iff.enter_chunk(TAG_INFO)
        max_transforms_per_vertex = iff.read_int32()
        max_transforms_per_shader = iff.read_int32()
        position_count = iff.read_int32()
        transform_weight_count = iff.read_int32()
        normal_count = iff.read_int32()
        shader_count = iff.read_int32()
        blend_target_count = iff.read_int32()
        occlusion_zone_count = iff.read_int32() if iff.get_chunk_length_left() >= 4 else 0
        occlusion_combo_count = iff.read_int32() if iff.get_chunk_length_left() >= 4 else 0
        zone_combo_count = iff.read_int32() if iff.get_chunk_length_left() >= 4 else 0
        iff.exit_chunk()
        
        # Skeleton template name
        if iff.enter_chunk(TAG_SKTM, optional=True):
            data.skeleton_template_name = iff.read_string()
            iff.exit_chunk()
        
        # Transform names
        if iff.enter_chunk(TAG_XFNM, optional=True):
            count = iff.get_chunk_length_left(1)  # Estimate
            while iff.get_chunk_length_left() > 0:
                data.transform_names.append(iff.read_string())
            iff.exit_chunk()
        
        # Positions
        if iff.enter_chunk(TAG_POSN, optional=True):
            for _ in range(position_count):
                x, y, z = iff.read_vector()
                data.positions.append(Vector3(x, y, z))
            iff.exit_chunk()
        
        # Transform weight header
        if iff.enter_chunk(TAG_TWHD, optional=True):
            for _ in range(position_count):
                count = iff.read_uint32()
                data.transform_weights.append([])
            iff.exit_chunk()
        
        # Transform weight data
        if iff.enter_chunk(TAG_TWDT, optional=True):
            for i in range(position_count):
                if i < len(data.transform_weights):
                    count = len(data.transform_weights[i])
                    for _ in range(count):
                        idx = iff.read_int32()
                        weight = iff.read_float()
                        data.transform_weights[i].append(
                            TransformWeightData(idx, weight)
                        )
            iff.exit_chunk()
        
        # Normals
        if iff.enter_chunk(TAG_NORM, optional=True):
            for _ in range(normal_count):
                x, y, z = iff.read_vector()
                data.normals.append(Vector3(x, y, z))
            iff.exit_chunk()
        
        # DOT3 vectors
        if iff.enter_chunk(TAG_DOT3, optional=True):
            while iff.get_chunk_length_left() >= 16:
                x = iff.read_float()
                y = iff.read_float()
                z = iff.read_float()
                flip = iff.read_float()
                data.dot3_vectors.append((x, y, z, flip))
            iff.exit_chunk()
        
        # Blend targets
        if iff.enter_form('BLTS', optional=True):
            for _ in range(blend_target_count):
                if iff.at_end_of_form():
                    break
                blend = self._parse_blend_target(iff)
                data.blend_targets.append(blend)
            iff.exit_form()
        
        # Occlusion zones
        if occlusion_zone_count > 0:
            if iff.enter_chunk('OZN ', optional=True):
                for _ in range(occlusion_zone_count):
                    name = iff.read_string()
                    data.occlusion_zones.append(OcclusionZoneData(name))
                iff.exit_chunk()
        
        # Fully occluded zone combos
        if iff.enter_chunk(TAG_FOZC, optional=True):
            while iff.get_chunk_length_left() >= 2:
                data.fully_occluded_zones.append(iff.read_int16())
            iff.exit_chunk()
        
        # Per-shader data
        for _ in range(shader_count):
            if iff.at_end_of_form():
                break
            if iff.is_current_form():
                shader_data = self._parse_per_shader_data(iff)
                data.per_shader_data.append(shader_data)
            else:
                iff.go_forward()
        
        # Hardpoints
        if iff.enter_form('HPTS', optional=True):
            while not iff.at_end_of_form():
                hp = self._parse_hardpoint(iff)
                data.hardpoints.append(hp)
            iff.exit_form()
        
        iff.exit_form()
    
    def _parse_version_0003(self, iff: Iff, data: MeshGeneratorData) -> None:
        """Parse version 0003 (similar to 0004)."""
        self._parse_version_0004(iff, data)
    
    def _parse_version_0002(self, iff: Iff, data: MeshGeneratorData) -> None:
        """Parse version 0002."""
        self._parse_version_0004(iff, data)
    
    def _parse_blend_target(self, iff: Iff) -> BlendTargetData:
        """Parse a blend target."""
        blend = BlendTargetData()
        
        if not iff.is_current_form():
            return blend
        
        iff.enter_form('BLT ')
        
        if iff.enter_chunk(TAG_INFO, optional=True):
            pos_count = iff.read_int32()
            norm_count = iff.read_int32()
            blend.name = iff.read_string()
            blend.variable_path = f"/shared_owner/{blend.name}"
            iff.exit_chunk()
        
        # Position deltas
        if iff.enter_chunk(TAG_POSN, optional=True):
            while iff.get_chunk_length_left() >= 16:
                idx = iff.read_int32()
                x, y, z = iff.read_vector()
                blend.positions.append(BlendVectorData(idx, Vector3(x, y, z)))
            iff.exit_chunk()
        
        # Normal deltas
        if iff.enter_chunk(TAG_NORM, optional=True):
            while iff.get_chunk_length_left() >= 16:
                idx = iff.read_int32()
                x, y, z = iff.read_vector()
                blend.normals.append(BlendVectorData(idx, Vector3(x, y, z)))
            iff.exit_chunk()
        
        # Hardpoint targets
        if iff.enter_chunk(TAG_HPTS, optional=True):
            count = iff.read_int16()
            for _ in range(count):
                hp_idx = iff.read_int16()
                dx, dy, dz = iff.read_vector()
                w, x, y, z = iff.read_quaternion()
                blend.hardpoint_targets.append(HardpointTargetData(
                    hp_idx, Vector3(dx, dy, dz), Quaternion(w, x, y, z)
                ))
            iff.exit_chunk()
        
        iff.exit_form()
        return blend
    
    def _parse_per_shader_data(self, iff: Iff) -> PerShaderMgnData:
        """Parse per-shader data."""
        shader = PerShaderMgnData()
        
        iff.enter_form('PSDT')
        
        # Version form
        if iff.is_current_form():
            iff.enter_form()
            
            # Shader name
            if iff.enter_chunk(TAG_NAME, optional=True):
                shader.shader_name = iff.read_string()
                iff.exit_chunk()
            
            # INFO chunk
            if iff.enter_chunk(TAG_INFO, optional=True):
                shader.vertex_count = iff.read_int32()
                iff.exit_chunk()
            
            # Position indices
            if iff.enter_chunk(TAG_PIDX, optional=True):
                for _ in range(shader.vertex_count):
                    if iff.get_chunk_length_left() >= 4:
                        shader.position_indices.append(iff.read_int32())
                iff.exit_chunk()
            
            # Normal indices
            if iff.enter_chunk(TAG_NIDX, optional=True):
                for _ in range(shader.vertex_count):
                    if iff.get_chunk_length_left() >= 4:
                        shader.normal_indices.append(iff.read_int32())
                iff.exit_chunk()
            
            # Diffuse colors
            if iff.enter_chunk(TAG_VDCL, optional=True):
                for _ in range(shader.vertex_count):
                    if iff.get_chunk_length_left() >= 4:
                        packed = iff.read_uint32()
                        a = (packed >> 24) & 0xFF
                        r = (packed >> 16) & 0xFF
                        g = (packed >> 8) & 0xFF
                        b = packed & 0xFF
                        shader.diffuse_colors.append(PackedArgb(a, r, g, b))
                iff.exit_chunk()
            
            # Texture coordinates
            if iff.enter_chunk(TAG_TXCI, optional=True):
                num_sets = iff.read_int32()
                iff.exit_chunk()
                
                for set_idx in range(num_sets):
                    if iff.enter_chunk(Tag(f'TCS{set_idx}'), optional=True) or \
                       iff.enter_chunk(TAG_TCSF, optional=True) or \
                       iff.enter_chunk(TAG_TCSD, optional=True):
                        coords = []
                        while iff.get_chunk_length_left() >= 8:
                            u = iff.read_float()
                            v = iff.read_float()
                            coords.append((u, v))
                        shader.tex_coord_sets.append(coords)
                        iff.exit_chunk()
            
            # Primitives
            if iff.enter_form('PRIM', optional=True):
                if iff.enter_chunk('INFO', optional=True):
                    iff.exit_chunk()
                
                # Indexed triangle lists
                while not iff.at_end_of_form():
                    if iff.enter_chunk('ITL ', optional=True):
                        tri_count = iff.read_int32()
                        for _ in range(tri_count * 3):
                            if iff.get_chunk_length_left() >= 4:
                                shader.triangle_indices.append(iff.read_int32())
                        iff.exit_chunk()
                    elif iff.enter_chunk('OITL', optional=True):
                        zone_combo = iff.read_int16()
                        tri_count = iff.read_int32()
                        indices = []
                        for _ in range(tri_count * 3):
                            if iff.get_chunk_length_left() >= 4:
                                indices.append(iff.read_int32())
                        shader.occluded_triangles[zone_combo] = indices
                        iff.exit_chunk()
                    else:
                        iff.go_forward()
                
                iff.exit_form()
            
            iff.exit_form()  # Version
        
        iff.exit_form()  # PSDT
        return shader
    
    def _parse_hardpoint(self, iff: Iff) -> HardpointMgnData:
        """Parse a hardpoint."""
        hp = HardpointMgnData()
        
        if not iff.is_current_form():
            iff.go_forward()
            return hp
        
        iff.enter_form()  # Hardpoint form
        
        if iff.enter_chunk(TAG_NAME, optional=True):
            hp.name = iff.read_string()
            hp.parent_joint_name = iff.read_string()
            iff.exit_chunk()
        
        if iff.enter_chunk('DATA', optional=True):
            x, y, z = iff.read_vector()
            hp.position = Vector3(x, y, z)
            w, qx, qy, qz = iff.read_quaternion()
            hp.rotation = Quaternion(w, qx, qy, qz)
            iff.exit_chunk()
        
        iff.exit_form()
        return hp


# =============================================================================
# Blender Integration
# =============================================================================

@register_handler
class MgnHandler(FormatHandler):
    """Handler for MGN (mesh generator/skinned mesh) files."""
    
    FORMAT_NAME = "MGN"
    FORMAT_DESCRIPTION = "SWG Mesh Generator (SkeletalMeshGeneratorTemplate)"
    FILE_EXTENSIONS = ["mgn"]
    ROOT_TAGS = [TAG_SKMG]
    CAN_IMPORT = True
    CAN_EXPORT = False  # Export not yet implemented
    IS_FULLY_UNDERSTOOD = False
    
    def import_file(self, filepath: str, **options) -> ImportResult:
        """Import MGN file to Blender."""
        try:
            iff = Iff.from_file(filepath)
            parser = MgnParser()
            data = parser.parse(iff)
            
            self.warnings.extend(parser.warnings)
            self.unknown_chunks = parser.unknown_chunks
            
            objects = self._create_blender_objects(data, filepath, **options)
            
            return ImportResult(
                success=True,
                message=f"Imported mesh with {len(data.per_shader_data)} shaders, "
                        f"{len(data.positions)} vertices",
                warnings=self.warnings,
                objects=objects,
                unknown_chunks=self.unknown_chunks,
            )
            
        except Exception as e:
            import traceback
            return ImportResult(
                success=False,
                message=f"Failed to import: {str(e)}\n{traceback.format_exc()}",
                warnings=self.warnings,
            )
    
    def export_file(self, filepath: str, objects: List[Any], **options) -> ExportResult:
        """Export is not yet implemented for MGN."""
        return ExportResult(
            success=False,
            message="MGN export not yet implemented",
            warnings=["MGN export requires complex skinning data reconstruction"],
        )
    
    def _create_blender_objects(self, data: MeshGeneratorData, filepath: str,
                                 **options) -> List[Any]:
        """Create Blender objects from mesh generator data."""
        import bpy
        import bmesh
        
        created_objects = []
        base_name = Path(filepath).stem
        
        # Create a single mesh with vertex groups
        mesh = bpy.data.meshes.new(base_name)
        
        bm = bmesh.new()
        
        # Add all positions as vertices
        verts = []
        for pos in data.positions:
            v = bm.verts.new((pos.x, -pos.z, pos.y))  # SWG Y-up to Blender Z-up
            verts.append(v)
        
        bm.verts.ensure_lookup_table()
        
        # Collect all triangles from all shaders
        for shader in data.per_shader_data:
            indices = shader.triangle_indices
            pos_indices = shader.position_indices
            
            # Map shader vertex indices to global position indices
            for i in range(0, len(indices), 3):
                if i + 2 < len(indices):
                    try:
                        # Get shader vertex indices
                        sv0, sv1, sv2 = indices[i], indices[i+1], indices[i+2]
                        
                        # Map to position indices
                        if sv0 < len(pos_indices) and sv1 < len(pos_indices) and sv2 < len(pos_indices):
                            p0 = pos_indices[sv0]
                            p1 = pos_indices[sv1]
                            p2 = pos_indices[sv2]
                            
                            if p0 < len(verts) and p1 < len(verts) and p2 < len(verts):
                                bm.faces.new((verts[p0], verts[p2], verts[p1]))
                    except (ValueError, IndexError):
                        pass
        
        bm.to_mesh(mesh)
        bm.free()
        mesh.update()
        
        # Create object
        obj = bpy.data.objects.new(base_name, mesh)
        obj['swg_format'] = 'MGN'
        obj['swg_version'] = data.version
        obj['swg_skeleton'] = data.skeleton_template_name
        
        bpy.context.collection.objects.link(obj)
        created_objects.append(obj)
        
        # Create vertex groups for bone weights
        if data.transform_names and data.transform_weights:
            for name in data.transform_names:
                obj.vertex_groups.new(name=name)
            
            # Apply weights
            for vert_idx, weights in enumerate(data.transform_weights):
                for tw in weights:
                    if tw.transform_index < len(data.transform_names):
                        group_name = data.transform_names[tw.transform_index]
                        group = obj.vertex_groups.get(group_name)
                        if group and vert_idx < len(mesh.vertices):
                            group.add([vert_idx], tw.weight, 'REPLACE')
        
        # Create shape keys for blend targets
        if data.blend_targets and options.get('import_blend_shapes', True):
            # Add basis shape key
            obj.shape_key_add(name='Basis')
            
            for blend in data.blend_targets:
                sk = obj.shape_key_add(name=blend.name)
                
                # Apply position deltas
                for delta in blend.positions:
                    if delta.index < len(sk.data):
                        current = sk.data[delta.index].co
                        # Convert delta from SWG to Blender coords
                        sk.data[delta.index].co = (
                            current.x + delta.delta.x,
                            current.y - delta.delta.z,
                            current.z + delta.delta.y
                        )
        
        # Create materials for each shader
        if options.get('import_materials', True):
            for shader in data.per_shader_data:
                if shader.shader_name:
                    mat = bpy.data.materials.new(name=shader.shader_name)
                    mat.use_nodes = True
                    mesh.materials.append(mat)
        
        # Create hardpoint empties
        for hp in data.hardpoints:
            empty = bpy.data.objects.new(f"hp_{hp.name}", None)
            empty.empty_display_type = 'ARROWS'
            empty.empty_display_size = 0.05
            
            # Set position
            empty.location = (hp.position.x, -hp.position.z, hp.position.y)
            
            # Set rotation from quaternion
            import mathutils
            quat = mathutils.Quaternion((hp.rotation.w, hp.rotation.x, 
                                          -hp.rotation.z, hp.rotation.y))
            empty.rotation_mode = 'QUATERNION'
            empty.rotation_quaternion = quat
            
            empty['swg_hardpoint'] = hp.name
            empty['swg_parent_joint'] = hp.parent_joint_name
            
            # Parent to mesh
            empty.parent = obj
            
            bpy.context.collection.objects.link(empty)
            created_objects.append(empty)
        
        return created_objects
