# SWG Asset Toolchain for Blender
# Copyright (c) Titan Project
# Complete toolchain for Star Wars Galaxies asset formats

bl_info = {
    'name': 'SWG Asset Toolchain',
    'author': 'Titan Project',
    'version': (1, 0, 0),
    'blender': (4, 0, 0),
    'location': 'File > Import/Export, View3D > Sidebar > SWG',
    'description': 'Import/Export for SWG formats (.msh, .mgn, .skt, .ans)',
    'category': 'Import-Export',
}

import bpy
from bpy.props import StringProperty, BoolProperty, EnumProperty
from bpy.types import Operator, Panel, AddonPreferences
from pathlib import Path
import struct
import traceback

# =============================================================================
# IFF Parser
# =============================================================================

class IffError(Exception):
    pass


class Tag:
    __slots__ = ('_value',)
    
    def __init__(self, value):
        if isinstance(value, Tag):
            self._value = value._value
        elif isinstance(value, str):
            s = value[:4].ljust(4)
            self._value = (ord(s[0]) << 24) | (ord(s[1]) << 16) | (ord(s[2]) << 8) | ord(s[3])
        elif isinstance(value, int):
            self._value = value & 0xFFFFFFFF
        elif isinstance(value, bytes):
            self._value = struct.unpack('>I', value[:4])[0]
        else:
            self._value = 0
    
    def __eq__(self, other):
        if isinstance(other, Tag):
            return self._value == other._value
        elif isinstance(other, str):
            return self._value == Tag(other)._value
        return False
    
    def __hash__(self):
        return hash(self._value)
    
    def __str__(self):
        chars = []
        for i in range(4):
            b = (self._value >> (24 - i * 8)) & 0xFF
            chars.append(chr(b) if 32 <= b < 127 else '?')
        return ''.join(chars)


TAG_FORM = Tag('FORM')


