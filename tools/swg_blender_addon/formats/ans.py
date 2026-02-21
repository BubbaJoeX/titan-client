# SWG Asset Toolchain - ANS (Animation) Format Handler
# Copyright (c) Titan Project
#
# Handles .ans (KeyframeSkeletalAnimationTemplate) files.
# Reference: D:/titan/client/src/engine/client/library/clientSkeletalAnimation/src/shared/animation/KeyframeSkeletalAnimationTemplate.cpp

from __future__ import annotations
from typing import List, Dict, Optional, Tuple, Any, TYPE_CHECKING
from dataclasses import dataclass, field
from pathlib import Path

from ..core.iff import Iff, IffError
from ..core.tag import Tag
from ..core.types import Vector3, Quaternion
from ..core.registry import FormatHandler, ImportResult, ExportResult, register_handler

if TYPE_CHECKING:
    import bpy

__all__ = ['AnsHandler', 'AnimationData', 'TransformChannelData']


# =============================================================================
# Tags
# =============================================================================

TAG_KFAT = Tag('KFAT')  # Keyframe Animation Template
TAG_CKAT = Tag('CKAT')  # Compressed Keyframe Animation Template
TAG_INFO = Tag('INFO')
TAG_XFIN = Tag('XFIN')  # Transform Info
TAG_AROT = Tag('AROT')  # Animated Rotation
TAG_SROT = Tag('SROT')  # Static Rotation
TAG_ATRN = Tag('ATRN')  # Animated Translation
TAG_STRN = Tag('STRN')  # Static Translation
TAG_QCHN = Tag('QCHN')  # Quaternion Channel
TAG_CHNL = Tag('CHNL')  # Channel (translation)
TAG_MSGS = Tag('MSGS')  # Messages
TAG_MESG = Tag('MESG')  # Message
TAG_LOCR = Tag('LOCR')  # Locomotion Rotation
TAG_LOCT = Tag('LOCT')  # Locomotion Translation

TAG_0002 = Tag('0002')
TAG_0003 = Tag('0003')
TAG_0004 = Tag('0004')


# =============================================================================
# Data Structures
# =============================================================================

@dataclass
class RotationKeyframe:
    """Rotation keyframe data."""
    frame: float = 0.0
    rotation: Quaternion = field(default_factory=Quaternion)


@dataclass
class TranslationKeyframe:
    """Translation keyframe data."""
    frame: float = 0.0
    value: float = 0.0


@dataclass
class RotationChannel:
    """Quaternion rotation channel."""
    keyframes: List[RotationKeyframe] = field(default_factory=list)


@dataclass 
class TranslationChannel:
    """Single-axis translation channel."""
    keyframes: List[TranslationKeyframe] = field(default_factory=list)


@dataclass
class TransformChannelData:
    """Animation channels for a single transform/bone."""
    name: str = ""
    has_animated_rotation: bool = False
    rotation_channel_index: int = -1
    translation_mask: int = 0  # Bit flags for X, Y, Z
    x_translation_channel_index: int = -1
    y_translation_channel_index: int = -1
    z_translation_channel_index: int = -1
    
    # Static values (used when not animated)
    static_rotation: Optional[Quaternion] = None
    static_translation: Optional[Vector3] = None


@dataclass
class AnimationMessage:
    """Animation event message."""
    name: str = ""
    frame_numbers: List[int] = field(default_factory=list)


@dataclass
class LocomotionData:
    """Locomotion (root motion) data."""
    rotation_keyframes: List[RotationKeyframe] = field(default_factory=list)
    translation_keyframes: List[Tuple[float, Vector3]] = field(default_factory=list)


@dataclass
class AnimationData:
    """Complete animation data."""
    version: str = "0003"
    fps: float = 30.0
    frame_count: int = 0
    transform_count: int = 0
    rotation_channel_count: int = 0
    translation_channel_count: int = 0
    
    transform_channels: List[TransformChannelData] = field(default_factory=list)
    rotation_channels: List[RotationChannel] = field(default_factory=list)
    translation_channels: List[TranslationChannel] = field(default_factory=list)
    messages: List[AnimationMessage] = field(default_factory=list)
    locomotion: Optional[LocomotionData] = None
    
    unknown_chunks: Dict[str, bytes] = field(default_factory=dict)
    
    @property
    def duration(self) -> float:
        """Get animation duration in seconds."""
        if self.fps > 0:
            return self.frame_count / self.fps
        return 0.0


# =============================================================================
# ANS Parser
# =============================================================================

