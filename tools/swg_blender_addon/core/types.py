# SWG Asset Toolchain - Common Types
# Copyright (c) Titan Project
#
# Common data types used across SWG formats, matching C++ implementations.

from __future__ import annotations
from dataclasses import dataclass, field
from typing import List, Tuple, Optional, Dict, Any
from enum import IntEnum, auto
import math

__all__ = [
    'Vector3', 'Quaternion', 'Transform', 'PackedArgb', 'Sphere', 'AxisAlignedBox',
    'CompressorType', 'JointRotationOrder', 'BlendTarget', 'Hardpoint',
    'PerShaderData', 'IndexedTriangleList'
]


# =============================================================================
# Math Types
# =============================================================================

@dataclass
class Vector3:
    """3D vector matching SWG Vector class."""
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0
    
    def __iter__(self):
        yield self.x
        yield self.y
        yield self.z
    
    def __getitem__(self, index):
        return (self.x, self.y, self.z)[index]
    
    def to_tuple(self) -> Tuple[float, float, float]:
        return (self.x, self.y, self.z)
    
    def to_blender(self) -> Tuple[float, float, float]:
        """Convert to Blender coordinate system (Y-up to Z-up)."""
        return (self.x, -self.z, self.y)
    
    @classmethod
    def from_blender(cls, x: float, y: float, z: float) -> 'Vector3':
        """Create from Blender coordinates."""
        return cls(x, z, -y)
    
    def magnitude(self) -> float:
        return math.sqrt(self.x * self.x + self.y * self.y + self.z * self.z)
    
    def normalized(self) -> 'Vector3':
        mag = self.magnitude()
        if mag == 0:
            return Vector3(0, 0, 0)
        return Vector3(self.x / mag, self.y / mag, self.z / mag)


@dataclass
class Quaternion:
    """Quaternion matching SWG Quaternion class."""
    w: float = 1.0
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0
    
    def __iter__(self):
        yield self.w
        yield self.x
        yield self.y
        yield self.z
    
    def to_tuple(self) -> Tuple[float, float, float, float]:
        return (self.w, self.x, self.y, self.z)
    
    def to_blender(self) -> Tuple[float, float, float, float]:
        """Convert to Blender quaternion (W, X, Y, Z) with coordinate transform."""
        return (self.w, self.x, -self.z, self.y)
    
    @classmethod
    def from_blender(cls, w: float, x: float, y: float, z: float) -> 'Quaternion':
        """Create from Blender quaternion."""
        return cls(w, x, z, -y)
    
    def magnitude(self) -> float:
        return math.sqrt(self.w * self.w + self.x * self.x + 
                        self.y * self.y + self.z * self.z)
    
    def normalized(self) -> 'Quaternion':
        mag = self.magnitude()
        if mag == 0:
            return Quaternion(1, 0, 0, 0)
        return Quaternion(self.w / mag, self.x / mag, 
                         self.y / mag, self.z / mag)


@dataclass
class Transform:
    """3x4 transformation matrix matching SWG Transform class."""
    # Row-major 3x4 matrix (rotation + translation)
    # [r00 r01 r02 tx]
    # [r10 r11 r12 ty]
    # [r20 r21 r22 tz]
    matrix: Tuple[Tuple[float, ...], ...] = field(
        default_factory=lambda: (
            (1.0, 0.0, 0.0, 0.0),
            (0.0, 1.0, 0.0, 0.0),
            (0.0, 0.0, 1.0, 0.0),
        )
    )
    
    @property
    def translation(self) -> Vector3:
        return Vector3(self.matrix[0][3], self.matrix[1][3], self.matrix[2][3])
    
    @staticmethod
    def identity() -> 'Transform':
        return Transform()
    
    def to_blender_matrix(self):
        """Convert to Blender 4x4 matrix."""
        # Will be imported when needed to avoid circular imports
        import mathutils
        
        # Convert from SWG (Y-up) to Blender (Z-up)
        m = self.matrix
        return mathutils.Matrix((
            (m[0][0], -m[0][2], m[0][1], m[0][3]),
            (-m[2][0], m[2][2], -m[2][1], -m[2][3]),
            (m[1][0], -m[1][2], m[1][1], m[1][3]),
            (0, 0, 0, 1)
        ))


@dataclass
class PackedArgb:
    """Packed ARGB color."""
    a: int = 255
    r: int = 255
    g: int = 255
    b: int = 255
    
    def to_float_tuple(self) -> Tuple[float, float, float, float]:
        """Convert to RGBA floats (0.0-1.0)."""
        return (self.r / 255.0, self.g / 255.0, 
                self.b / 255.0, self.a / 255.0)
    
    @classmethod
    def from_float_tuple(cls, r: float, g: float, b: float, a: float = 1.0) -> 'PackedArgb':
        return cls(
            a=int(a * 255),
            r=int(r * 255),
            g=int(g * 255),
            b=int(b * 255)
        )


@dataclass
class Sphere:
    """Bounding sphere."""
    center: Vector3 = field(default_factory=Vector3)
    radius: float = 0.0


