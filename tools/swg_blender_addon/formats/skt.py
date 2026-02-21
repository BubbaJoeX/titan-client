# SWG Asset Toolchain - SKT (Skeleton) Format Handler
# Copyright (c) Titan Project
#
# Handles .skt (BasicSkeletonTemplate) files.
# Reference: D:/titan/client/src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/BasicSkeletonTemplate.cpp

from __future__ import annotations
from typing import List, Dict, Optional, Tuple, Any, TYPE_CHECKING
from dataclasses import dataclass, field
from pathlib import Path
import math

from ..core.iff import Iff, IffError
from ..core.tag import Tag
from ..core.types import Vector3, Quaternion, JointRotationOrder
from ..core.registry import FormatHandler, ImportResult, ExportResult, register_handler

if TYPE_CHECKING:
    import bpy

__all__ = ['SktHandler', 'SkeletonData', 'JointData']


# =============================================================================
# Tags
# =============================================================================

TAG_SKTM = Tag('SKTM')
TAG_INFO = Tag('INFO')
TAG_NAME = Tag('NAME')
TAG_PRNT = Tag('PRNT')
TAG_RPRE = Tag('RPRE')
TAG_RPST = Tag('RPST')
TAG_BPTR = Tag('BPTR')
TAG_BPRO = Tag('BPRO')
TAG_BPMJ = Tag('BPMJ')
TAG_JROR = Tag('JROR')

TAG_0001 = Tag('0001')
TAG_0002 = Tag('0002')


# =============================================================================
# Data Structures
# =============================================================================

@dataclass
class JointData:
    """Data for a single skeleton joint/bone."""
    name: str = ""
    parent_index: int = -1
    pre_multiply_rotation: Quaternion = field(default_factory=Quaternion)
    post_multiply_rotation: Quaternion = field(default_factory=Quaternion)
    bind_pose_translation: Vector3 = field(default_factory=Vector3)
    bind_pose_rotation: Quaternion = field(default_factory=Quaternion)
    bind_pose_model_to_joint: Optional[Tuple[Tuple[float, ...], ...]] = None
    rotation_order: JointRotationOrder = JointRotationOrder.XYZ


@dataclass
class SkeletonData:
    """Complete skeleton data structure."""
    version: str = "0002"
    joints: List[JointData] = field(default_factory=list)
    unknown_chunks: Dict[str, bytes] = field(default_factory=dict)
    
    def get_joint_by_name(self, name: str) -> Optional[JointData]:
        """Find joint by name."""
        for joint in self.joints:
            if joint.name == name:
                return joint
        return None
    
    def get_joint_index(self, name: str) -> int:
        """Get index of joint by name."""
        for i, joint in enumerate(self.joints):
            if joint.name == name:
                return i
        return -1


# =============================================================================
# SKT Parser
# =============================================================================