class Iff:
    def __init__(self, data=None):
        self._data = bytearray(data) if data else bytearray()
        self._stack = [{'start': 0, 'length': len(self._data), 'used': 0}]
        self._stack_depth = 0
        self._in_chunk = False
    
    @classmethod
    def from_file(cls, filepath):
        with open(filepath, 'rb') as f:
            data = f.read()
        return cls(data)
    
    def _current_pos(self):
        return self._stack[self._stack_depth]['start'] + self._stack[self._stack_depth]['used']
    
    def _bytes_left(self):
        return self._stack[self._stack_depth]['length'] - self._stack[self._stack_depth]['used']
    
    def _get_tag(self, offset=0):
        pos = self._current_pos() + offset
        if pos + 4 > len(self._data):
            return Tag(0)
        return Tag(struct.unpack_from('>I', self._data, pos)[0])
    
    def _get_length(self, offset=0):
        pos = self._current_pos() + offset + 4
        if pos + 4 > len(self._data):
            return 0
        return struct.unpack_from('>I', self._data, pos)[0]
    
    def get_current_name(self):
        tag = self._get_tag()
        if tag == TAG_FORM:
            return self._get_tag(8)
        return tag
    
    def is_current_form(self):
        if self._bytes_left() < 8:
            return False
        return self._get_tag() == TAG_FORM
    
    def at_end_of_form(self):
        return self._bytes_left() < 8
    
    def enter_form(self, name=None, optional=False):
        if self._in_chunk:
            if optional:
                return False
            raise IffError("Cannot enter form while in chunk")
        
        if self._bytes_left() < 12:
            if optional:
                return False
            raise IffError("Not enough data for form")
        
        tag = self._get_tag()
        if tag != TAG_FORM:
            if optional:
                return False
            raise IffError(f"Expected FORM, got {tag}")
        
        if name is not None:
            form_name = self._get_tag(8)
            if form_name != Tag(name):
                if optional:
                    return False
                raise IffError(f"Expected form {name}, got {form_name}")
        
        length = self._get_length()
        
        self._stack_depth += 1
        if self._stack_depth >= len(self._stack):
            self._stack.append({})
        
        self._stack[self._stack_depth] = {
            'start': self._current_pos() - self._stack[self._stack_depth - 1]['used'] + self._stack[self._stack_depth - 1]['used'] + 12,
            'length': length - 4,
            'used': 0
        }
        # Fix: recalculate start position
        self._stack[self._stack_depth]['start'] = self._stack[self._stack_depth - 1]['start'] + self._stack[self._stack_depth - 1]['used'] + 12
        self._stack[self._stack_depth - 1]['used'] += length + 8
        
        return True
    
    def exit_form(self):
        if self._in_chunk:
            raise IffError("Cannot exit form while in chunk")
        if self._stack_depth == 0:
            raise IffError("Cannot exit root")
        self._stack_depth -= 1
    
    def enter_chunk(self, name=None, optional=False):
        if self._in_chunk:
            if optional:
                return False
            raise IffError("Already in chunk")
        
        if self._bytes_left() < 8:
            if optional:
                return False
            raise IffError("Not enough data for chunk")
        
        tag = self._get_tag()
        if tag == TAG_FORM:
            if optional:
                return False
            raise IffError(f"Expected chunk, got FORM")
        
        if name is not None:
            if tag != Tag(name):
                if optional:
                    return False
                raise IffError(f"Expected chunk {name}, got {tag}")
        
        length = self._get_length()
        
        self._stack_depth += 1
        if self._stack_depth >= len(self._stack):
            self._stack.append({})
        
        self._stack[self._stack_depth] = {
            'start': self._stack[self._stack_depth - 1]['start'] + self._stack[self._stack_depth - 1]['used'] + 8,
            'length': length,
            'used': 0
        }
        self._stack[self._stack_depth - 1]['used'] += length + 8
        self._in_chunk = True
        
        return True
    
    def exit_chunk(self):
        if not self._in_chunk:
            raise IffError("Not in chunk")
        self._in_chunk = False
        self._stack_depth -= 1
    
    def skip_block(self):
        """Skip the current form or chunk."""
        if self._bytes_left() < 8:
            return False
        length = self._get_length()
        self._stack[self._stack_depth]['used'] += length + 8
        return True
    
    def get_chunk_length_left(self, element_size=1):
        if not self._in_chunk:
            return 0
        left = self._stack[self._stack_depth]['length'] - self._stack[self._stack_depth]['used']
        return left // element_size
    
    def _read(self, length):
        if not self._in_chunk:
            raise IffError("Not in chunk")
        entry = self._stack[self._stack_depth]
        if entry['used'] + length > entry['length']:
            length = entry['length'] - entry['used']
            if length <= 0:
                return bytes()
        start = entry['start'] + entry['used']
        entry['used'] += length
        return self._data[start:start + length]
    
    def read_int8(self):
        data = self._read(1)
        return struct.unpack('b', data)[0] if len(data) == 1 else 0
    
    def read_uint8(self):
        data = self._read(1)
        return data[0] if len(data) == 1 else 0
    
    def read_int16(self):
        data = self._read(2)
        return struct.unpack('>h', data)[0] if len(data) == 2 else 0
    
    def read_uint16(self):
        data = self._read(2)
        return struct.unpack('>H', data)[0] if len(data) == 2 else 0
    
    def read_int32(self):
        data = self._read(4)
        return struct.unpack('>i', data)[0] if len(data) == 4 else 0
    
    def read_uint32(self):
        data = self._read(4)
        return struct.unpack('>I', data)[0] if len(data) == 4 else 0
    
    def read_float(self):
        data = self._read(4)
        return struct.unpack('>f', data)[0] if len(data) == 4 else 0.0
    
    def read_string(self):
        chars = []
        while self.get_chunk_length_left() > 0:
            b = self._read(1)
            if not b or b[0] == 0:
                break
            chars.append(chr(b[0]))
        return ''.join(chars)
    
    def read_vector(self):
        return (self.read_float(), self.read_float(), self.read_float())
    
    def read_quaternion(self):
        return (self.read_float(), self.read_float(), self.read_float(), self.read_float())
    
    def read_rest(self):
        left = self.get_chunk_length_left()
        return bytes(self._read(left)) if left > 0 else bytes()
    
    def dump_structure(self):
        """Debug: dump IFF structure."""
        self._stack_depth = 0
        self._stack[0]['used'] = 0
        self._in_chunk = False
        
        lines = []
        self._dump_recursive(lines, 0)
        return '\n'.join(lines)
    
    def _dump_recursive(self, lines, indent):
        while not self.at_end_of_form():
            tag = self._get_tag()
            length = self._get_length()
            prefix = '  ' * indent
            
            if tag == TAG_FORM:
                form_name = self._get_tag(8)
                lines.append(f"{prefix}[FORM] {form_name} ({length} bytes)")
                self.enter_form()
                self._dump_recursive(lines, indent + 1)
                self.exit_form()
            else:
                lines.append(f"{prefix}[{tag}] ({length} bytes)")
                self.skip_block()


# =============================================================================
# Import Result
# =============================================================================

class ImportResult:
    def __init__(self, success, message, objects=None):
        self.success = success
        self.message = message
        self.objects = objects or []