@dataclass
class AxisAlignedBox:
    """Axis-aligned bounding box."""
    min_corner: Vector3 = field(default_factory=Vector3)
    max_corner: Vector3 = field(default_factory=Vector3)
    
    @property
    def center(self) -> Vector3:
        return Vector3(
            (self.min_corner.x + self.max_corner.x) / 2,
            (self.min_corner.y + self.max_corner.y) / 2,
            (self.min_corner.z + self.max_corner.z) / 2,
        )
    
    @property
    def extents(self) -> Vector3:
        return Vector3(
            (self.max_corner.x - self.min_corner.x) / 2,
            (self.max_corner.y - self.min_corner.y) / 2,
            (self.max_corner.z - self.min_corner.z) / 2,
        )


# =============================================================================
# Enums
# =============================================================================

class CompressorType(IntEnum):
    """Compression type used in mesh data."""
    NONE = 0
    DEPRECATED = 1
    ZLIB = 2


class JointRotationOrder(IntEnum):
    """Joint rotation order for skeletons."""
    XYZ = 0
    XZY = 1
    YXZ = 2
    YZX = 3
    ZXY = 4
    ZYX = 5


# =============================================================================
# Mesh Types
# =============================================================================

@dataclass
class BlendTarget:
    """Blend shape/morph target data."""
    name: str = ""
    positions: List[int] = field(default_factory=list)  # Indices into position array
    position_deltas: List[Vector3] = field(default_factory=list)
    normals: List[int] = field(default_factory=list)  # Indices into normal array  
    normal_deltas: List[Vector3] = field(default_factory=list)
    dot3_indices: List[int] = field(default_factory=list)
    dot3_deltas: List[Vector3] = field(default_factory=list)


@dataclass
class Hardpoint:
    """Attachment hardpoint."""
    name: str = ""
    parent_transform_name: str = ""
    transform: Transform = field(default_factory=Transform)


@dataclass 
class PerShaderData:
    """Per-shader mesh data."""
    shader_template_name: str = ""
    position_indices: List[int] = field(default_factory=list)
    normal_indices: List[int] = field(default_factory=list)
    lighting_normal_indices: List[int] = field(default_factory=list)
    color0_indices: List[int] = field(default_factory=list)
    color1_indices: List[int] = field(default_factory=list)
    tex_coord_set_indices: List[List[int]] = field(default_factory=list)
    primitives: List['DrawPrimitive'] = field(default_factory=list)


@dataclass
class DrawPrimitive:
    """Base class for draw primitives."""
    pass


@dataclass
class IndexedTriangleList(DrawPrimitive):
    """Indexed triangle list primitive."""
    indices: List[int] = field(default_factory=list)
    
    @property
    def triangle_count(self) -> int:
        return len(self.indices) // 3


@dataclass
class OccludedIndexedTriangleList(DrawPrimitive):
    """Occluded indexed triangle list with zone info."""
    occlusion_zone_combination: int = 0
    indices: List[int] = field(default_factory=list)
    
    @property
    def triangle_count(self) -> int:
        return len(self.indices) // 3


# =============================================================================
# Skeleton Types
# =============================================================================

@dataclass
class SkeletonJoint:
    """Skeleton joint/bone data."""
    name: str = ""
    parent_index: int = -1
    pre_multiply_rotation: Quaternion = field(default_factory=Quaternion)
    post_multiply_rotation: Quaternion = field(default_factory=Quaternion)
    bind_pose_translation: Vector3 = field(default_factory=Vector3)
    bind_pose_rotation: Quaternion = field(default_factory=Quaternion)
    rotation_order: JointRotationOrder = JointRotationOrder.XYZ


# =============================================================================
# Animation Types
# =============================================================================

@dataclass
class AnimationKeyframe:
    """Single keyframe in an animation channel."""
    time: float = 0.0
    value: Any = None  # Can be float, Vector3, Quaternion


@dataclass
class AnimationChannel:
    """Animation channel for a single property."""
    target_name: str = ""  # Joint name
    property_type: str = ""  # 'rotation', 'translation', 'scale'
    keyframes: List[AnimationKeyframe] = field(default_factory=list)


@dataclass
class AnimationMessage:
    """Animation event/message at a specific time."""
    time: float = 0.0
    name: str = ""
    

# =============================================================================
# Material/Shader Types
# =============================================================================

@dataclass
class ShaderEffect:
    """Shader effect reference."""
    name: str = ""
    effect_file: str = ""


@dataclass
class ShaderTexture:
    """Shader texture reference."""
    tag: str = ""
    texture_file: str = ""
    placeholder: bool = False


@dataclass
class ShaderMaterial:
    """Shader material properties."""
    ambient: Tuple[float, float, float, float] = (0.2, 0.2, 0.2, 1.0)
    diffuse: Tuple[float, float, float, float] = (0.8, 0.8, 0.8, 1.0)
    specular: Tuple[float, float, float, float] = (0.0, 0.0, 0.0, 1.0)
    emissive: Tuple[float, float, float, float] = (0.0, 0.0, 0.0, 1.0)
    shininess: float = 0.0


@dataclass
class ShaderData:
    """Complete shader data."""
    effect: Optional[ShaderEffect] = None
    material: ShaderMaterial = field(default_factory=ShaderMaterial)
    textures: List[ShaderTexture] = field(default_factory=list)
    alpha_reference: float = 0.0
    stencil_reference: int = 0
    
    # Unknown/preserved data for round-trip export
    unknown_chunks: Dict[str, bytes] = field(default_factory=dict)
