# SWG Asset Toolchain - Tag Utilities
# Copyright (c) Titan Project
#
# Matches C++ Tag implementation from sharedFoundation/Tag.h
# Tags are 4-byte identifiers stored in big-endian format

from __future__ import annotations
import struct
from typing import Union

__all__ = ['Tag', 'tag_to_string', 'string_to_tag', 'TAG', 'TAG3']


class Tag:
    """
    4-byte tag identifier used throughout SWG IFF files.
    
    Tags are stored in big-endian (network byte order) in files but
    are often displayed and manipulated as 4-character strings.
    
    Examples:
        Tag('FORM')  # Create from string
        Tag(0x464F524D)  # Create from integer
        Tag.from_bytes(b'FORM')  # Create from bytes
    """
    
    __slots__ = ('_value',)
    
    def __init__(self, value: Union[str, int, bytes, 'Tag'] = 0):
        if isinstance(value, Tag):
            self._value = value._value
        elif isinstance(value, str):
            self._value = string_to_tag(value)
        elif isinstance(value, bytes):
            if len(value) != 4:
                raise ValueError(f"Tag bytes must be exactly 4 bytes, got {len(value)}")
            self._value = struct.unpack('>I', value)[0]
        elif isinstance(value, int):
            self._value = value & 0xFFFFFFFF
        else:
            raise TypeError(f"Cannot create Tag from {type(value)}")
    
    @classmethod
    def from_bytes(cls, data: bytes, offset: int = 0) -> 'Tag':
        """Create a Tag from bytes at the given offset."""
        return cls(struct.unpack_from('>I', data, offset)[0])
    
    @property
    def value(self) -> int:
        """Raw integer value of the tag."""
        return self._value
    
    def to_bytes(self) -> bytes:
        """Convert tag to 4 bytes in big-endian format."""
        return struct.pack('>I', self._value)
    
    def to_string(self) -> str:
        """Convert tag to 4-character string representation."""
        return tag_to_string(self._value)
    
    def __eq__(self, other) -> bool:
        if isinstance(other, Tag):
            return self._value == other._value
        elif isinstance(other, int):
            return self._value == other
        elif isinstance(other, str):
            return self._value == string_to_tag(other)
        return False
    
    def __ne__(self, other) -> bool:
        return not self.__eq__(other)
    
    def __hash__(self) -> int:
        return hash(self._value)
    
    def __repr__(self) -> str:
        return f"Tag('{self.to_string()}')"
    
    def __str__(self) -> str:
        return self.to_string()
    
    def __int__(self) -> int:
        return self._value
    
    def __bool__(self) -> bool:
        return self._value != 0


def tag_to_string(tag: int) -> str:
    """
    Convert a 4-byte tag integer to its string representation.
    
    Non-printable characters are replaced with '?'.
    
    Args:
        tag: 32-bit unsigned integer tag value
        
    Returns:
        4-character string representation
    """
    chars = []
    for i in range(4):
        byte = (tag >> (24 - i * 8)) & 0xFF
        if 32 <= byte < 127:
            chars.append(chr(byte))
        else:
            chars.append('?')
    return ''.join(chars)


def string_to_tag(s: str) -> int:
    """
    Convert a string to a 4-byte tag integer.
    
    Strings shorter than 4 characters are space-padded on the right.
    Strings longer than 4 characters are truncated.
    
    Args:
        s: String to convert (typically 4 characters)
        
    Returns:
        32-bit unsigned integer tag value
    """
    # Pad with spaces if too short, truncate if too long
    s = s[:4].ljust(4)
    
    result = 0
    for i, c in enumerate(s):
        result |= (ord(c) & 0xFF) << (24 - i * 8)
    return result


def TAG(a: str, b: str, c: str, d: str) -> Tag:
    """
    Create a Tag from 4 individual characters.
    
    Matches C++ macro: TAG(a,b,c,d)
    
    Args:
        a, b, c, d: Single character strings
        
    Returns:
        Tag instance
    """
    return Tag(a + b + c + d)


def TAG3(a: str, b: str, c: str) -> Tag:
    """
    Create a Tag from 3 characters (space-padded).
    
    Matches C++ macro: TAG3(a,b,c)
    
    Args:
        a, b, c: Single character strings
        
    Returns:
        Tag instance with trailing space
    """
    return Tag(a + b + c + ' ')


# =============================================================================
# Common SWG Tags
# =============================================================================

# IFF Structure Tags
TAG_FORM = Tag('FORM')
TAG_DATA = Tag('DATA')
TAG_INFO = Tag('INFO')
TAG_NAME = Tag('NAME')

# Version Tags
TAG_0001 = Tag('0001')
TAG_0002 = Tag('0002')
TAG_0003 = Tag('0003')
TAG_0004 = Tag('0004')
TAG_0005 = Tag('0005')