# =============================================================================
# MSH Handler - Static Mesh (versions 0002-0005)
# =============================================================================

class MshHandler:
    def __init__(self, context):
        self.context = context
        self.warnings = []
    
    def import_file(self, filepath, **options):
        try:
            iff = Iff.from_file(filepath)
            
            # Enter MESH form
            if not iff.enter_form('MESH', optional=True):
                return ImportResult(False, "Not a valid MSH file - missing MESH form")
            
            version = str(iff.get_current_name())
            print(f"MSH version: {version}")
            
            # Enter version form
            if not iff.enter_form(version, optional=True):
                return ImportResult(False, f"Failed to enter version form {version}")
            
            mesh_data = self._parse_mesh(iff, version)
            
            iff.exit_form()  # version
            iff.exit_form()  # MESH
            
            objects = self._create_blender_objects(mesh_data, filepath, **options)
            
            return ImportResult(True, f"Imported mesh v{version}: {len(objects)} object(s)", objects=objects)
            
        except Exception as e:
            traceback.print_exc()
            return ImportResult(False, f"Failed: {str(e)}")
    
    def _parse_mesh(self, iff, version):
        data = {
            'version': version,
            'shaders': [],
            'hardpoints': [],
        }
        
        # Version 0004/0005 have appearance template data first
        if version in ('0004', '0005'):
            # Skip appearance template forms until we hit SPS
            while not iff.at_end_of_form():
                if iff.is_current_form():
                    name = str(iff.get_current_name())
                    if name == 'SPS ':
                        break
                    elif name == 'HPTS':
                        self._parse_hardpoints(iff, data)
                    else:
                        iff.skip_block()
                else:
                    iff.skip_block()
        
        # Parse ShaderPrimitiveSet
        if iff.enter_form('SPS ', optional=True):
            self._parse_sps(iff, data)
            iff.exit_form()
        
        # Version 0002/0003 have extent sphere after SPS
        if version in ('0002', '0003'):
            # Skip CNTR and RADI chunks
            while not iff.at_end_of_form():
                if not iff.is_current_form():
                    tag = str(iff.get_current_name())
                    if tag in ('CNTR', 'RADI'):
                        iff.enter_chunk()
                        iff.read_rest()
                        iff.exit_chunk()
                    else:
                        iff.skip_block()
                else:
                    break
        
        return data
    
    def _parse_sps(self, iff, data):
        """Parse ShaderPrimitiveSetTemplate."""
        # Enter version form (usually 0001)
        if not iff.is_current_form():
            return
        
        iff.enter_form()
        
        # Read count
        shader_count = 0
        if iff.enter_chunk('CNT ', optional=True):
            shader_count = iff.read_int32()
            iff.exit_chunk()
        
        # Parse each shader primitive
        for _ in range(shader_count):
            if iff.at_end_of_form():
                break
            if iff.is_current_form():
                shader = self._parse_shader_primitive(iff)
                if shader and shader['positions']:
                    data['shaders'].append(shader)
            else:
                iff.skip_block()
        
        iff.exit_form()
    
    def _parse_shader_primitive(self, iff):
        """Parse a single shader primitive."""
        shader = {
            'name': '',
            'positions': [],
            'normals': [],
            'uvs': [],
            'indices': [],
        }
        
        # Could be various primitive types (IDTL, SIDX, etc.)
        iff.enter_form()
        
        while not iff.at_end_of_form():
            if iff.is_current_form():
                name = str(iff.get_current_name())
                if name in ('VTXA', '0000', '0001', '0002', '0003'):
                    self._parse_vertex_data(iff, shader)
                else:
                    iff.skip_block()
            else:
                tag = str(iff.get_current_name())
                iff.enter_chunk()
                
                if tag == 'NAME':
                    shader['name'] = iff.read_string()
                elif tag in ('PIDX', 'NIDX'):
                    # Position/normal indices - 32-bit
                    count = iff.get_chunk_length_left(4)
                    for _ in range(count):
                        iff.read_int32()
                elif tag in ('SIDX', 'INDX', 'IDX '):
                    # Triangle indices - 16-bit
                    count = iff.get_chunk_length_left(2)
                    shader['indices'] = [iff.read_uint16() for _ in range(count)]
                elif tag == 'INFO':
                    # Shader info
                    iff.read_rest()
                else:
                    iff.read_rest()
                
                iff.exit_chunk()
        
        iff.exit_form()
        return shader
    
    def _parse_vertex_data(self, iff, shader):
        """Parse vertex array data (VTXA or version forms)."""
        iff.enter_form()
        
        # May have another version form inside
        if iff.is_current_form():
            iff.enter_form()
            self._parse_vertex_chunks(iff, shader)
            iff.exit_form()
        else:
            self._parse_vertex_chunks(iff, shader)
        
        iff.exit_form()
    
    def _parse_vertex_chunks(self, iff, shader):
        """Parse the actual vertex data chunks."""
        while not iff.at_end_of_form():
            if iff.is_current_form():
                iff.skip_block()
                continue
            
            tag = str(iff.get_current_name())
            iff.enter_chunk()
            
            if tag in ('POSN', 'POSI', 'DATA'):
                if tag == 'DATA':
                    # DATA chunk has vertex format info first
                    # Skip format flags (4 bytes) and vertex count (4 bytes) for now
                    # This is a simplification - real format is more complex
                    pass
                count = iff.get_chunk_length_left(12)
                shader['positions'] = [iff.read_vector() for _ in range(count)]
            elif tag == 'NORM':
                count = iff.get_chunk_length_left(12)
                shader['normals'] = [iff.read_vector() for _ in range(count)]
            elif tag.startswith('TC') or tag.startswith('TX') or tag == 'TUVS':
                count = iff.get_chunk_length_left(8)
                if not shader['uvs']:  # Only use first UV set
                    shader['uvs'] = [(iff.read_float(), iff.read_float()) for _ in range(count)]
                else:
                    iff.read_rest()
            else:
                iff.read_rest()
            
            iff.exit_chunk()
    
    def _parse_hardpoints(self, iff, data):
        """Parse hardpoints."""
        if not iff.enter_form('HPTS', optional=True):
            return
        
        # Version form
        if iff.is_current_form():
            iff.enter_form()
            
            while not iff.at_end_of_form():
                if iff.is_current_form():
                    hp = {'name': '', 'matrix': None}
                    iff.enter_form()
                    
                    while not iff.at_end_of_form():
                        if not iff.is_current_form():
                            tag = str(iff.get_current_name())
                            iff.enter_chunk()
                            if tag == 'NAME':
                                hp['name'] = iff.read_string()
                            else:
                                iff.read_rest()
                            iff.exit_chunk()
                        else:
                            iff.skip_block()
                    
                    iff.exit_form()
                    if hp['name']:
                        data['hardpoints'].append(hp)
                else:
                    iff.skip_block()
            
            iff.exit_form()
        
        iff.exit_form()
    
    def _create_blender_objects(self, data, filepath, **options):
        """Create Blender mesh objects."""
        import bmesh
        
        objects = []
        base_name = Path(filepath).stem
        
        for i, shader in enumerate(data['shaders']):
            if not shader['positions']:
                continue
            
            mesh_name = f"{base_name}_{i}" if len(data['shaders']) > 1 else base_name
            mesh = bpy.data.meshes.new(mesh_name)
            bm = bmesh.new()
            
            # Add vertices (convert Y-up to Z-up)
            verts = []
            for pos in shader['positions']:
                v = bm.verts.new((pos[0], -pos[2], pos[1]))
                verts.append(v)
            
            bm.verts.ensure_lookup_table()
            
            # Add faces from triangle indices
            indices = shader['indices']
            for j in range(0, len(indices) - 2, 3):
                i0, i1, i2 = indices[j], indices[j+1], indices[j+2]
                if i0 < len(verts) and i1 < len(verts) and i2 < len(verts):
                    if i0 != i1 and i1 != i2 and i0 != i2:
                        try:
                            # Reverse winding for Blender
                            bm.faces.new((verts[i0], verts[i2], verts[i1]))
                        except ValueError:
                            pass  # Duplicate face
            
            bm.to_mesh(mesh)
            bm.free()
            
            # Add UVs
            if shader['uvs'] and len(shader['uvs']) == len(shader['positions']):
                uv_layer = mesh.uv_layers.new(name="UVMap")
                for poly in mesh.polygons:
                    for loop_idx in poly.loop_indices:
                        vert_idx = mesh.loops[loop_idx].vertex_index
                        if vert_idx < len(shader['uvs']):
                            u, v = shader['uvs'][vert_idx]
                            uv_layer.data[loop_idx].uv = (u, 1.0 - v)
            
            mesh.update()
            
            # Create object
            obj = bpy.data.objects.new(mesh_name, mesh)
            obj['swg_format'] = 'MSH'
            obj['swg_version'] = data['version']
            obj['swg_shader'] = shader['name']
            
            bpy.context.collection.objects.link(obj)
            objects.append(obj)
            
            # Create placeholder material
            if shader['name']:
                mat = bpy.data.materials.new(name=shader['name'])
                mat.use_nodes = True
                mesh.materials.append(mat)
        
        return objects
    
    def export_file(self, filepath, objects, **options):
        return ImportResult(False, "Export not yet implemented")