class SktParser:
    """Parser for SKT (BasicSkeletonTemplate) files."""
    
    def __init__(self):
        self.warnings: List[str] = []
        self.unknown_chunks: Dict[str, bytes] = {}
    
    def parse(self, iff: Iff) -> SkeletonData:
        """Parse a skeleton file."""
        skeleton_data = SkeletonData()
        
        iff.enter_form(TAG_SKTM)
        
        version_tag = iff.get_current_name()
        skeleton_data.version = str(version_tag)
        
        if version_tag == TAG_0002:
            self._parse_version_0002(iff, skeleton_data)
        elif version_tag == TAG_0001:
            self._parse_version_0001(iff, skeleton_data)
        else:
            self.warnings.append(f"Unknown skeleton version: {version_tag}")
            self._parse_version_0002(iff, skeleton_data)
        
        iff.exit_form()
        
        skeleton_data.unknown_chunks = self.unknown_chunks
        return skeleton_data
    
    def _parse_version_0001(self, iff: Iff, skeleton_data: SkeletonData) -> None:
        """Parse version 0001 skeleton format."""
        iff.enter_form(TAG_0001)
        
        # INFO chunk - joint count
        joint_count = 0
        iff.enter_chunk(TAG_INFO)
        joint_count = iff.read_int32()
        iff.exit_chunk()
        
        # Initialize joints
        skeleton_data.joints = [JointData() for _ in range(joint_count)]
        
        # NAME chunk - joint names
        iff.enter_chunk(TAG_NAME)
        for i in range(joint_count):
            skeleton_data.joints[i].name = iff.read_string()
        iff.exit_chunk()
        
        # PRNT chunk - parent indices
        iff.enter_chunk(TAG_PRNT)
        for i in range(joint_count):
            skeleton_data.joints[i].parent_index = iff.read_int32()
        iff.exit_chunk()
        
        # RPRE chunk - pre-multiply rotations
        iff.enter_chunk(TAG_RPRE)
        for i in range(joint_count):
            w, x, y, z = iff.read_quaternion()
            skeleton_data.joints[i].pre_multiply_rotation = Quaternion(w, x, y, z)
        iff.exit_chunk()
        
        # RPST chunk - post-multiply rotations
        iff.enter_chunk(TAG_RPST)
        for i in range(joint_count):
            w, x, y, z = iff.read_quaternion()
            skeleton_data.joints[i].post_multiply_rotation = Quaternion(w, x, y, z)
        iff.exit_chunk()
        
        # BPTR chunk - bind pose translations
        iff.enter_chunk(TAG_BPTR)
        for i in range(joint_count):
            x, y, z = iff.read_vector()
            skeleton_data.joints[i].bind_pose_translation = Vector3(x, y, z)
        iff.exit_chunk()
        
        # BPRO chunk - bind pose rotations
        iff.enter_chunk(TAG_BPRO)
        for i in range(joint_count):
            w, x, y, z = iff.read_quaternion()
            skeleton_data.joints[i].bind_pose_rotation = Quaternion(w, x, y, z)
        iff.exit_chunk()
        
        # BPMJ chunk - bind pose model-to-joint transforms (version 0001 only)
        iff.enter_chunk(TAG_BPMJ)
        for i in range(joint_count):
            transform = iff.read_transform()
            skeleton_data.joints[i].bind_pose_model_to_joint = transform
        iff.exit_chunk()
        
        # JROR chunk - joint rotation orders
        iff.enter_chunk(TAG_JROR)
        for i in range(joint_count):
            order = iff.read_uint32()
            skeleton_data.joints[i].rotation_order = JointRotationOrder(order)
        iff.exit_chunk()
        
        iff.exit_form()
    
    def _parse_version_0002(self, iff: Iff, skeleton_data: SkeletonData) -> None:
        """Parse version 0002 skeleton format (no BPMJ chunk)."""
        iff.enter_form(TAG_0002)
        
        # INFO chunk
        joint_count = 0
        iff.enter_chunk(TAG_INFO)
        joint_count = iff.read_int32()
        iff.exit_chunk()
        
        skeleton_data.joints = [JointData() for _ in range(joint_count)]
        
        # NAME chunk
        iff.enter_chunk(TAG_NAME)
        for i in range(joint_count):
            skeleton_data.joints[i].name = iff.read_string()
        iff.exit_chunk()
        
        # PRNT chunk
        iff.enter_chunk(TAG_PRNT)
        for i in range(joint_count):
            skeleton_data.joints[i].parent_index = iff.read_int32()
        iff.exit_chunk()
        
        # RPRE chunk
        iff.enter_chunk(TAG_RPRE)
        for i in range(joint_count):
            w, x, y, z = iff.read_quaternion()
            skeleton_data.joints[i].pre_multiply_rotation = Quaternion(w, x, y, z)
        iff.exit_chunk()
        
        # RPST chunk
        iff.enter_chunk(TAG_RPST)
        for i in range(joint_count):
            w, x, y, z = iff.read_quaternion()
            skeleton_data.joints[i].post_multiply_rotation = Quaternion(w, x, y, z)
        iff.exit_chunk()
        
        # BPTR chunk
        iff.enter_chunk(TAG_BPTR)
        for i in range(joint_count):
            x, y, z = iff.read_vector()
            skeleton_data.joints[i].bind_pose_translation = Vector3(x, y, z)
        iff.exit_chunk()
        
        # BPRO chunk
        iff.enter_chunk(TAG_BPRO)
        for i in range(joint_count):
            w, x, y, z = iff.read_quaternion()
            skeleton_data.joints[i].bind_pose_rotation = Quaternion(w, x, y, z)
        iff.exit_chunk()
        
        # JROR chunk
        iff.enter_chunk(TAG_JROR)
        for i in range(joint_count):
            order = iff.read_uint32()
            skeleton_data.joints[i].rotation_order = JointRotationOrder(order)
        iff.exit_chunk()
        
        iff.exit_form()