class AnsParser:
    """Parser for ANS (KeyframeSkeletalAnimationTemplate) files."""
    
    def __init__(self):
        self.warnings: List[str] = []
        self.unknown_chunks: Dict[str, bytes] = {}
    
    def parse(self, iff: Iff) -> AnimationData:
        """Parse an animation file."""
        data = AnimationData()
        
        # Check for KFAT or CKAT root
        root_tag = iff.get_current_name()
        if iff.is_current_form():
            iff.enter_form()
            root_tag = iff.get_current_name()
            iff.go_to_top_of_form()
            iff.exit_form()
        
        iff.enter_form(TAG_KFAT)
        
        version_tag = iff.get_current_name()
        data.version = str(version_tag)
        
        if version_tag == TAG_0004:
            self._parse_version_0004(iff, data)
        elif version_tag == TAG_0003:
            self._parse_version_0003(iff, data)
        elif version_tag == TAG_0002:
            self._parse_version_0002(iff, data)
        else:
            self.warnings.append(f"Unknown animation version: {version_tag}")
            self._parse_version_0003(iff, data)
        
        iff.exit_form()
        
        data.unknown_chunks = self.unknown_chunks
        return data
    
    def _parse_version_0002(self, iff: Iff, data: AnimationData) -> None:
        """Parse version 0002."""
        iff.enter_form(TAG_0002)
        self._parse_common(iff, data, version=2)
        iff.exit_form()
    
    def _parse_version_0003(self, iff: Iff, data: AnimationData) -> None:
        """Parse version 0003."""
        iff.enter_form(TAG_0003)
        self._parse_common(iff, data, version=3)
        iff.exit_form()
    
    def _parse_version_0004(self, iff: Iff, data: AnimationData) -> None:
        """Parse version 0004."""
        iff.enter_form(TAG_0004)
        self._parse_common(iff, data, version=4)
        iff.exit_form()
    
    def _parse_common(self, iff: Iff, data: AnimationData, version: int) -> None:
        """Parse common animation data."""
        # INFO chunk
        if iff.enter_chunk(TAG_INFO, optional=True):
            data.fps = iff.read_float()
            data.frame_count = iff.read_int32()
            data.transform_count = iff.read_int32()
            data.rotation_channel_count = iff.read_int32()
            data.translation_channel_count = iff.read_int32()
            iff.exit_chunk()
        
        # Transform info
        for _ in range(data.transform_count):
            if iff.at_end_of_form():
                break
            
            transform = self._parse_transform_info(iff)
            data.transform_channels.append(transform)
        
        # Rotation channels
        for _ in range(data.rotation_channel_count):
            if iff.at_end_of_form():
                break
            
            channel = self._parse_rotation_channel(iff, version)
            data.rotation_channels.append(channel)
        
        # Translation channels
        for _ in range(data.translation_channel_count):
            if iff.at_end_of_form():
                break
            
            channel = self._parse_translation_channel(iff, version)
            data.translation_channels.append(channel)
        
        # Messages (optional)
        if iff.enter_form('MSGS', optional=True):
            while not iff.at_end_of_form():
                msg = self._parse_message(iff, version)
                data.messages.append(msg)
            iff.exit_form()
        
        # Locomotion (optional)
        if not iff.at_end_of_form():
            data.locomotion = self._parse_locomotion(iff)
    
    def _parse_transform_info(self, iff: Iff) -> TransformChannelData:
        """Parse transform channel info."""
        transform = TransformChannelData()
        
        if not iff.enter_form('XFIN', optional=True):
            return transform
        
        # Version form
        if iff.is_current_form():
            iff.enter_form()
            
            # INFO chunk
            if iff.enter_chunk(TAG_INFO, optional=True):
                transform.name = iff.read_string()
                transform.has_animated_rotation = iff.read_bool8()
                transform.rotation_channel_index = iff.read_int32()
                transform.translation_mask = iff.read_uint32()
                transform.x_translation_channel_index = iff.read_int32()
                transform.y_translation_channel_index = iff.read_int32()
                transform.z_translation_channel_index = iff.read_int32()
                iff.exit_chunk()
            
            # Static rotation (SROT)
            if iff.enter_chunk('SROT', optional=True):
                w, x, y, z = iff.read_quaternion()
                transform.static_rotation = Quaternion(w, x, y, z)
                iff.exit_chunk()
            
            # Static translation (STRN)
            if iff.enter_chunk('STRN', optional=True):
                x, y, z = iff.read_vector()
                transform.static_translation = Vector3(x, y, z)
                iff.exit_chunk()
            
            iff.exit_form()
        
        iff.exit_form()
        return transform
    
    def _parse_rotation_channel(self, iff: Iff, version: int) -> RotationChannel:
        """Parse a rotation channel."""
        channel = RotationChannel()
        
        if not iff.enter_chunk('QCHN', optional=True):
            return channel
        
        key_count = iff.read_int32()
        
        for _ in range(key_count):
            if iff.get_chunk_length_left() < 20:  # 4 + 16 bytes minimum
                break
            
            frame = iff.read_float()
            w, x, y, z = iff.read_quaternion()
            
            channel.keyframes.append(RotationKeyframe(
                frame=frame,
                rotation=Quaternion(w, x, y, z)
            ))
        
        iff.exit_chunk()
        return channel
    
    def _parse_translation_channel(self, iff: Iff, version: int) -> TranslationChannel:
        """Parse a translation channel."""
        channel = TranslationChannel()
        
        if not iff.enter_chunk('CHNL', optional=True):
            return channel
        
        key_count = iff.read_int32()
        
        for _ in range(key_count):
            if iff.get_chunk_length_left() < 8:  # 4 + 4 bytes minimum
                break
            
            frame = iff.read_float()
            value = iff.read_float()
            
            channel.keyframes.append(TranslationKeyframe(
                frame=frame,
                value=value
            ))
        
        iff.exit_chunk()
        return channel
    
    def _parse_message(self, iff: Iff, version: int) -> AnimationMessage:
        """Parse an animation message."""
        msg = AnimationMessage()
        
        if not iff.enter_chunk('MESG', optional=True):
            return msg
        
        signal_count = iff.read_int16()
        msg.name = iff.read_string()
        
        for _ in range(signal_count):
            if iff.get_chunk_length_left() >= 4:
                msg.frame_numbers.append(iff.read_int32())
        
        iff.exit_chunk()
        return msg
    
    def _parse_locomotion(self, iff: Iff) -> Optional[LocomotionData]:
        """Parse locomotion data."""
        locomotion = LocomotionData()
        
        # Locomotion rotation
        if iff.enter_chunk('LOCR', optional=True):
            key_count = iff.read_int32()
            for _ in range(key_count):
                if iff.get_chunk_length_left() < 20:
                    break
                frame = iff.read_float()
                w, x, y, z = iff.read_quaternion()
                locomotion.rotation_keyframes.append(RotationKeyframe(
                    frame=frame,
                    rotation=Quaternion(w, x, y, z)
                ))
            iff.exit_chunk()
        
        # Locomotion translation
        if iff.enter_chunk('LOCT', optional=True):
            key_count = iff.read_int32()
            for _ in range(key_count):
                if iff.get_chunk_length_left() < 16:
                    break
                frame = iff.read_float()
                x, y, z = iff.read_vector()
                locomotion.translation_keyframes.append((frame, Vector3(x, y, z)))
            iff.exit_chunk()
        
        if locomotion.rotation_keyframes or locomotion.translation_keyframes:
            return locomotion
        return None