# =============================================================================
# SKT Handler - Skeleton (versions 0001-0002)
# =============================================================================

class SktHandler:
    def __init__(self, context):
        self.context = context
    
    def import_file(self, filepath, **options):
        try:
            iff = Iff.from_file(filepath)
            
            if not iff.enter_form('SKTM', optional=True):
                return ImportResult(False, "Not a valid SKT file")
            
            version = str(iff.get_current_name())
            iff.enter_form(version)
            
            joints = self._parse_skeleton(iff, version)
            
            iff.exit_form()
            iff.exit_form()
            
            objects = self._create_armature(joints, filepath, version)
            
            return ImportResult(True, f"Imported skeleton v{version}: {len(joints)} bones", objects=objects)
            
        except Exception as e:
            traceback.print_exc()
            return ImportResult(False, f"Failed: {str(e)}")
    
    def _parse_skeleton(self, iff, version):
        joints = []
        
        # INFO
        if iff.enter_chunk('INFO', optional=True):
            joint_count = iff.read_int32()
            iff.exit_chunk()
            joints = [{'name': '', 'parent': -1, 'translation': (0,0,0), 'rotation': (1,0,0,0)} 
                      for _ in range(joint_count)]
        else:
            return joints
        
        # NAME
        if iff.enter_chunk('NAME', optional=True):
            for i in range(len(joints)):
                joints[i]['name'] = iff.read_string()
            iff.exit_chunk()
        
        # PRNT
        if iff.enter_chunk('PRNT', optional=True):
            for i in range(len(joints)):
                joints[i]['parent'] = iff.read_int32()
            iff.exit_chunk()
        
        # RPRE
        if iff.enter_chunk('RPRE', optional=True):
            for i in range(len(joints)):
                joints[i]['pre_rot'] = iff.read_quaternion()
            iff.exit_chunk()
        
        # RPST
        if iff.enter_chunk('RPST', optional=True):
            for i in range(len(joints)):
                joints[i]['post_rot'] = iff.read_quaternion()
            iff.exit_chunk()
        
        # BPTR
        if iff.enter_chunk('BPTR', optional=True):
            for i in range(len(joints)):
                joints[i]['translation'] = iff.read_vector()
            iff.exit_chunk()
        
        # BPRO
        if iff.enter_chunk('BPRO', optional=True):
            for i in range(len(joints)):
                joints[i]['rotation'] = iff.read_quaternion()
            iff.exit_chunk()
        
        # Skip remaining chunks
        while not iff.at_end_of_form():
            iff.skip_block()
        
        return joints
    
    def _create_armature(self, joints, filepath, version):
        import mathutils
        
        name = Path(filepath).stem
        armature = bpy.data.armatures.new(name)
        armature_obj = bpy.data.objects.new(name, armature)
        
        bpy.context.collection.objects.link(armature_obj)
        bpy.context.view_layer.objects.active = armature_obj
        
        bpy.ops.object.mode_set(mode='EDIT')
        
        bone_map = {}
        for i, joint in enumerate(joints):
            bone = armature.edit_bones.new(joint['name'])
            bone_map[i] = bone
            
            pos = joint['translation']
            bone.head = (pos[0], -pos[2], pos[1])
            bone.tail = (pos[0], -pos[2], pos[1] + 0.1)
        
        for i, joint in enumerate(joints):
            if 0 <= joint['parent'] < len(joints):
                bone_map[i].parent = bone_map.get(joint['parent'])
        
        for i, joint in enumerate(joints):
            bone = bone_map[i]
            children = [j for j, jt in enumerate(joints) if jt['parent'] == i]
            if children:
                bone.tail = bone_map[children[0]].head
            elif joint['parent'] >= 0 and joint['parent'] in bone_map:
                parent = bone_map[joint['parent']]
                direction = mathutils.Vector(bone.head) - mathutils.Vector(parent.head)
                if direction.length > 0.001:
                    direction.normalize()
                    bone.tail = mathutils.Vector(bone.head) + direction * 0.1
        
        bpy.ops.object.mode_set(mode='OBJECT')
        
        armature_obj['swg_format'] = 'SKT'
        armature_obj['swg_version'] = version
        
        return [armature_obj]
    
    def export_file(self, filepath, objects, **options):
        return ImportResult(False, "Export not yet implemented")


