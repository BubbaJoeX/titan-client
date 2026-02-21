# SWG Asset Toolchain - MSH (Static Mesh) Format Handler
# Copyright (c) Titan Project
#
# Handles .msh (MeshAppearanceTemplate) files.
# Reference: D:/titan/client/src/engine/client/library/clientObject/src/shared/appearance/MeshAppearanceTemplate.cpp

from __future__ import annotations
from typing import List, Dict, Optional, Tuple, Any, TYPE_CHECKING
from dataclasses import dataclass, field
from pathlib import Path

from ..core.iff import Iff, IffError
from ..core.tag import Tag, TAG_FORM
from ..core.types import Vector3, Sphere, AxisAlignedBox
from ..core.registry import FormatHandler, ImportResult, ExportResult, register_handler

if TYPE_CHECKING:
    import bpy

__all__ = ['MshHandler', 'MeshData', 'ShaderPrimitiveData']


# =============================================================================
# Tags
# =============================================================================

TAG_MESH = Tag('MESH')
TAG_SPS  = Tag('SPS ')  # ShaderPrimitiveSet
TAG_CNT  = Tag('CNT ')
TAG_DATA = Tag('DATA')
TAG_INFO = Tag('INFO')
TAG_NAME = Tag('NAME')
TAG_SIDX = Tag('SIDX')  # Shader Index
TAG_INDX = Tag('INDX')  # Triangle Indices
TAG_VERT = Tag('VERT')  # Vertices
TAG_NORM = Tag('NORM')  # Normals
TAG_TXCO = Tag('TXCO')  # Texture Coordinates
TAG_CNTR = Tag('CNTR')  # Center
TAG_RADI = Tag('RADI')  # Radius
TAG_SPHR = Tag('SPHR')  # Sphere
TAG_EXBX = Tag('EXBX')  # Extent Box
TAG_HPTS = Tag('HPTS')  # Hardpoints
TAG_NULL = Tag('NULL')  # Null/Empty

# Version tags
TAG_0002 = Tag('0002')
TAG_0003 = Tag('0003')
TAG_0004 = Tag('0004')
TAG_0005 = Tag('0005')


# =============================================================================
# Data Structures
# =============================================================================

@dataclass
class VertexData:
    """Vertex data for a shader primitive."""
    positions: List[Vector3] = field(default_factory=list)
    normals: List[Vector3] = field(default_factory=list)
    tex_coords: List[List[Tuple[float, float]]] = field(default_factory=list)
    colors: List[Tuple[int, int, int, int]] = field(default_factory=list)


@dataclass
class ShaderPrimitiveData:
    """Data for a single shader/material in the mesh."""
    shader_name: str = ""
    vertices: VertexData = field(default_factory=VertexData)
    indices: List[int] = field(default_factory=list)
    
    @property
    def triangle_count(self) -> int:
        return len(self.indices) // 3


@dataclass
class HardpointData:
    """Hardpoint attachment point data."""
    name: str = ""
    transform: Tuple[Tuple[float, ...], ...] = ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0))


@dataclass
class ExtentData:
    """Extent/bounds data."""
    box_min: Optional[Vector3] = None
    box_max: Optional[Vector3] = None
    sphere_center: Optional[Vector3] = None
    sphere_radius: float = 0.0


@dataclass
class MeshData:
    """Complete mesh data structure."""
    version: str = "0005"
    sphere: Sphere = field(default_factory=Sphere)
    extent: ExtentData = field(default_factory=ExtentData)
    shader_primitives: List[ShaderPrimitiveData] = field(default_factory=list)
    hardpoints: List[HardpointData] = field(default_factory=list)
    unknown_chunks: Dict[str, bytes] = field(default_factory=dict)


# =============================================================================
# MSH Parser
# =============================================================================