# =============================================================================
# ANS Writer
# =============================================================================

class AnsWriter:
    """Writer for ANS animation files."""
    
    def write(self, data: AnimationData) -> Iff:
        """Write animation data to IFF."""
        iff = Iff(growable=True)
        
        iff.insert_form(TAG_KFAT)
        
        version_tag = Tag(data.version) if data.version else TAG_0003
        iff.insert_form(version_tag)
        
        # INFO chunk
        iff.insert_chunk(TAG_INFO)
        iff.insert_float(data.fps)
        iff.insert_int32(data.frame_count)
        iff.insert_int32(len(data.transform_channels))
        iff.insert_int32(len(data.rotation_channels))
        iff.insert_int32(len(data.translation_channels))
        iff.exit_chunk()
        
        # Transform info
        for transform in data.transform_channels:
            self._write_transform_info(iff, transform)
        
        # Rotation channels
        for channel in data.rotation_channels:
            self._write_rotation_channel(iff, channel)
        
        # Translation channels
        for channel in data.translation_channels:
            self._write_translation_channel(iff, channel)
        
        # Messages
        if data.messages:
            iff.insert_form(Tag('MSGS'))
            for msg in data.messages:
                self._write_message(iff, msg)
            iff.exit_form()
        
        # Locomotion
        if data.locomotion:
            self._write_locomotion(iff, data.locomotion)
        
        iff.exit_form()  # Version
        iff.exit_form()  # KFAT
        
        return iff
    
    def _write_transform_info(self, iff: Iff, transform: TransformChannelData) -> None:
        """Write transform channel info."""
        iff.insert_form(Tag('XFIN'))
        iff.insert_form(TAG_0003)
        
        iff.insert_chunk(TAG_INFO)
        iff.insert_string(transform.name)
        iff.insert_bool8(transform.has_animated_rotation)
        iff.insert_int32(transform.rotation_channel_index)
        iff.insert_uint32(transform.translation_mask)
        iff.insert_int32(transform.x_translation_channel_index)
        iff.insert_int32(transform.y_translation_channel_index)
        iff.insert_int32(transform.z_translation_channel_index)
        iff.exit_chunk()
        
        if transform.static_rotation:
            q = transform.static_rotation
            iff.insert_chunk(Tag('SROT'))
            iff.insert_quaternion(q.w, q.x, q.y, q.z)
            iff.exit_chunk()
        
        if transform.static_translation:
            t = transform.static_translation
            iff.insert_chunk(Tag('STRN'))
            iff.insert_vector(t.x, t.y, t.z)
            iff.exit_chunk()
        
        iff.exit_form()
        iff.exit_form()
    
    def _write_rotation_channel(self, iff: Iff, channel: RotationChannel) -> None:
        """Write rotation channel."""
        iff.insert_chunk(Tag('QCHN'))
        iff.insert_int32(len(channel.keyframes))
        
        for kf in channel.keyframes:
            iff.insert_float(kf.frame)
            iff.insert_quaternion(kf.rotation.w, kf.rotation.x, 
                                  kf.rotation.y, kf.rotation.z)
        
        iff.exit_chunk()
    
    def _write_translation_channel(self, iff: Iff, channel: TranslationChannel) -> None:
        """Write translation channel."""
        iff.insert_chunk(Tag('CHNL'))
        iff.insert_int32(len(channel.keyframes))
        
        for kf in channel.keyframes:
            iff.insert_float(kf.frame)
            iff.insert_float(kf.value)
        
        iff.exit_chunk()
    
    def _write_message(self, iff: Iff, msg: AnimationMessage) -> None:
        """Write animation message."""
        iff.insert_chunk(Tag('MESG'))
        iff.insert_int16(len(msg.frame_numbers))
        iff.insert_string(msg.name)
        
        for frame in msg.frame_numbers:
            iff.insert_int32(frame)
        
        iff.exit_chunk()
    
    def _write_locomotion(self, iff: Iff, locomotion: LocomotionData) -> None:
        """Write locomotion data."""
        if locomotion.rotation_keyframes:
            iff.insert_chunk(Tag('LOCR'))
            iff.insert_int32(len(locomotion.rotation_keyframes))
            for kf in locomotion.rotation_keyframes:
                iff.insert_float(kf.frame)
                iff.insert_quaternion(kf.rotation.w, kf.rotation.x,
                                      kf.rotation.y, kf.rotation.z)
            iff.exit_chunk()
        
        if locomotion.translation_keyframes:
            iff.insert_chunk(Tag('LOCT'))
            iff.insert_int32(len(locomotion.translation_keyframes))
            for frame, vec in locomotion.translation_keyframes:
                iff.insert_float(frame)
                iff.insert_vector(vec.x, vec.y, vec.z)
            iff.exit_chunk()