# =============================================================================
# MGN Handler - Mesh Generator (versions 0002-0004)
# =============================================================================

class MgnHandler:
    def __init__(self, context):
        self.context = context
    
    def import_file(self, filepath, **options):
        try:
            iff = Iff.from_file(filepath)
            
            if not iff.enter_form('SKMG', optional=True):
                return ImportResult(False, "Not a valid MGN file")
            
            version = str(iff.get_current_name())
            iff.enter_form(version)
            
            data = self._parse_mgn(iff, version)
            
            iff.exit_form()
            iff.exit_form()
            
            objects = self._create_objects(data, filepath)
            
            return ImportResult(True, f"Imported MGN v{version}: {len(objects)} object(s)", objects=objects)
            
        except Exception as e:
            traceback.print_exc()
            return ImportResult(False, f"Failed: {str(e)}")
    
    def _parse_mgn(self, iff, version):
        data = {
            'version': version,
            'skeleton': '',
            'bone_names': [],
            'positions': [],
            'normals': [],
            'shaders': [],
        }
        
        # INFO
        position_count = 0
        if iff.enter_chunk('INFO', optional=True):
            iff.read_int32()  # max transforms per vertex
            iff.read_int32()  # max transforms per shader
            position_count = iff.read_int32()
            iff.read_rest()
            iff.exit_chunk()
        
        # SKTM - skeleton name
        if iff.enter_chunk('SKTM', optional=True):
            data['skeleton'] = iff.read_string()
            iff.exit_chunk()
        
        # XFNM - bone names
        if iff.enter_chunk('XFNM', optional=True):
            while iff.get_chunk_length_left() > 0:
                data['bone_names'].append(iff.read_string())
            iff.exit_chunk()
        
        # POSN - positions
        if iff.enter_chunk('POSN', optional=True):
            for _ in range(position_count):
                if iff.get_chunk_length_left() >= 12:
                    data['positions'].append(iff.read_vector())
            iff.exit_chunk()
        
        # Skip to PSDT forms
        while not iff.at_end_of_form():
            if iff.is_current_form():
                name = str(iff.get_current_name())
                if name == 'PSDT':
                    shader = self._parse_psdt(iff, data['positions'])
                    if shader and shader['positions']:
                        data['shaders'].append(shader)
                else:
                    iff.skip_block()
            else:
                iff.skip_block()
        
        return data
    
    def _parse_psdt(self, iff, all_positions):
        """Parse per-shader data."""
        shader = {
            'name': '',
            'positions': [],
            'uvs': [],
            'indices': [],
            'pidx': [],
        }
        
        iff.enter_form()
        
        # Version form
        if iff.is_current_form():
            iff.enter_form()
            
            while not iff.at_end_of_form():
                if iff.is_current_form():
                    name = str(iff.get_current_name())
                    if name == 'PRIM':
                        self._parse_prim(iff, shader)
                    else:
                        iff.skip_block()
                else:
                    tag = str(iff.get_current_name())
                    iff.enter_chunk()
                    
                    if tag == 'NAME':
                        shader['name'] = iff.read_string()
                    elif tag == 'PIDX':
                        count = iff.get_chunk_length_left(4)
                        shader['pidx'] = [iff.read_int32() for _ in range(count)]
                    elif tag.startswith('TCS'):
                        if not shader['uvs']:
                            count = iff.get_chunk_length_left(8)
                            shader['uvs'] = [(iff.read_float(), iff.read_float()) for _ in range(count)]
                        else:
                            iff.read_rest()
                    else:
                        iff.read_rest()
                    
                    iff.exit_chunk()
            
            iff.exit_form()
        
        iff.exit_form()
        
        # Map positions using PIDX
        for idx in shader['pidx']:
            if 0 <= idx < len(all_positions):
                shader['positions'].append(all_positions[idx])
        
        return shader
    
    def _parse_prim(self, iff, shader):
        """Parse primitive data."""
        iff.enter_form()
        
        while not iff.at_end_of_form():
            if not iff.is_current_form():
                tag = str(iff.get_current_name())
                iff.enter_chunk()
                
                if tag in ('ITL ', 'OITL'):
                    if tag == 'OITL':
                        iff.read_int16()  # zone
                    tri_count = iff.read_int32()
                    for _ in range(tri_count * 3):
                        if iff.get_chunk_length_left() >= 4:
                            shader['indices'].append(iff.read_int32())
                else:
                    iff.read_rest()
                
                iff.exit_chunk()
            else:
                iff.skip_block()
        
        iff.exit_form()
    
    def _create_objects(self, data, filepath):
        import bmesh
        
        objects = []
        base_name = Path(filepath).stem
        
        for i, shader in enumerate(data['shaders']):
            if not shader['positions']:
                continue
            
            mesh_name = f"{base_name}_{i}" if len(data['shaders']) > 1 else base_name
            mesh = bpy.data.meshes.new(mesh_name)
            bm = bmesh.new()
            
            verts = []
            for pos in shader['positions']:
                v = bm.verts.new((pos[0], -pos[2], pos[1]))
                verts.append(v)
            
            bm.verts.ensure_lookup_table()
            
            for j in range(0, len(shader['indices']) - 2, 3):
                i0, i1, i2 = shader['indices'][j:j+3]
                if i0 < len(verts) and i1 < len(verts) and i2 < len(verts):
                    if i0 != i1 and i1 != i2 and i0 != i2:
                        try:
                            bm.faces.new((verts[i0], verts[i2], verts[i1]))
                        except ValueError:
                            pass
            
            bm.to_mesh(mesh)
            bm.free()
            mesh.update()
            
            obj = bpy.data.objects.new(mesh_name, mesh)
            obj['swg_format'] = 'MGN'
            obj['swg_version'] = data['version']
            obj['swg_skeleton'] = data['skeleton']
            
            bpy.context.collection.objects.link(obj)
            objects.append(obj)
            
            # Create vertex groups for bones
            for bone_name in data['bone_names']:
                obj.vertex_groups.new(name=bone_name)
        
        return objects
    
    def export_file(self, filepath, objects, **options):
        return ImportResult(False, "Export not yet implemented")