# =============================================================================
# SKT Writer
# =============================================================================

class SktWriter:
    """Writer for SKT skeleton files."""
    
    def write(self, skeleton_data: SkeletonData) -> Iff:
        """Write skeleton data to IFF."""
        iff = Iff(growable=True)
        
        iff.insert_form(TAG_SKTM)
        
        version_tag = Tag(skeleton_data.version) if skeleton_data.version else TAG_0002
        iff.insert_form(version_tag)
        
        joint_count = len(skeleton_data.joints)
        
        # INFO chunk
        iff.insert_chunk(TAG_INFO)
        iff.insert_int32(joint_count)
        iff.exit_chunk()
        
        # NAME chunk
        iff.insert_chunk(TAG_NAME)
        for joint in skeleton_data.joints:
            iff.insert_string(joint.name)
        iff.exit_chunk()
        
        # PRNT chunk
        iff.insert_chunk(TAG_PRNT)
        for joint in skeleton_data.joints:
            iff.insert_int32(joint.parent_index)
        iff.exit_chunk()
        
        # RPRE chunk
        iff.insert_chunk(TAG_RPRE)
        for joint in skeleton_data.joints:
            q = joint.pre_multiply_rotation
            iff.insert_quaternion(q.w, q.x, q.y, q.z)
        iff.exit_chunk()
        
        # RPST chunk
        iff.insert_chunk(TAG_RPST)
        for joint in skeleton_data.joints:
            q = joint.post_multiply_rotation
            iff.insert_quaternion(q.w, q.x, q.y, q.z)
        iff.exit_chunk()
        
        # BPTR chunk
        iff.insert_chunk(TAG_BPTR)
        for joint in skeleton_data.joints:
            t = joint.bind_pose_translation
            iff.insert_vector(t.x, t.y, t.z)
        iff.exit_chunk()
        
        # BPRO chunk
        iff.insert_chunk(TAG_BPRO)
        for joint in skeleton_data.joints:
            q = joint.bind_pose_rotation
            iff.insert_quaternion(q.w, q.x, q.y, q.z)
        iff.exit_chunk()
        
        # BPMJ chunk (version 0001 only)
        if version_tag == TAG_0001:
            iff.insert_chunk(TAG_BPMJ)
            for joint in skeleton_data.joints:
                if joint.bind_pose_model_to_joint:
                    iff.insert_transform(joint.bind_pose_model_to_joint)
                else:
                    # Write identity transform
                    iff.insert_transform(((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0)))
            iff.exit_chunk()
        
        # JROR chunk
        iff.insert_chunk(TAG_JROR)
        for joint in skeleton_data.joints:
            iff.insert_uint32(int(joint.rotation_order))
        iff.exit_chunk()
        
        iff.exit_form()  # Version
        iff.exit_form()  # SKTM
        
        return iff


# =============================================================================
# Blender Integration
# =============================================================================