class MshParser:
    """Parser for MSH (MeshAppearanceTemplate) files."""
    
    def __init__(self):
        self.warnings: List[str] = []
        self.unknown_chunks: Dict[str, bytes] = {}
    
    def parse(self, iff: Iff) -> MeshData:
        """
        Parse an MSH file.
        
        Args:
            iff: IFF data to parse
            
        Returns:
            MeshData containing parsed mesh data
        """
        mesh_data = MeshData()
        
        # Enter root MESH form
        iff.enter_form(TAG_MESH)
        
        # Get version
        version_tag = iff.get_current_name()
        mesh_data.version = str(version_tag)
        
        if version_tag == TAG_0005:
            self._parse_version_0005(iff, mesh_data)
        elif version_tag == TAG_0004:
            self._parse_version_0004(iff, mesh_data)
        elif version_tag == TAG_0003:
            self._parse_version_0003(iff, mesh_data)
        elif version_tag == TAG_0002:
            self._parse_version_0002(iff, mesh_data)
        else:
            self.warnings.append(f"Unknown mesh version: {version_tag}")
            # Try to parse anyway
            self._parse_version_0005(iff, mesh_data)
        
        iff.exit_form()  # MESH
        
        mesh_data.unknown_chunks = self.unknown_chunks
        return mesh_data
    
    def _parse_version_0005(self, iff: Iff, mesh_data: MeshData) -> None:
        """Parse version 0005 mesh format."""
        iff.enter_form(TAG_0005)
        
        # AppearanceTemplate::load (extent data)
        self._parse_appearance_template(iff, mesh_data)
        
        # ShaderPrimitiveSetTemplate
        self._parse_shader_primitive_set(iff, mesh_data)
        
        iff.exit_form()
    
    def _parse_version_0004(self, iff: Iff, mesh_data: MeshData) -> None:
        """Parse version 0004 mesh format."""
        iff.enter_form(TAG_0004)
        
        self._parse_appearance_template(iff, mesh_data)
        self._parse_shader_primitive_set(iff, mesh_data)
        
        iff.exit_form()
    
    def _parse_version_0003(self, iff: Iff, mesh_data: MeshData) -> None:
        """Parse version 0003 mesh format."""
        iff.enter_form(TAG_0003)
        
        self._parse_shader_primitive_set(iff, mesh_data)
        self._parse_sphere_old(iff, mesh_data)
        
        iff.exit_form()
        
        # Extent and hardpoints after version form
        if not iff.at_end_of_form():
            self._try_parse_extent(iff, mesh_data)
        if not iff.at_end_of_form():
            self._try_parse_hardpoints(iff, mesh_data)
    
    def _parse_version_0002(self, iff: Iff, mesh_data: MeshData) -> None:
        """Parse version 0002 mesh format."""
        iff.enter_form(TAG_0002)
        
        self._parse_shader_primitive_set(iff, mesh_data)
        self._parse_sphere_old(iff, mesh_data)
        
        iff.exit_form()
        
        if not iff.at_end_of_form():
            self._try_parse_extent(iff, mesh_data)
        if not iff.at_end_of_form():
            self._try_parse_hardpoints(iff, mesh_data)
    
    def _parse_appearance_template(self, iff: Iff, mesh_data: MeshData) -> None:
        """Parse AppearanceTemplate data (extent, sphere, hardpoints)."""
        while not iff.at_end_of_form():
            tag = iff.get_current_name()
            
            if tag == TAG_FORM:
                # Nested form - could be extent or SPS
                iff.enter_form()
                form_name = iff.get_current_name()
                iff.go_to_top_of_form()
                iff.exit_form()
                
                # Check if this is SPS (shader primitive set)
                if iff.is_current_form():
                    inner_name = self._peek_form_name(iff)
                    if inner_name == TAG_SPS:
                        break  # Stop - SPS will be parsed separately
                
                # Otherwise try to parse as extent
                self._try_parse_extent(iff, mesh_data)
            elif tag == TAG_HPTS:
                self._parse_hardpoints(iff, mesh_data)
            else:
                break  # Unknown block, let caller handle
    
    def _peek_form_name(self, iff: Iff) -> Optional[Tag]:
        """Peek at a form's inner name without consuming it."""
        if not iff.is_current_form():
            return None
        
        iff.enter_form()
        if iff.is_current_form():
            iff.enter_form()
            name = iff.get_current_name() if not iff.at_end_of_form() else None
            iff.exit_form()
        else:
            name = iff.get_current_name() if not iff.at_end_of_form() else None
        iff.exit_form()
        iff.go_to_top_of_form()
        
        # This is a simplification - real peek would need state restoration
        return None
    
    def _try_parse_extent(self, iff: Iff, mesh_data: MeshData) -> bool:
        """Try to parse extent data."""
        if iff.at_end_of_form():
            return False
        
        tag = iff.get_current_name()
        
        if tag == TAG_EXBX or (tag == TAG_FORM):
            if iff.is_current_form():
                iff.enter_form()
                inner_tag = iff.get_current_name()
                
                if inner_tag == TAG_EXBX or str(inner_tag).startswith('0'):
                    # Parse box extent
                    if iff.seek_chunk(TAG_DATA):
                        iff.enter_chunk()
                        # Box extent: min (x,y,z), max (x,y,z)
                        min_x = iff.read_float()
                        min_y = iff.read_float()
                        min_z = iff.read_float()
                        max_x = iff.read_float()
                        max_y = iff.read_float()
                        max_z = iff.read_float()
                        
                        mesh_data.extent.box_min = Vector3(min_x, min_y, min_z)
                        mesh_data.extent.box_max = Vector3(max_x, max_y, max_z)
                        iff.exit_chunk()
                
                iff.exit_form(may_not_be_at_end=True)
                return True
        
        return False
    
    def _try_parse_hardpoints(self, iff: Iff, mesh_data: MeshData) -> bool:
        """Try to parse hardpoints."""
        if iff.at_end_of_form():
            return False
        
        if iff.seek_form('HPTS'):
            self._parse_hardpoints(iff, mesh_data)
            return True
        
        return False
    
    def _parse_hardpoints(self, iff: Iff, mesh_data: MeshData) -> None:
        """Parse hardpoint data."""
        iff.enter_form('HPTS')
        
        while not iff.at_end_of_form():
            if iff.is_current_form():
                iff.enter_form()  # Hardpoint form
                
                hp = HardpointData()
                
                if iff.enter_chunk(TAG_NAME, optional=True):
                    hp.name = iff.read_string()
                    iff.exit_chunk()
                
                if iff.enter_chunk(TAG_DATA, optional=True):
                    # Transform matrix
                    rows = []
                    for _ in range(3):
                        row = tuple(iff.read_float() for _ in range(4))
                        rows.append(row)
                    hp.transform = tuple(rows)
                    iff.exit_chunk()
                
                mesh_data.hardpoints.append(hp)
                iff.exit_form()
            else:
                iff.go_forward()
        
        iff.exit_form()
    
    def _parse_sphere_old(self, iff: Iff, mesh_data: MeshData) -> None:
        """Parse old-style sphere (CNTR + RADI chunks)."""
        if iff.enter_chunk(TAG_CNTR, optional=True):
            x, y, z = iff.read_vector()
            mesh_data.sphere.center = Vector3(x, y, z)
            iff.exit_chunk()
        
        if iff.enter_chunk(TAG_RADI, optional=True):
            mesh_data.sphere.radius = iff.read_float()
            iff.exit_chunk()
    
    def _parse_shader_primitive_set(self, iff: Iff, mesh_data: MeshData) -> None:
        """Parse ShaderPrimitiveSetTemplate."""
        # SPS can be wrapped in a FORM or directly present
        if iff.is_current_form():
            iff.enter_form()
        
        # Look for SPS form
        if iff.seek_form('SPS '):
            iff.enter_form('SPS ')
            
            # Version form
            if iff.is_current_form():
                iff.enter_form()  # Version form
                
                # CNT chunk - count of shader primitives
                count = 0
                if iff.enter_chunk('CNT ', optional=True):
                    count = iff.read_int32()
                    iff.exit_chunk()
                
                # Parse each shader primitive
                for _ in range(count):
                    if iff.at_end_of_form():
                        break
                    
                    primitive = self._parse_shader_primitive(iff)
                    if primitive:
                        mesh_data.shader_primitives.append(primitive)
                
                iff.exit_form()  # Version
            
            iff.exit_form()  # SPS
    
    def _parse_shader_primitive(self, iff: Iff) -> Optional[ShaderPrimitiveData]:
        """Parse a single shader primitive."""
        if not iff.is_current_form():
            return None
        
        primitive = ShaderPrimitiveData()
        
        iff.enter_form()  # Shader primitive form
        
        while not iff.at_end_of_form():
            if iff.is_current_form():
                iff.enter_form()
                # Could be vertex buffer, index buffer, etc.
                tag = iff.get_current_name()
                
                if tag == Tag('VTXA') or str(tag).startswith('0'):
                    # Vertex array
                    self._parse_vertex_array(iff, primitive)
                
                iff.exit_form(may_not_be_at_end=True)
            else:
                tag = iff.get_current_name()
                
                if tag == TAG_NAME or tag == Tag('SHDR'):
                    iff.enter_chunk()
                    primitive.shader_name = iff.read_string()
                    iff.exit_chunk()
                elif tag == TAG_INDX or tag == Tag('IDX '):
                    iff.enter_chunk()
                    # Read indices - could be 16 or 32 bit
                    count = iff.get_chunk_length_left(2)  # Assume 16-bit
                    primitive.indices = iff.read_uint16_array(count)
                    iff.exit_chunk()
                elif tag == TAG_SIDX:
                    iff.enter_chunk()
                    count = iff.get_chunk_length_left(2)
                    primitive.indices = iff.read_uint16_array(count)
                    iff.exit_chunk()
                else:
                    # Store unknown chunk
                    iff.enter_chunk()
                    self.unknown_chunks[str(tag)] = iff.read_rest_bytes()
                    iff.exit_chunk()
        
        iff.exit_form()
        
        return primitive
    
    def _parse_vertex_array(self, iff: Iff, primitive: ShaderPrimitiveData) -> None:
        """Parse vertex array data."""
        while not iff.at_end_of_form():
            if iff.is_current_chunk():
                tag = iff.get_current_name()
                iff.enter_chunk()
                
                if tag == Tag('POSI') or tag == Tag('POSN'):
                    # Positions
                    count = iff.get_chunk_length_left(12)  # 3 floats
                    for _ in range(count):
                        x, y, z = iff.read_vector()
                        primitive.vertices.positions.append(Vector3(x, y, z))
                
                elif tag == TAG_NORM:
                    # Normals
                    count = iff.get_chunk_length_left(12)
                    for _ in range(count):
                        x, y, z = iff.read_vector()
                        primitive.vertices.normals.append(Vector3(x, y, z))
                
                elif str(tag).startswith('TXC') or tag == Tag('TUVS'):
                    # Texture coordinates
                    count = iff.get_chunk_length_left(8)  # 2 floats
                    uvs = []
                    for _ in range(count):
                        u = iff.read_float()
                        v = iff.read_float()
                        uvs.append((u, v))
                    primitive.vertices.tex_coords.append(uvs)
                
                else:
                    # Unknown - preserve
                    self.unknown_chunks[f"VTXA_{tag}"] = iff.read_rest_bytes()
                
                iff.exit_chunk()
            else:
                iff.go_forward()