# =============================================================================
# ANS Handler - Animation (versions 0002-0004)
# =============================================================================

class AnsHandler:
    def __init__(self, context):
        self.context = context
    
    def import_file(self, filepath, **options):
        try:
            iff = Iff.from_file(filepath)
            
            if not iff.enter_form('KFAT', optional=True):
                return ImportResult(False, "Not a valid ANS file")
            
            version = str(iff.get_current_name())
            iff.enter_form(version)
            
            data = self._parse_animation(iff, version)
            
            iff.exit_form()
            iff.exit_form()
            
            action = self._create_action(data, filepath)
            
            return ImportResult(True, f"Imported animation v{version}: {data['frame_count']} frames", objects=[action])
            
        except Exception as e:
            traceback.print_exc()
            return ImportResult(False, f"Failed: {str(e)}")
    
    def _parse_animation(self, iff, version):
        data = {
            'version': version,
            'fps': 30.0,
            'frame_count': 0,
        }
        
        if iff.enter_chunk('INFO', optional=True):
            data['fps'] = iff.read_float()
            data['frame_count'] = iff.read_int32()
            iff.read_rest()
            iff.exit_chunk()
        
        # Skip remaining data for now
        while not iff.at_end_of_form():
            iff.skip_block()
        
        return data
    
    def _create_action(self, data, filepath):
        name = Path(filepath).stem
        action = bpy.data.actions.new(name)
        action['swg_format'] = 'ANS'
        action['swg_version'] = data['version']
        action['swg_fps'] = data['fps']
        
        bpy.context.scene.frame_start = 0
        bpy.context.scene.frame_end = data['frame_count']
        
        return action
    
    def export_file(self, filepath, objects, **options):
        return ImportResult(False, "Export not yet implemented")