# =============================================================================
# Blender Integration
# =============================================================================

@register_handler
class AnsHandler(FormatHandler):
    """Handler for ANS (animation) files."""
    
    FORMAT_NAME = "ANS"
    FORMAT_DESCRIPTION = "SWG Animation (KeyframeSkeletalAnimationTemplate)"
    FILE_EXTENSIONS = ["ans"]
    ROOT_TAGS = [TAG_KFAT, TAG_CKAT]
    CAN_IMPORT = True
    CAN_EXPORT = True
    IS_FULLY_UNDERSTOOD = True
    
    def import_file(self, filepath: str, **options) -> ImportResult:
        """Import ANS file to Blender action."""
        try:
            iff = Iff.from_file(filepath)
            parser = AnsParser()
            data = parser.parse(iff)
            
            self.warnings.extend(parser.warnings)
            self.unknown_chunks = parser.unknown_chunks
            
            objects = self._create_blender_action(data, filepath, **options)
            
            return ImportResult(
                success=True,
                message=f"Imported animation: {data.frame_count} frames, "
                        f"{len(data.transform_channels)} transforms",
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
        """Export Blender action to ANS file."""
        try:
            data = self._convert_from_blender(objects, **options)
            
            writer = AnsWriter()
            iff = writer.write(data)
            iff.write(filepath)
            
            return ExportResult(
                success=True,
                message=f"Exported animation: {data.frame_count} frames",
                warnings=self.warnings,
                bytes_written=iff.raw_data_size,
            )
            
        except Exception as e:
            return ExportResult(
                success=False,
                message=f"Failed to export: {str(e)}",
                warnings=self.warnings,
            )
    
    def _create_blender_action(self, data: AnimationData, filepath: str,
                                **options) -> List[Any]:
        """Create Blender action from animation data."""
        import bpy
        
        name = Path(filepath).stem
        
        # Create action
        action = bpy.data.actions.new(name)
        action['swg_format'] = 'ANS'
        action['swg_version'] = data.version
        action['swg_fps'] = data.fps
        
        # Get or create target armature
        target_name = options.get('target_armature', '')
        armature_obj = None
        
        if target_name:
            armature_obj = bpy.data.objects.get(target_name)
        else:
            # Try to find an armature in selection or scene
            for obj in bpy.context.selected_objects:
                if obj.type == 'ARMATURE':
                    armature_obj = obj
                    break
            if not armature_obj:
                for obj in bpy.context.scene.objects:
                    if obj.type == 'ARMATURE':
                        armature_obj = obj
                        break
        
        # Create F-curves for each transform
        for transform in data.transform_channels:
            bone_name = transform.name
            
            # Rotation
            if transform.has_animated_rotation and transform.rotation_channel_index >= 0:
                if transform.rotation_channel_index < len(data.rotation_channels):
                    channel = data.rotation_channels[transform.rotation_channel_index]
                    
                    # Create quaternion F-curves
                    for qi, component in enumerate(['W', 'X', 'Y', 'Z']):
                        data_path = f'pose.bones["{bone_name}"].rotation_quaternion'
                        fcurve = action.fcurves.new(data_path=data_path, index=qi)
                        
                        for kf in channel.keyframes:
                            # Convert SWG to Blender quaternion
                            if qi == 0:
                                val = kf.rotation.w
                            elif qi == 1:
                                val = kf.rotation.x
                            elif qi == 2:
                                val = -kf.rotation.z  # Coordinate conversion
                            else:
                                val = kf.rotation.y
                            
                            fcurve.keyframe_points.insert(kf.frame, val)
            
            # Translation
            translation_mask = transform.translation_mask
            
            # X translation
            if translation_mask & 1 and transform.x_translation_channel_index >= 0:
                if transform.x_translation_channel_index < len(data.translation_channels):
                    channel = data.translation_channels[transform.x_translation_channel_index]
                    data_path = f'pose.bones["{bone_name}"].location'
                    fcurve = action.fcurves.new(data_path=data_path, index=0)
                    
                    for kf in channel.keyframes:
                        fcurve.keyframe_points.insert(kf.frame, kf.value)
            
            # Y translation (maps to Z in Blender)
            if translation_mask & 2 and transform.y_translation_channel_index >= 0:
                if transform.y_translation_channel_index < len(data.translation_channels):
                    channel = data.translation_channels[transform.y_translation_channel_index]
                    data_path = f'pose.bones["{bone_name}"].location'
                    fcurve = action.fcurves.new(data_path=data_path, index=2)
                    
                    for kf in channel.keyframes:
                        fcurve.keyframe_points.insert(kf.frame, kf.value)
            
            # Z translation (maps to -Y in Blender)
            if translation_mask & 4 and transform.z_translation_channel_index >= 0:
                if transform.z_translation_channel_index < len(data.translation_channels):
                    channel = data.translation_channels[transform.z_translation_channel_index]
                    data_path = f'pose.bones["{bone_name}"].location'
                    fcurve = action.fcurves.new(data_path=data_path, index=1)
                    
                    for kf in channel.keyframes:
                        fcurve.keyframe_points.insert(kf.frame, -kf.value)
        
        # Store messages as markers
        for msg in data.messages:
            for frame in msg.frame_numbers:
                marker = action.pose_markers.new(name=msg.name)
                marker.frame = frame
        
        # Assign to armature if found
        if armature_obj:
            if not armature_obj.animation_data:
                armature_obj.animation_data_create()
            armature_obj.animation_data.action = action
            
            # Set frame range
            bpy.context.scene.frame_start = 0
            bpy.context.scene.frame_end = data.frame_count
            bpy.context.scene.render.fps = int(data.fps)
        
        return [action]
    
    def _convert_from_blender(self, objects: List[Any], **options) -> AnimationData:
        """Convert Blender action to animation data."""
        import bpy
        
        data = AnimationData()
        data.fps = float(bpy.context.scene.render.fps)
        
        # Find action to export
        action = None
        for obj in objects:
            if obj.type == 'ARMATURE' and obj.animation_data and obj.animation_data.action:
                action = obj.animation_data.action
                break
        
        if not action:
            # Try active action
            if bpy.context.object and bpy.context.object.animation_data:
                action = bpy.context.object.animation_data.action
        
        if not action:
            raise ValueError("No animation action found")
        
        # Get frame range
        frame_start, frame_end = action.frame_range
        data.frame_count = int(frame_end - frame_start)
        
        # Group F-curves by bone
        bone_curves: Dict[str, Dict[str, List]] = {}
        
        for fcurve in action.fcurves:
            # Parse data path
            if 'pose.bones[' not in fcurve.data_path:
                continue
            
            # Extract bone name
            start = fcurve.data_path.find('["') + 2
            end = fcurve.data_path.find('"]')
            if start < 2 or end < 0:
                continue
            
            bone_name = fcurve.data_path[start:end]
            prop = fcurve.data_path.split('.')[-1]
            
            if bone_name not in bone_curves:
                bone_curves[bone_name] = {}
            
            key = f"{prop}_{fcurve.array_index}"
            bone_curves[bone_name][key] = fcurve
        
        # Convert to SWG format
        for bone_name, curves in bone_curves.items():
            transform = TransformChannelData()
            transform.name = bone_name
            
            # Check for rotation
            if 'rotation_quaternion_0' in curves:
                transform.has_animated_rotation = True
                transform.rotation_channel_index = len(data.rotation_channels)
                
                channel = RotationChannel()
                
                # Get keyframe times from W channel
                w_curve = curves.get('rotation_quaternion_0')
                if w_curve:
                    for kp in w_curve.keyframe_points:
                        frame = kp.co[0]
                        
                        # Get all components at this frame
                        w = w_curve.evaluate(frame)
                        x = curves.get('rotation_quaternion_1', w_curve).evaluate(frame)
                        y = curves.get('rotation_quaternion_2', w_curve).evaluate(frame)
                        z = curves.get('rotation_quaternion_3', w_curve).evaluate(frame)
                        
                        # Convert Blender to SWG quaternion
                        channel.keyframes.append(RotationKeyframe(
                            frame=frame,
                            rotation=Quaternion(w, x, z, -y)
                        ))
                
                data.rotation_channels.append(channel)
            
            # Check for translation
            translation_mask = 0
            
            if 'location_0' in curves:
                translation_mask |= 1
                transform.x_translation_channel_index = len(data.translation_channels)
                
                channel = TranslationChannel()
                for kp in curves['location_0'].keyframe_points:
                    channel.keyframes.append(TranslationKeyframe(
                        frame=kp.co[0],
                        value=kp.co[1]
                    ))
                data.translation_channels.append(channel)
            
            if 'location_2' in curves:  # Blender Z -> SWG Y
                translation_mask |= 2
                transform.y_translation_channel_index = len(data.translation_channels)
                
                channel = TranslationChannel()
                for kp in curves['location_2'].keyframe_points:
                    channel.keyframes.append(TranslationKeyframe(
                        frame=kp.co[0],
                        value=kp.co[1]
                    ))
                data.translation_channels.append(channel)
            
            if 'location_1' in curves:  # Blender Y -> SWG -Z
                translation_mask |= 4
                transform.z_translation_channel_index = len(data.translation_channels)
                
                channel = TranslationChannel()
                for kp in curves['location_1'].keyframe_points:
                    channel.keyframes.append(TranslationKeyframe(
                        frame=kp.co[0],
                        value=-kp.co[1]
                    ))
                data.translation_channels.append(channel)
            
            transform.translation_mask = translation_mask
            data.transform_channels.append(transform)
        
        # Convert pose markers to messages
        for marker in action.pose_markers:
            # Find or create message
            msg = None
            for m in data.messages:
                if m.name == marker.name:
                    msg = m
                    break
            
            if not msg:
                msg = AnimationMessage(name=marker.name)
                data.messages.append(msg)
            
            msg.frame_numbers.append(marker.frame)
        
        return data