# =============================================================================
# MSH Writer
# =============================================================================

class MshWriter:
    """Writer for MSH files."""
    
    def write(self, mesh_data: MeshData) -> Iff:
        """
        Write mesh data to an IFF.
        
        Args:
            mesh_data: Mesh data to write
            
        Returns:
            IFF containing the mesh data
        """
        iff = Iff(growable=True)
        
        iff.insert_form(TAG_MESH)
        
        version_tag = Tag(mesh_data.version) if mesh_data.version else TAG_0005
        iff.insert_form(version_tag)
        
        # Write extent if present
        if mesh_data.extent.box_min and mesh_data.extent.box_max:
            self._write_extent(iff, mesh_data.extent)
        
        # Write shader primitive set
        self._write_shader_primitive_set(iff, mesh_data)
        
        # Write hardpoints if present
        if mesh_data.hardpoints:
            self._write_hardpoints(iff, mesh_data.hardpoints)
        
        iff.exit_form()  # Version
        iff.exit_form()  # MESH
        
        return iff
    
    def _write_extent(self, iff: Iff, extent: ExtentData) -> None:
        """Write extent data."""
        iff.insert_form(TAG_EXBX)
        iff.insert_form(TAG_0000)
        
        iff.insert_chunk(TAG_DATA)
        iff.insert_float(extent.box_min.x)
        iff.insert_float(extent.box_min.y)
        iff.insert_float(extent.box_min.z)
        iff.insert_float(extent.box_max.x)
        iff.insert_float(extent.box_max.y)
        iff.insert_float(extent.box_max.z)
        iff.exit_chunk()
        
        iff.exit_form()
        iff.exit_form()
    
    def _write_shader_primitive_set(self, iff: Iff, mesh_data: MeshData) -> None:
        """Write shader primitive set."""
        iff.insert_form(TAG_SPS)
        iff.insert_form(TAG_0001)
        
        # Count
        iff.insert_chunk(TAG_CNT)
        iff.insert_int32(len(mesh_data.shader_primitives))
        iff.exit_chunk()
        
        # Each primitive
        for primitive in mesh_data.shader_primitives:
            self._write_shader_primitive(iff, primitive)
        
        iff.exit_form()
        iff.exit_form()
    
    def _write_shader_primitive(self, iff: Iff, primitive: ShaderPrimitiveData) -> None:
        """Write a single shader primitive."""
        iff.insert_form(Tag('SPST'))  # Shader Primitive SubTemplate
        
        # Shader name
        if primitive.shader_name:
            iff.insert_chunk(TAG_NAME)
            iff.insert_string(primitive.shader_name)
            iff.exit_chunk()
        
        # Vertex data
        if primitive.vertices.positions:
            iff.insert_form(Tag('VTXA'))
            iff.insert_form(TAG_0003)
            
            # Positions
            iff.insert_chunk(Tag('POSN'))
            for pos in primitive.vertices.positions:
                iff.insert_vector(pos.x, pos.y, pos.z)
            iff.exit_chunk()
            
            # Normals
            if primitive.vertices.normals:
                iff.insert_chunk(TAG_NORM)
                for norm in primitive.vertices.normals:
                    iff.insert_vector(norm.x, norm.y, norm.z)
                iff.exit_chunk()
            
            # Texture coordinates
            for i, uvs in enumerate(primitive.vertices.tex_coords):
                tag = Tag(f'TXC{i}')
                iff.insert_chunk(tag)
                for u, v in uvs:
                    iff.insert_float(u)
                    iff.insert_float(v)
                iff.exit_chunk()
            
            iff.exit_form()
            iff.exit_form()
        
        # Indices
        if primitive.indices:
            iff.insert_chunk(TAG_SIDX)
            for idx in primitive.indices:
                iff.insert_uint16(idx)
            iff.exit_chunk()
        
        iff.exit_form()
    
    def _write_hardpoints(self, iff: Iff, hardpoints: List[HardpointData]) -> None:
        """Write hardpoints."""
        iff.insert_form(Tag('HPTS'))
        iff.insert_form(TAG_0001)
        
        for hp in hardpoints:
            iff.insert_form(Tag('HPNT'))
            
            iff.insert_chunk(TAG_NAME)
            iff.insert_string(hp.name)
            iff.exit_chunk()
            
            iff.insert_chunk(TAG_DATA)
            for row in hp.transform:
                for val in row:
                    iff.insert_float(val)
            iff.exit_chunk()
            
            iff.exit_form()
        
        iff.exit_form()
        iff.exit_form()