# =============================================================================
# Preferences
# =============================================================================

class SWGAddonPreferences(AddonPreferences):
    bl_idname = __name__
    
    swg_data_path: StringProperty(
        name='SWG Data Path',
        description='Path to SWG client data',
        default='D:/titan/data/sku.0/sys.client/compiled/game/',
        subtype='DIR_PATH'
    )
    
    debug_mode: BoolProperty(
        name='Debug Mode',
        default=False
    )
    
    def draw(self, context):
        layout = self.layout
        layout.prop(self, 'swg_data_path')
        layout.prop(self, 'debug_mode')


# =============================================================================
# Operators
# =============================================================================

class SWG_OT_import_msh(Operator):
    """Import SWG Static Mesh"""
    bl_idname = 'import_mesh.swg_msh'
    bl_label = 'Import SWG Mesh (.msh)'
    bl_options = {'REGISTER', 'UNDO'}
    
    filepath: StringProperty(subtype='FILE_PATH')
    filter_glob: StringProperty(default='*.msh', options={'HIDDEN'})
    
    def execute(self, context):
        handler = MshHandler(context)
        result = handler.import_file(self.filepath)
        self.report({'INFO'} if result.success else {'ERROR'}, result.message)
        return {'FINISHED'} if result.success else {'CANCELLED'}
    
    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class SWG_OT_import_mgn(Operator):
    """Import SWG Mesh Generator"""
    bl_idname = 'import_mesh.swg_mgn'
    bl_label = 'Import SWG Mesh Generator (.mgn)'
    bl_options = {'REGISTER', 'UNDO'}
    
    filepath: StringProperty(subtype='FILE_PATH')
    filter_glob: StringProperty(default='*.mgn', options={'HIDDEN'})
    
    def execute(self, context):
        handler = MgnHandler(context)
        result = handler.import_file(self.filepath)
        self.report({'INFO'} if result.success else {'ERROR'}, result.message)
        return {'FINISHED'} if result.success else {'CANCELLED'}
    
    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class SWG_OT_import_skt(Operator):
    """Import SWG Skeleton"""
    bl_idname = 'import_armature.swg_skt'
    bl_label = 'Import SWG Skeleton (.skt)'
    bl_options = {'REGISTER', 'UNDO'}
    
    filepath: StringProperty(subtype='FILE_PATH')
    filter_glob: StringProperty(default='*.skt', options={'HIDDEN'})
    
    def execute(self, context):
        handler = SktHandler(context)
        result = handler.import_file(self.filepath)
        self.report({'INFO'} if result.success else {'ERROR'}, result.message)
        return {'FINISHED'} if result.success else {'CANCELLED'}
    
    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class SWG_OT_import_ans(Operator):
    """Import SWG Animation"""
    bl_idname = 'import_anim.swg_ans'
    bl_label = 'Import SWG Animation (.ans)'
    bl_options = {'REGISTER', 'UNDO'}
    
    filepath: StringProperty(subtype='FILE_PATH')
    filter_glob: StringProperty(default='*.ans', options={'HIDDEN'})
    
    def execute(self, context):
        handler = AnsHandler(context)
        result = handler.import_file(self.filepath)
        self.report({'INFO'} if result.success else {'ERROR'}, result.message)
        return {'FINISHED'} if result.success else {'CANCELLED'}
    
    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class SWG_OT_inspect_iff(Operator):
    """Inspect IFF file structure"""
    bl_idname = 'swg.inspect_iff'
    bl_label = 'Inspect IFF'
    
    filepath: StringProperty(subtype='FILE_PATH')
    filter_glob: StringProperty(default='*.*', options={'HIDDEN'})
    
    def execute(self, context):
        try:
            iff = Iff.from_file(self.filepath)
            structure = iff.dump_structure()
            print(f"\n=== {Path(self.filepath).name} ===")
            print(structure)
            print("=== End ===\n")
            self.report({'INFO'}, f'Inspected - see console')
            return {'FINISHED'}
        except Exception as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
    
    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