def quaternion_to_euler(q: Quaternion) -> Tuple[float, float, float]:
    """Convert quaternion to Euler angles (XYZ order)."""
    # Roll (x-axis rotation)
    sinr_cosp = 2 * (q.w * q.x + q.y * q.z)
    cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y)
    roll = math.atan2(sinr_cosp, cosr_cosp)
    
    # Pitch (y-axis rotation)
    sinp = 2 * (q.w * q.y - q.z * q.x)
    if abs(sinp) >= 1:
        pitch = math.copysign(math.pi / 2, sinp)
    else:
        pitch = math.asin(sinp)
    
    # Yaw (z-axis rotation)
    siny_cosp = 2 * (q.w * q.z + q.x * q.y)
    cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z)
    yaw = math.atan2(siny_cosp, cosy_cosp)
    
    return (roll, pitch, yaw)


@register_handler
class SktHandler(FormatHandler):
    """Handler for SKT (skeleton) files."""
    
    FORMAT_NAME = "SKT"
    FORMAT_DESCRIPTION = "SWG Skeleton (BasicSkeletonTemplate)"
    FILE_EXTENSIONS = ["skt"]
    ROOT_TAGS = [TAG_SKTM]
    CAN_IMPORT = True
    CAN_EXPORT = True
    IS_FULLY_UNDERSTOOD = True
    
    def import_file(self, filepath: str, **options) -> ImportResult:
        """Import SKT file to Blender armature."""
        try:
            iff = Iff.from_file(filepath)
            parser = SktParser()
            skeleton_data = parser.parse(iff)
            
            self.warnings.extend(parser.warnings)
            self.unknown_chunks = parser.unknown_chunks
            
            objects = self._create_blender_armature(skeleton_data, filepath, **options)
            
            return ImportResult(
                success=True,
                message=f"Imported skeleton with {len(skeleton_data.joints)} joints",
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
        """Export Blender armature to SKT file."""
        try:
            skeleton_data = self._convert_from_blender(objects, **options)
            
            writer = SktWriter()
            iff = writer.write(skeleton_data)
            iff.write(filepath)
            
            return ExportResult(
                success=True,
                message=f"Exported skeleton with {len(skeleton_data.joints)} joints",
                warnings=self.warnings,
                bytes_written=iff.raw_data_size,
            )
            
        except Exception as e:
            return ExportResult(
                success=False,
                message=f"Failed to export: {str(e)}",
                warnings=self.warnings,
            )
    
    def _create_blender_armature(self, skeleton_data: SkeletonData, filepath: str,
                                  **options) -> List[Any]:
        """Create Blender armature from skeleton data."""
        import bpy
        import mathutils
        
        name = Path(filepath).stem
        
        # Create armature
        armature = bpy.data.armatures.new(name)
        armature_obj = bpy.data.objects.new(name, armature)
        
        bpy.context.collection.objects.link(armature_obj)
        bpy.context.view_layer.objects.active = armature_obj
        
        # Set display options
        bone_display = options.get('bone_display_type', 'OCTAHEDRAL')
        armature.display_type = bone_display
        
        # Enter edit mode to create bones
        bpy.ops.object.mode_set(mode='EDIT')
        
        edit_bones = armature.edit_bones
        bone_map: Dict[int, Any] = {}
        
        # First pass: create all bones
        for i, joint in enumerate(skeleton_data.joints):
            bone = edit_bones.new(joint.name)
            bone_map[i] = bone
            
            # Calculate world position from bind pose
            # Convert SWG Y-up to Blender Z-up
            pos = joint.bind_pose_translation
            bone.head = (pos.x, -pos.z, pos.y)
            
            # Initial tail position (will be adjusted)
            bone.tail = (pos.x, -pos.z, pos.y + 0.1)
        
        # Second pass: set up parenting
        for i, joint in enumerate(skeleton_data.joints):
            if joint.parent_index >= 0 and joint.parent_index < len(skeleton_data.joints):
                parent_bone = bone_map.get(joint.parent_index)
                if parent_bone:
                    bone_map[i].parent = parent_bone
                    
                    # Connect if close enough
                    if (mathutils.Vector(bone_map[i].head) - 
                        mathutils.Vector(parent_bone.tail)).length < 0.001:
                        bone_map[i].use_connect = True
        
        # Third pass: adjust tail positions
        for i, joint in enumerate(skeleton_data.joints):
            bone = bone_map[i]
            
            # Find children
            children = [j for j, jd in enumerate(skeleton_data.joints) 
                       if jd.parent_index == i]
            
            if children:
                # Point toward first child
                child_bone = bone_map[children[0]]
                bone.tail = child_bone.head
            else:
                # No children - extend in local direction
                parent_idx = joint.parent_index
                if parent_idx >= 0:
                    parent_bone = bone_map[parent_idx]
                    direction = mathutils.Vector(bone.head) - mathutils.Vector(parent_bone.head)
                    if direction.length > 0.001:
                        direction.normalize()
                        bone.tail = mathutils.Vector(bone.head) + direction * 0.1
                    else:
                        bone.tail = mathutils.Vector(bone.head) + mathutils.Vector((0, 0, 0.1))
                else:
                    bone.tail = mathutils.Vector(bone.head) + mathutils.Vector((0, 0, 0.1))
        
        bpy.ops.object.mode_set(mode='OBJECT')
        
        # Store custom properties
        armature_obj['swg_format'] = 'SKT'
        armature_obj['swg_version'] = skeleton_data.version
        
        # Store rotation orders and other data in bone properties
        for i, joint in enumerate(skeleton_data.joints):
            bone = armature.bones.get(joint.name)
            if bone:
                bone['swg_rotation_order'] = int(joint.rotation_order)
                bone['swg_pre_rot'] = joint.pre_multiply_rotation.to_tuple()
                bone['swg_post_rot'] = joint.post_multiply_rotation.to_tuple()
        
        return [armature_obj]
    
    def _convert_from_blender(self, objects: List[Any], **options) -> SkeletonData:
        """Convert Blender armature to skeleton data."""
        import bpy
        import mathutils
        
        skeleton_data = SkeletonData()
        skeleton_data.version = options.get('export_version', '0002')
        
        # Find armature
        armature_obj = None
        for obj in objects:
            if obj.type == 'ARMATURE':
                armature_obj = obj
                break
        
        if not armature_obj:
            raise ValueError("No armature found in selection")
        
        armature = armature_obj.data
        
        # Build joint list maintaining bone order
        bone_names = [bone.name for bone in armature.bones]
        name_to_index = {name: i for i, name in enumerate(bone_names)}
        
        for i, bone in enumerate(armature.bones):
            joint = JointData()
            joint.name = bone.name
            
            # Get parent index
            if bone.parent:
                joint.parent_index = name_to_index.get(bone.parent.name, -1)
            else:
                joint.parent_index = -1
            
            # Get bind pose translation (convert Blender Z-up to SWG Y-up)
            head = bone.head_local
            joint.bind_pose_translation = Vector3(head.x, head.z, -head.y)
            
            # Get rotation from bone matrix
            matrix = bone.matrix_local
            quat = matrix.to_quaternion()
            
            # Convert quaternion (Blender Z-up to SWG Y-up)
            joint.bind_pose_rotation = Quaternion(quat.w, quat.x, quat.z, -quat.y)
            
            # Get custom properties if available
            if 'swg_rotation_order' in bone:
                joint.rotation_order = JointRotationOrder(bone['swg_rotation_order'])
            
            if 'swg_pre_rot' in bone:
                pre = bone['swg_pre_rot']
                joint.pre_multiply_rotation = Quaternion(pre[0], pre[1], pre[2], pre[3])
            
            if 'swg_post_rot' in bone:
                post = bone['swg_post_rot']
                joint.post_multiply_rotation = Quaternion(post[0], post[1], post[2], post[3])
            
            skeleton_data.joints.append(joint)
        
        return skeleton_data