# =============================================================================
# Blender Integration
# =============================================================================

TAG_0000 = Tag('0000')
TAG_0001 = Tag('0001')


@register_handler
class MshHandler(FormatHandler):
    """Handler for MSH (static mesh) files."""
    
    FORMAT_NAME = "MSH"
    FORMAT_DESCRIPTION = "SWG Static Mesh (MeshAppearanceTemplate)"
    FILE_EXTENSIONS = ["msh"]
    ROOT_TAGS = [TAG_MESH]
    CAN_IMPORT = True
    CAN_EXPORT = True
    IS_FULLY_UNDERSTOOD = False  # Some chunks may be unknown
    
    def import_file(self, filepath: str, **options) -> ImportResult:
        """Import MSH file to Blender."""
        try:
            # Parse IFF
            iff = Iff.from_file(filepath)
            parser = MshParser()
            mesh_data = parser.parse(iff)
            
            self.warnings.extend(parser.warnings)
            self.unknown_chunks = parser.unknown_chunks
            
            # Create Blender objects
            objects = self._create_blender_objects(mesh_data, filepath, **options)
            
            return ImportResult(
                success=True,
                message=f"Imported {len(mesh_data.shader_primitives)} shader primitives",
                warnings=self.warnings,
                objects=objects,
                unknown_chunks=self.unknown_chunks,
            )
            
        except Exception as e:
            return ImportResult(
                success=False,
                message=f"Failed to import: {str(e)}",
                warnings=self.warnings,
            )
    
    def export_file(self, filepath: str, objects: List[Any], **options) -> ExportResult:
        """Export Blender objects to MSH file."""
        try:
            # Convert Blender objects to mesh data
            mesh_data = self._convert_from_blender(objects, **options)
            
            # Write IFF
            writer = MshWriter()
            iff = writer.write(mesh_data)
            
            # Save file
            iff.write(filepath)
            
            return ExportResult(
                success=True,
                message=f"Exported to {filepath}",
                warnings=self.warnings,
                bytes_written=iff.raw_data_size,
            )
            
        except Exception as e:
            return ExportResult(
                success=False,
                message=f"Failed to export: {str(e)}",
                warnings=self.warnings,
            )
    
    def _create_blender_objects(self, mesh_data: MeshData, filepath: str, 
                                 **options) -> List[Any]:
        """Create Blender mesh objects from parsed data."""
        import bpy
        import bmesh
        
        created_objects = []
        base_name = Path(filepath).stem
        
        for i, primitive in enumerate(mesh_data.shader_primitives):
            # Create mesh
            mesh_name = f"{base_name}_{i}"
            mesh = bpy.data.meshes.new(mesh_name)
            
            # Create bmesh for construction
            bm = bmesh.new()
            
            # Add vertices
            verts = []
            for pos in primitive.vertices.positions:
                # Convert coordinates (SWG Y-up to Blender Z-up)
                v = bm.verts.new((pos.x, -pos.z, pos.y))
                verts.append(v)
            
            bm.verts.ensure_lookup_table()
            
            # Add faces from triangle indices
            if primitive.indices:
                for j in range(0, len(primitive.indices), 3):
                    if j + 2 < len(primitive.indices):
                        try:
                            i0, i1, i2 = primitive.indices[j:j+3]
                            if i0 < len(verts) and i1 < len(verts) and i2 < len(verts):
                                # Reverse winding for Blender
                                bm.faces.new((verts[i0], verts[i2], verts[i1]))
                        except ValueError:
                            # Degenerate triangle
                            pass
            
            # Transfer to mesh
            bm.to_mesh(mesh)
            bm.free()
            
            # Add normals if present
            if primitive.vertices.normals and options.get('import_normals', True):
                mesh.calc_normals_split()
                # Custom normals would go here
            
            # Add UV layers
            if primitive.vertices.tex_coords:
                for uv_idx, uvs in enumerate(primitive.vertices.tex_coords):
                    uv_layer = mesh.uv_layers.new(name=f"UVMap_{uv_idx}")
                    
                    # Map UVs to loops
                    for poly in mesh.polygons:
                        for loop_idx in poly.loop_indices:
                            vert_idx = mesh.loops[loop_idx].vertex_index
                            if vert_idx < len(uvs):
                                u, v = uvs[vert_idx]
                                uv_layer.data[loop_idx].uv = (u, 1.0 - v)  # Flip V
            
            mesh.update()
            
            # Create object
            obj = bpy.data.objects.new(mesh_name, mesh)
            
            # Store metadata
            obj['swg_shader'] = primitive.shader_name
            obj['swg_format'] = 'MSH'
            obj['swg_version'] = mesh_data.version
            
            # Link to scene
            bpy.context.collection.objects.link(obj)
            created_objects.append(obj)
            
            # Create material if requested
            if options.get('import_materials', True) and primitive.shader_name:
                mat = bpy.data.materials.new(name=primitive.shader_name)
                mat.use_nodes = True
                obj.data.materials.append(mat)
        
        # Create hardpoint empties
        for hp in mesh_data.hardpoints:
            empty = bpy.data.objects.new(f"hp_{hp.name}", None)
            empty.empty_display_type = 'ARROWS'
            empty.empty_display_size = 0.1
            
            # Set transform
            import mathutils
            matrix = mathutils.Matrix((
                (hp.transform[0][0], -hp.transform[0][2], hp.transform[0][1], hp.transform[0][3]),
                (-hp.transform[2][0], hp.transform[2][2], -hp.transform[2][1], -hp.transform[2][3]),
                (hp.transform[1][0], -hp.transform[1][2], hp.transform[1][1], hp.transform[1][3]),
                (0, 0, 0, 1)
            ))
            empty.matrix_world = matrix
            
            empty['swg_hardpoint'] = hp.name
            bpy.context.collection.objects.link(empty)
            created_objects.append(empty)
        
        return created_objects
    
    def _convert_from_blender(self, objects: List[Any], **options) -> MeshData:
        """Convert Blender objects to MeshData."""
        import bpy
        import bmesh
        
        mesh_data = MeshData()
        mesh_data.version = options.get('export_version', '0005')
        
        # Calculate bounds
        min_corner = [float('inf')] * 3
        max_corner = [float('-inf')] * 3
        
        for obj in objects:
            if obj.type == 'MESH':
                # Apply modifiers if requested
                if options.get('apply_modifiers', True):
                    depsgraph = bpy.context.evaluated_depsgraph_get()
                    obj_eval = obj.evaluated_get(depsgraph)
                    mesh = obj_eval.to_mesh()
                else:
                    mesh = obj.data
                
                primitive = ShaderPrimitiveData()
                
                # Get shader name from material or property
                if obj.get('swg_shader'):
                    primitive.shader_name = obj['swg_shader']
                elif mesh.materials:
                    primitive.shader_name = mesh.materials[0].name
                
                # Export vertices
                for vert in mesh.vertices:
                    # Convert Blender Z-up to SWG Y-up
                    pos = Vector3(vert.co.x, vert.co.z, -vert.co.y)
                    primitive.vertices.positions.append(pos)
                    
                    norm = Vector3(vert.normal.x, vert.normal.z, -vert.normal.y)
                    primitive.vertices.normals.append(norm)
                    
                    # Update bounds
                    for i in range(3):
                        min_corner[i] = min(min_corner[i], pos[i])
                        max_corner[i] = max(max_corner[i], pos[i])
                
                # Export UVs
                if mesh.uv_layers:
                    uvs = [None] * len(mesh.vertices)
                    for uv_layer in mesh.uv_layers:
                        for poly in mesh.polygons:
                            for i, loop_idx in enumerate(poly.loop_indices):
                                vert_idx = mesh.loops[loop_idx].vertex_index
                                uv = uv_layer.data[loop_idx].uv
                                uvs[vert_idx] = (uv.x, 1.0 - uv.y)  # Flip V back
                        
                        # Replace None with (0, 0)
                        uvs = [(0, 0) if uv is None else uv for uv in uvs]
                        primitive.vertices.tex_coords.append(uvs)
                
                # Export triangles
                mesh.calc_loop_triangles()
                for tri in mesh.loop_triangles:
                    # Reverse winding back to SWG convention
                    primitive.indices.extend([
                        tri.vertices[0],
                        tri.vertices[2],
                        tri.vertices[1]
                    ])
                
                mesh_data.shader_primitives.append(primitive)
                
                if options.get('apply_modifiers', True):
                    obj_eval.to_mesh_clear()
            
            elif obj.type == 'EMPTY' and obj.get('swg_hardpoint'):
                # Export hardpoint
                hp = HardpointData()
                hp.name = obj['swg_hardpoint']
                
                # Convert transform
                m = obj.matrix_world
                hp.transform = (
                    (m[0][0], m[0][2], -m[0][1], m[0][3]),
                    (m[2][0], m[2][2], -m[2][1], m[2][3]),
                    (-m[1][0], -m[1][2], m[1][1], -m[1][3]),
                )
                mesh_data.hardpoints.append(hp)
        
        # Set extent
        if min_corner[0] != float('inf'):
            mesh_data.extent.box_min = Vector3(*min_corner)
            mesh_data.extent.box_max = Vector3(*max_corner)
            
            # Calculate sphere
            center = Vector3(
                (min_corner[0] + max_corner[0]) / 2,
                (min_corner[1] + max_corner[1]) / 2,
                (min_corner[2] + max_corner[2]) / 2,
            )
            radius = max(
                max_corner[0] - min_corner[0],
                max_corner[1] - min_corner[1],
                max_corner[2] - min_corner[2],
            ) / 2
            mesh_data.sphere = Sphere(center, radius)
        
        return mesh_data