# =============================================================================
# Panel
# =============================================================================

class SWG_PT_main_panel(Panel):
    bl_label = 'SWG Tools'
    bl_idname = 'SWG_PT_main_panel'
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'SWG'
    
    def draw(self, context):
        layout = self.layout
        
        box = layout.box()
        box.label(text='Import', icon='IMPORT')
        col = box.column(align=True)
        col.operator('import_mesh.swg_msh', text='Mesh (.msh)')
        col.operator('import_mesh.swg_mgn', text='Mesh Gen (.mgn)')
        col.operator('import_armature.swg_skt', text='Skeleton (.skt)')
        col.operator('import_anim.swg_ans', text='Animation (.ans)')
        
        box = layout.box()
        box.label(text='Tools', icon='TOOL_SETTINGS')
        box.operator('swg.inspect_iff', text='Inspect IFF')


# =============================================================================
# Menu
# =============================================================================

def menu_func_import(self, context):
    self.layout.separator()
    self.layout.operator('import_mesh.swg_msh', text='SWG Mesh (.msh)')
    self.layout.operator('import_mesh.swg_mgn', text='SWG Mesh Generator (.mgn)')
    self.layout.operator('import_armature.swg_skt', text='SWG Skeleton (.skt)')
    self.layout.operator('import_anim.swg_ans', text='SWG Animation (.ans)')


# =============================================================================
# Registration
# =============================================================================

classes = (
    SWGAddonPreferences,
    SWG_OT_import_msh,
    SWG_OT_import_mgn,
    SWG_OT_import_skt,
    SWG_OT_import_ans,
    SWG_OT_inspect_iff,
    SWG_PT_main_panel,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)
    print('SWG Asset Toolchain registered')


def unregister():
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == '__main__':
    register()