# Mesh Tags
TAG_MESH = Tag('MESH')
TAG_CNTR = Tag('CNTR')
TAG_RADI = Tag('RADI')
TAG_SPHR = Tag('SPHR')

# Skeleton Tags
TAG_SKTM = Tag('SKTM')
TAG_PRNT = Tag('PRNT')
TAG_RPRE = Tag('RPRE')
TAG_RPST = Tag('RPST')
TAG_BPTR = Tag('BPTR')
TAG_BPRO = Tag('BPRO')
TAG_BPMJ = Tag('BPMJ')
TAG_JROR = Tag('JROR')

# Mesh Generator Tags
TAG_SKMG = Tag('SKMG')
TAG_POSN = Tag('POSN')
TAG_NORM = Tag('NORM')
TAG_TWHD = Tag('TWHD')
TAG_TWDT = Tag('TWDT')
TAG_DOT3 = Tag('DOT3')
TAG_HPTS = Tag('HPTS')
TAG_BLTS = Tag('BLTS')
TAG_BLT  = TAG3('B', 'L', 'T')
TAG_OZN  = TAG3('O', 'Z', 'N')
TAG_FOZC = Tag('FOZC')
TAG_OZC  = TAG3('O', 'Z', 'C')
TAG_XFNM = Tag('XFNM')
TAG_PSDT = Tag('PSDT')
TAG_PRIM = Tag('PRIM')
TAG_ITL  = TAG3('I', 'T', 'L')
TAG_OITL = Tag('OITL')
TAG_PIDX = Tag('PIDX')
TAG_NIDX = Tag('NIDX')
TAG_VDCL = Tag('VDCL')
TAG_TXCI = Tag('TXCI')
TAG_TCSD = Tag('TCSD')
TAG_TCSF = Tag('TCSF')
TAG_TRTS = Tag('TRTS')
TAG_TRT  = TAG3('T', 'R', 'T')
TAG_DYN  = TAG3('D', 'Y', 'N')
TAG_STAT = Tag('STAT')
TAG_ZTO  = TAG3('Z', 'T', 'O')

# Animation Tags
TAG_CKAT = Tag('CKAT')
TAG_KFAT = Tag('KFAT')
TAG_QCHN = Tag('QCHN')
TAG_SCHN = Tag('SCHN')
TAG_XCHN = Tag('XCHN')
TAG_APTS = Tag('APTS')
TAG_MESG = Tag('MESG')
TAG_LOCR = Tag('LOCR')
TAG_LOCS = Tag('LOCS')

# Appearance Tags
TAG_APPR = Tag('APPR')
TAG_APT  = TAG3('A', 'P', 'T')
TAG_SAT  = TAG3('S', 'A', 'T')
TAG_SMAT = Tag('SMAT')

# LOD Tags
TAG_DTLA = Tag('DTLA')
TAG_RADR = Tag('RADR')
TAG_CHLD = Tag('CHLD')
TAG_PIVT = Tag('PIVT')

# Shader Tags
TAG_SSHT = Tag('SSHT')
TAG_EFCT = Tag('EFCT')
TAG_ARVS = Tag('ARVS')
TAG_MATS = Tag('MATS')
TAG_TXMS = Tag('TXMS')
TAG_TCSS = Tag('TCSS')
TAG_TFNS = Tag('TFNS')
TAG_TSNS = Tag('TSNS')

# Portal/Floor Tags
TAG_PRTO = Tag('PRTO')
TAG_PRTL = Tag('PRTL')
TAG_IDTL = Tag('IDTL')
TAG_FLOR = Tag('FLOR')
TAG_PGRF = Tag('PGRF')
TAG_BTRE = Tag('BTRE')
TAG_BCRC = Tag('BCRC')
TAG_BEDG = Tag('BEDG')

# Component Tags
TAG_CMPA = Tag('CMPA')
TAG_CMPT = Tag('CMPT')
TAG_PART = Tag('PART')

# Particle Tags
TAG_PTCL = Tag('PTCL')
TAG_PEFT = Tag('PEFT')
TAG_WEFT = Tag('WEFT')
TAG_EMTR = Tag('EMTR')
TAG_EMGP = Tag('EMGP')
TAG_PTGP = Tag('PTGP')
TAG_PTAP = Tag('PTAP')

# Collision Tags
TAG_EXBX = Tag('EXBX')
TAG_EXSP = Tag('EXSP')
TAG_XCYL = Tag('XCYL')
TAG_CMSH = Tag('CMSH')
TAG_CPST = Tag('CPST')
TAG_XMSH = Tag('XMSH')
TAG_DTAL = Tag('DTAL')
TAG_VERT = Tag('VERT')
TAG_INDX = Tag('INDX')

# String Tags
TAG_STOT = Tag('STOT')
TAG_XXXX = Tag('XXXX')
TAG_DATA = Tag('DATA')
