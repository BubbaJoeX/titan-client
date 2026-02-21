# SWG Asset Toolchain - IFF Parser
# Copyright (c) Titan Project
#
# Python implementation matching C++ Iff class behavior from:
# D:/titan/client/src/engine/shared/library/sharedFile/src/shared/Iff.cpp
#
# IFF (Interchange File Format) is a hierarchical container format using:
# - FORM blocks: Container blocks with a name tag and nested content
# - Chunk blocks: Data blocks with a name tag and raw data
#
# All multi-byte values are stored in big-endian (network byte order).

from __future__ import annotations
import struct
import io
from dataclasses import dataclass, field
from typing import Optional, List, Union, BinaryIO, Tuple, Any
from pathlib import Path
from enum import Enum, auto

from .tag import Tag, TAG_FORM

__all__ = ['Iff', 'IffError', 'IffBlock', 'BlockType', 'SeekType']


class IffError(Exception):
    """Exception raised for IFF parsing/writing errors."""
    pass


class SeekType(Enum):
    """Seek type for chunk navigation."""
    BEGIN = auto()
    CURRENT = auto()
    END = auto()


class BlockType(Enum):
    """Type of IFF block."""
    FORM = auto()
    CHUNK = auto()
    EITHER = auto()


@dataclass
class StackEntry:
    """
    Internal stack entry tracking current position within nested blocks.
    
    Matches C++ Iff::Stack structure.
    """
    start: int = 0      # Start offset of block data
    length: int = 0     # Total length of block data
    used: int = 0       # Bytes consumed within this block


@dataclass
class IffBlock:
    """
    Represents a parsed IFF block (FORM or chunk).
    
    Used for inspection and tree traversal.
    """
    tag: Tag
    block_type: BlockType
    offset: int
    length: int
    children: List['IffBlock'] = field(default_factory=list)
    raw_data: Optional[bytes] = None
    form_name: Optional[Tag] = None  # For FORM blocks
    
    def is_form(self) -> bool:
        return self.block_type == BlockType.FORM
    
    def is_chunk(self) -> bool:
        return self.block_type == BlockType.CHUNK
    
    def get_child(self, tag: Union[Tag, str]) -> Optional['IffBlock']:
        """Find first child with matching tag."""
        target = Tag(tag) if isinstance(tag, str) else tag
        for child in self.children:
            if child.tag == target:
                return child
        return None
    
    def get_children(self, tag: Union[Tag, str]) -> List['IffBlock']:
        """Find all children with matching tag."""
        target = Tag(tag) if isinstance(tag, str) else tag
        return [c for c in self.children if c.tag == target]


class Iff:
    """
    IFF file reader/writer matching the C++ Iff class.
    
    Supports reading and writing SWG IFF files with proper handling of:
    - FORM containers (hierarchical grouping)
    - Data chunks (raw data blocks)
    - Big-endian byte order
    - Nested block navigation
    
    Usage (Reading):
        iff = Iff.from_file("example.msh")
        iff.enter_form("MESH")
        iff.enter_form("0005")
        iff.enter_chunk("DATA")
        value = iff.read_float()
        iff.exit_chunk()
        iff.exit_form()
        iff.exit_form()
    
    Usage (Writing):
        iff = Iff(growable=True)
        iff.insert_form("MESH")
        iff.insert_form("0005")
        iff.insert_chunk("DATA")
        iff.insert_float(3.14)
        iff.exit_chunk()
        iff.exit_form()
        iff.exit_form()
        iff.write("output.msh")
    """
    
    DEFAULT_STACK_DEPTH = 64
    FORM_OVERHEAD = 12  # TAG(4) + LENGTH(4) + FORM_NAME(4)
    CHUNK_OVERHEAD = 8  # TAG(4) + LENGTH(4)
    
    def __init__(self, data: Optional[bytes] = None, 
                 initial_size: int = 4096,
                 growable: bool = True,
                 owns_data: bool = True):
        """
        Initialize an Iff instance.
        
        Args:
            data: Raw IFF data to parse (for reading)
            initial_size: Initial buffer size (for writing)
            growable: Whether buffer can grow (for writing)
            owns_data: Whether this instance owns the data buffer
        """
        self._filename: Optional[str] = None
        self._stack: List[StackEntry] = [StackEntry() for _ in range(self.DEFAULT_STACK_DEPTH)]
        self._stack_depth: int = 0
        self._in_chunk: bool = False
        self._growable: bool = growable
        self._nonlinear: bool = False
        self._owns_data: bool = owns_data
        
        if data is not None:
            self._data = bytearray(data)
            self._stack[0].length = len(data)
        else:
            self._data = bytearray(initial_size)
            self._stack[0].length = 0
    
    @classmethod
    def from_file(cls, filepath: Union[str, Path]) -> 'Iff':
        """
        Load an IFF from a file.
        
        Args:
            filepath: Path to the IFF file
            
        Returns:
            Iff instance with loaded data
            
        Raises:
            IffError: If file cannot be read or is invalid
        """
        filepath = Path(filepath)
        if not filepath.exists():
            raise IffError(f"File not found: {filepath}")
        
        with open(filepath, 'rb') as f:
            data = f.read()
        
        if len(data) < 12:
            raise IffError(f"File too small to be valid IFF: {filepath}")
        
        iff = cls(data)
        iff._filename = str(filepath)
        return iff
    
    @classmethod
    def from_bytes(cls, data: bytes) -> 'Iff':
        """Create an Iff from raw bytes."""
        return cls(data)
    
    @property
    def filename(self) -> Optional[str]:
        """Get the filename if loaded from file."""
        return self._filename
    
    @property
    def raw_data(self) -> bytes:
        """Get the raw IFF data."""
        return bytes(self._data[:self._stack[0].length])
    
    @property
    def raw_data_size(self) -> int:
        """Get the size of the raw IFF data."""
        return self._stack[0].length
    
    # =========================================================================
    # Navigation - Current Position Info
    # =========================================================================
    
    def get_current_name(self) -> Tag:
        """Get the tag name of the current block."""
        return self._get_first_tag(self._stack_depth)
    
    def get_current_length(self) -> int:
        """Get the data length of the current block."""
        return self._get_length(self._stack_depth)
    
    def is_current_chunk(self) -> bool:
        """Check if the current block is a chunk."""
        tag = self._get_first_tag(self._stack_depth)
        return tag != TAG_FORM
    
    def is_current_form(self) -> bool:
        """Check if the current block is a form."""
        tag = self._get_first_tag(self._stack_depth)
        return tag == TAG_FORM
    
    def at_end_of_form(self) -> bool:
        """Check if at end of current form."""
        entry = self._stack[self._stack_depth]
        return entry.used >= entry.length
    
    def get_blocks_left(self) -> int:
        """Get the number of blocks remaining in current form."""
        count = 0
        saved_used = self._stack[self._stack_depth].used
        
        while not self.at_end_of_form():
            count += 1
            self._skip_block()
        
        self._stack[self._stack_depth].used = saved_used
        return count
    
    def get_chunk_length_total(self, element_size: int = 1) -> int:
        """Get total chunk length in elements."""
        if not self._in_chunk:
            raise IffError("Not inside a chunk")
        return self._stack[self._stack_depth].length // element_size
    
    def get_chunk_length_left(self, element_size: int = 1) -> int:
        """Get remaining chunk length in elements."""
        if not self._in_chunk:
            raise IffError("Not inside a chunk")
        entry = self._stack[self._stack_depth]
        return (entry.length - entry.used) // element_size
    
    # =========================================================================
    # Navigation - Enter/Exit Forms
    # =========================================================================
    
    def enter_form(self, name: Optional[Union[Tag, str]] = None, 
                   optional: bool = False) -> bool:
        """
        Enter a FORM block.
        
        Args:
            name: Expected form name (None to enter any form)
            optional: If True, return False instead of raising on failure
            
        Returns:
            True if form was entered, False if optional and not found
            
        Raises:
            IffError: If not optional and form not found
        """
        if self._in_chunk:
            raise IffError("Cannot enter form while inside chunk")
        
        if self.at_end_of_form():
            if optional:
                return False
            raise IffError("At end of form, cannot enter new form")
        
        tag = self._get_first_tag(self._stack_depth)
        if tag != TAG_FORM:
            if optional:
                return False
            raise IffError(f"Expected FORM, got {tag}")
        
        if name is not None:
            target = Tag(name) if isinstance(name, str) else name
            form_name = self._get_second_tag(self._stack_depth)
            if form_name != target:
                if optional:
                    return False
                raise IffError(f"Expected form {target}, got {form_name}")
        
        # Push onto stack
        length = self._get_length(self._stack_depth)
        self._grow_stack_if_needed()
        self._stack_depth += 1
        
        entry = self._stack[self._stack_depth]
        entry.start = (self._stack[self._stack_depth - 1].start + 
                       self._stack[self._stack_depth - 1].used + 
                       self.FORM_OVERHEAD)
        entry.length = length - 4  # Subtract form name tag
        entry.used = 0
        
        # Advance parent
        self._stack[self._stack_depth - 1].used += length + 8  # TAG + LENGTH + content
        
        return True
    
    def exit_form(self, name: Optional[Union[Tag, str]] = None,
                  may_not_be_at_end: bool = False) -> None:
        """
        Exit the current FORM block.
        
        Args:
            name: Expected form name to verify (optional)
            may_not_be_at_end: If True, don't require being at end of form
            
        Raises:
            IffError: If validation fails
        """
        if self._in_chunk:
            raise IffError("Cannot exit form while inside chunk")
        
        if self._stack_depth == 0:
            raise IffError("Cannot exit root level")
        
        if not may_not_be_at_end and not self.at_end_of_form():
            raise IffError("Not at end of form")
        
        self._stack_depth -= 1
    
    # =========================================================================
    # Navigation - Enter/Exit Chunks
    # =========================================================================
    
    def enter_chunk(self, name: Optional[Union[Tag, str]] = None,
                    optional: bool = False) -> bool:
        """
        Enter a chunk block.
        
        Args:
            name: Expected chunk name (None to enter any chunk)
            optional: If True, return False instead of raising on failure
            
        Returns:
            True if chunk was entered, False if optional and not found
        """
        if self._in_chunk:
            raise IffError("Already inside a chunk")
        
        if self.at_end_of_form():
            if optional:
                return False
            raise IffError("At end of form, cannot enter chunk")
        
        tag = self._get_first_tag(self._stack_depth)
        if tag == TAG_FORM:
            if optional:
                return False
            raise IffError(f"Expected chunk, got FORM")
        
        if name is not None:
            target = Tag(name) if isinstance(name, str) else name
            if tag != target:
                if optional:
                    return False
                raise IffError(f"Expected chunk {target}, got {tag}")
        
        # Push onto stack
        length = self._get_length(self._stack_depth)
        self._grow_stack_if_needed()
        self._stack_depth += 1
        
        entry = self._stack[self._stack_depth]
        entry.start = (self._stack[self._stack_depth - 1].start +
                       self._stack[self._stack_depth - 1].used +
                       self.CHUNK_OVERHEAD)
        entry.length = length
        entry.used = 0
        
        self._in_chunk = True
        
        # Advance parent
        self._stack[self._stack_depth - 1].used += length + 8
        
        return True
    
    def exit_chunk(self, name: Optional[Union[Tag, str]] = None,
                   may_not_be_at_end: bool = False) -> None:
        """
        Exit the current chunk block.
        
        Args:
            name: Expected chunk name to verify (optional)
            may_not_be_at_end: If True, don't require being at end of chunk
        """
        if not self._in_chunk:
            raise IffError("Not inside a chunk")
        
        entry = self._stack[self._stack_depth]
        if not may_not_be_at_end and entry.used < entry.length:
            raise IffError(f"Not at end of chunk ({entry.used}/{entry.length} bytes read)")
        
        self._in_chunk = False
        self._stack_depth -= 1
    
    # =========================================================================
    # Navigation - Seeking
    # =========================================================================
    
    def seek(self, name: Union[Tag, str]) -> bool:
        """Seek to a block with the given name."""
        target = Tag(name) if isinstance(name, str) else name
        
        while not self.at_end_of_form():
            current = self._get_first_tag(self._stack_depth)
            if current == target:
                return True
            if current == TAG_FORM:
                form_name = self._get_second_tag(self._stack_depth)
                if form_name == target:
                    return True
            self._skip_block()
        
        return False
    
    def seek_form(self, name: Union[Tag, str]) -> bool:
        """Seek to a FORM with the given name."""
        target = Tag(name) if isinstance(name, str) else name
        
        while not self.at_end_of_form():
            if self.is_current_form():
                form_name = self._get_second_tag(self._stack_depth)
                if form_name == target:
                    return True
            self._skip_block()
        
        return False
    
    def seek_chunk(self, name: Union[Tag, str]) -> bool:
        """Seek to a chunk with the given name."""
        target = Tag(name) if isinstance(name, str) else name
        
        while not self.at_end_of_form():
            if self.is_current_chunk():
                chunk_name = self._get_first_tag(self._stack_depth)
                if chunk_name == target:
                    return True
            self._skip_block()
        
        return False
    
    def go_forward(self, count: int = 1, optional: bool = False) -> bool:
        """Skip forward by count blocks."""
        for _ in range(count):
            if self.at_end_of_form():
                if optional:
                    return False
                raise IffError("At end of form")
            self._skip_block()
        return True
    
    def go_to_top_of_form(self) -> None:
        """Reset position to start of current form."""
        self._stack[self._stack_depth].used = 0
    
    def allow_nonlinear_functions(self) -> None:
        """Enable nonlinear seeking within chunks."""
        self._nonlinear = True
    
    def seek_within_chunk(self, offset: int, seek_type: SeekType) -> None:
        """Seek to a position within the current chunk."""
        if not self._in_chunk:
            raise IffError("Not inside a chunk")
        if not self._nonlinear:
            raise IffError("Nonlinear functions not enabled")
        
        entry = self._stack[self._stack_depth]
        
        if seek_type == SeekType.BEGIN:
            new_pos = offset
        elif seek_type == SeekType.CURRENT:
            new_pos = entry.used + offset
        elif seek_type == SeekType.END:
            new_pos = entry.length + offset
        else:
            raise IffError(f"Invalid seek type: {seek_type}")
        
        if new_pos < 0 or new_pos > entry.length:
            raise IffError(f"Seek position out of bounds: {new_pos}")
        
        entry.used = new_pos
    
    # =========================================================================
    # Reading - Primitives
    # =========================================================================
    
    def read_bool8(self) -> bool:
        """Read a boolean (1 byte)."""
        return self._read_bytes(1)[0] != 0
    
    def read_int8(self) -> int:
        """Read a signed 8-bit integer."""
        return struct.unpack('b', self._read_bytes(1))[0]
    
    def read_uint8(self) -> int:
        """Read an unsigned 8-bit integer."""
        return self._read_bytes(1)[0]
    
    def read_int16(self) -> int:
        """Read a signed 16-bit integer (big-endian)."""
        return struct.unpack('>h', self._read_bytes(2))[0]
    
    def read_uint16(self) -> int:
        """Read an unsigned 16-bit integer (big-endian)."""
        return struct.unpack('>H', self._read_bytes(2))[0]
    
    def read_int32(self) -> int:
        """Read a signed 32-bit integer (big-endian)."""
        return struct.unpack('>i', self._read_bytes(4))[0]
    
    def read_uint32(self) -> int:
        """Read an unsigned 32-bit integer (big-endian)."""
        return struct.unpack('>I', self._read_bytes(4))[0]
    
    def read_float(self) -> float:
        """Read a 32-bit float (big-endian)."""
        return struct.unpack('>f', self._read_bytes(4))[0]
    
    def read_string(self, max_length: Optional[int] = None) -> str:
        """Read a null-terminated string."""
        chars = []
        while True:
            if max_length is not None and len(chars) >= max_length:
                break
            b = self._read_bytes(1)[0]
            if b == 0:
                break
            chars.append(chr(b))
        return ''.join(chars)
    
    def read_bytes(self, length: int) -> bytes:
        """Read raw bytes."""
        return bytes(self._read_bytes(length))
    
    # =========================================================================
    # Reading - Compound Types
    # =========================================================================
    
    def read_vector(self) -> Tuple[float, float, float]:
        """Read a 3D vector (x, y, z)."""
        x = self.read_float()
        y = self.read_float()
        z = self.read_float()
        return (x, y, z)
    
    def read_quaternion(self) -> Tuple[float, float, float, float]:
        """Read a quaternion (w, x, y, z)."""
        w = self.read_float()
        x = self.read_float()
        y = self.read_float()
        z = self.read_float()
        return (w, x, y, z)
    
    def read_transform(self) -> Tuple[Tuple[float, ...], ...]:
        """Read a 3x4 transform matrix (row-major)."""
        rows = []
        for _ in range(3):
            row = tuple(self.read_float() for _ in range(4))
            rows.append(row)
        return tuple(rows)
    
    def read_vector_argb(self) -> Tuple[float, float, float, float]:
        """Read an ARGB color vector."""
        a = self.read_float()
        r = self.read_float()
        g = self.read_float()
        b = self.read_float()
        return (a, r, g, b)
    
    # =========================================================================
    # Reading - Arrays
    # =========================================================================
    
    def read_int8_array(self, count: int) -> List[int]:
        """Read an array of signed 8-bit integers."""
        return list(struct.unpack(f'{count}b', self._read_bytes(count)))
    
    def read_uint8_array(self, count: int) -> List[int]:
        """Read an array of unsigned 8-bit integers."""
        return list(self._read_bytes(count))
    
    def read_int16_array(self, count: int) -> List[int]:
        """Read an array of signed 16-bit integers."""
        return list(struct.unpack(f'>{count}h', self._read_bytes(count * 2)))
    
    def read_uint16_array(self, count: int) -> List[int]:
        """Read an array of unsigned 16-bit integers."""
        return list(struct.unpack(f'>{count}H', self._read_bytes(count * 2)))
    
    def read_int32_array(self, count: int) -> List[int]:
        """Read an array of signed 32-bit integers."""
        return list(struct.unpack(f'>{count}i', self._read_bytes(count * 4)))
    
    def read_uint32_array(self, count: int) -> List[int]:
        """Read an array of unsigned 32-bit integers."""
        return list(struct.unpack(f'>{count}I', self._read_bytes(count * 4)))
    
    def read_float_array(self, count: int) -> List[float]:
        """Read an array of 32-bit floats."""
        return list(struct.unpack(f'>{count}f', self._read_bytes(count * 4)))
    
    def read_vector_array(self, count: int) -> List[Tuple[float, float, float]]:
        """Read an array of 3D vectors."""
        return [self.read_vector() for _ in range(count)]
    
    def read_quaternion_array(self, count: int) -> List[Tuple[float, float, float, float]]:
        """Read an array of quaternions."""
        return [self.read_quaternion() for _ in range(count)]
    
    def read_rest_bytes(self) -> bytes:
        """Read all remaining bytes in the current chunk."""
        entry = self._stack[self._stack_depth]
        remaining = entry.length - entry.used
        return bytes(self._read_bytes(remaining))
    
    def read_rest_int32(self) -> List[int]:
        """Read all remaining 32-bit integers in the current chunk."""
        count = self.get_chunk_length_left(4)
        return self.read_int32_array(count)
    
    def read_rest_float(self) -> List[float]:
        """Read all remaining floats in the current chunk."""
        count = self.get_chunk_length_left(4)
        return self.read_float_array(count)
    
    # =========================================================================
    # Writing - Forms and Chunks
    # =========================================================================
    
    def insert_form(self, name: Union[Tag, str], enter: bool = True) -> None:
        """
        Insert a new FORM block.
        
        Args:
            name: Form name tag
            enter: Whether to automatically enter the form
        """
        if self._in_chunk:
            raise IffError("Cannot insert form while inside chunk")
        
        tag = Tag(name) if isinstance(name, str) else name
        
        self._adjust_data(self.FORM_OVERHEAD)
        offset = self._stack[self._stack_depth].start + self._stack[self._stack_depth].used
        
        # Write FORM tag
        struct.pack_into('>I', self._data, offset, TAG_FORM.value)
        # Write length (will be updated)
        struct.pack_into('>I', self._data, offset + 4, 4)
        # Write form name
        struct.pack_into('>I', self._data, offset + 8, tag.value)
        
        self._stack[self._stack_depth].used += self.FORM_OVERHEAD
        
        if enter:
            self._grow_stack_if_needed()
            self._stack_depth += 1
            entry = self._stack[self._stack_depth]
            entry.start = offset + self.FORM_OVERHEAD
            entry.length = 0
            entry.used = 0
    
    def insert_chunk(self, name: Union[Tag, str], enter: bool = True) -> None:
        """
        Insert a new chunk block.
        
        Args:
            name: Chunk name tag
            enter: Whether to automatically enter the chunk
        """
        if self._in_chunk:
            raise IffError("Cannot insert chunk while inside chunk")
        
        tag = Tag(name) if isinstance(name, str) else name
        
        self._adjust_data(self.CHUNK_OVERHEAD)
        offset = self._stack[self._stack_depth].start + self._stack[self._stack_depth].used
        
        # Write chunk tag
        struct.pack_into('>I', self._data, offset, tag.value)
        # Write length (will be updated)
        struct.pack_into('>I', self._data, offset + 4, 0)
        
        self._stack[self._stack_depth].used += self.CHUNK_OVERHEAD
        
        if enter:
            self._grow_stack_if_needed()
            self._stack_depth += 1
            entry = self._stack[self._stack_depth]
            entry.start = offset + self.CHUNK_OVERHEAD
            entry.length = 0
            entry.used = 0
            self._in_chunk = True
    
    def insert_iff(self, other: 'Iff') -> None:
        """Insert another IFF's data at the current position."""
        if self._in_chunk:
            raise IffError("Cannot insert IFF while inside chunk")
        
        data = other.raw_data
        self._adjust_data(len(data))
        offset = self._stack[self._stack_depth].start + self._stack[self._stack_depth].used
        self._data[offset:offset + len(data)] = data
        self._stack[self._stack_depth].used += len(data)
    
    # =========================================================================
    # Writing - Primitives
    # =========================================================================
    
    def insert_chunk_data(self, data: bytes) -> None:
        """Insert raw bytes into the current chunk."""
        if not self._in_chunk:
            raise IffError("Not inside a chunk")
        
        self._adjust_data(len(data))
        offset = self._stack[self._stack_depth].start + self._stack[self._stack_depth].used
        self._data[offset:offset + len(data)] = data
        self._stack[self._stack_depth].used += len(data)
        self._stack[self._stack_depth].length += len(data)
        self._update_chunk_length()
    
    def insert_bool8(self, value: bool) -> None:
        """Insert a boolean (1 byte)."""
        self.insert_chunk_data(bytes([1 if value else 0]))
    
    def insert_int8(self, value: int) -> None:
        """Insert a signed 8-bit integer."""
        self.insert_chunk_data(struct.pack('b', value))
    
    def insert_uint8(self, value: int) -> None:
        """Insert an unsigned 8-bit integer."""
        self.insert_chunk_data(struct.pack('B', value))
    
    def insert_int16(self, value: int) -> None:
        """Insert a signed 16-bit integer (big-endian)."""
        self.insert_chunk_data(struct.pack('>h', value))
    
    def insert_uint16(self, value: int) -> None:
        """Insert an unsigned 16-bit integer (big-endian)."""
        self.insert_chunk_data(struct.pack('>H', value))
    
    def insert_int32(self, value: int) -> None:
        """Insert a signed 32-bit integer (big-endian)."""
        self.insert_chunk_data(struct.pack('>i', value))
    
    def insert_uint32(self, value: int) -> None:
        """Insert an unsigned 32-bit integer (big-endian)."""
        self.insert_chunk_data(struct.pack('>I', value))
    
    def insert_float(self, value: float) -> None:
        """Insert a 32-bit float (big-endian)."""
        self.insert_chunk_data(struct.pack('>f', value))
    
    def insert_string(self, value: str) -> None:
        """Insert a null-terminated string."""
        self.insert_chunk_data(value.encode('latin-1') + b'\x00')
    
    # =========================================================================
    # Writing - Compound Types
    # =========================================================================
    
    def insert_vector(self, x: float, y: float, z: float) -> None:
        """Insert a 3D vector."""
        self.insert_chunk_data(struct.pack('>fff', x, y, z))
    
    def insert_quaternion(self, w: float, x: float, y: float, z: float) -> None:
        """Insert a quaternion."""
        self.insert_chunk_data(struct.pack('>ffff', w, x, y, z))
    
    def insert_transform(self, matrix: Tuple[Tuple[float, ...], ...]) -> None:
        """Insert a 3x4 transform matrix."""
        for row in matrix:
            for val in row:
                self.insert_float(val)
    
    # =========================================================================
    # File I/O
    # =========================================================================
    
    def write(self, filepath: Union[str, Path]) -> None:
        """Write the IFF to a file."""
        filepath = Path(filepath)
        filepath.parent.mkdir(parents=True, exist_ok=True)
        
        with open(filepath, 'wb') as f:
            f.write(self.raw_data)
    
    # =========================================================================
    # Tree Inspection
    # =========================================================================
    
    def parse_tree(self) -> IffBlock:
        """
        Parse the entire IFF structure into a tree of IffBlock objects.
        
        Returns:
            Root IffBlock representing the entire file
        """
        # Save state
        saved_depth = self._stack_depth
        saved_in_chunk = self._in_chunk
        saved_used = [s.used for s in self._stack[:saved_depth + 1]]
        
        # Reset to start
        self._stack_depth = 0
        self._in_chunk = False
        self._stack[0].used = 0
        
        try:
            root = IffBlock(
                tag=Tag('ROOT'),
                block_type=BlockType.FORM,
                offset=0,
                length=self._stack[0].length,
            )
            
            while not self.at_end_of_form():
                block = self._parse_block()
                root.children.append(block)
            
            return root
        finally:
            # Restore state
            self._stack_depth = saved_depth
            self._in_chunk = saved_in_chunk
            for i, used in enumerate(saved_used):
                self._stack[i].used = used
    
    def _parse_block(self) -> IffBlock:
        """Parse a single block at the current position."""
        offset = self._stack[self._stack_depth].start + self._stack[self._stack_depth].used
        tag = self._get_first_tag(self._stack_depth)
        length = self._get_length(self._stack_depth)
        
        if tag == TAG_FORM:
            form_name = self._get_second_tag(self._stack_depth)
            block = IffBlock(
                tag=form_name,
                block_type=BlockType.FORM,
                offset=offset,
                length=length + 8,
                form_name=form_name,
            )
            
            self.enter_form()
            while not self.at_end_of_form():
                child = self._parse_block()
                block.children.append(child)
            self.exit_form()
        else:
            block = IffBlock(
                tag=tag,
                block_type=BlockType.CHUNK,
                offset=offset,
                length=length + 8,
            )
            
            # Store raw chunk data
            self.enter_chunk()
            block.raw_data = self.read_rest_bytes()
            self.exit_chunk()
        
        return block
    
    # =========================================================================
    # Internal Methods
    # =========================================================================
    
    def _read_bytes(self, length: int) -> bytearray:
        """Read bytes from current position."""
        if not self._in_chunk:
            raise IffError("Cannot read data outside of chunk")
        
        entry = self._stack[self._stack_depth]
        if entry.used + length > entry.length:
            raise IffError(f"Read overflow: want {length}, have {entry.length - entry.used}")
        
        start = entry.start + entry.used
        entry.used += length
        return self._data[start:start + length]
    
    def _get_first_tag(self, depth: int) -> Tag:
        """Get the first tag at the given stack depth."""
        entry = self._stack[depth]
        offset = entry.start + entry.used
        return Tag(struct.unpack_from('>I', self._data, offset)[0])
    
    def _get_second_tag(self, depth: int) -> Tag:
        """Get the second tag (form name) at the given stack depth."""
        entry = self._stack[depth]
        offset = entry.start + entry.used + 8  # Skip first tag and length
        return Tag(struct.unpack_from('>I', self._data, offset)[0])
    
    def _get_length(self, depth: int, offset: int = 0) -> int:
        """Get the length field at the given stack depth."""
        entry = self._stack[depth]
        pos = entry.start + entry.used + offset + 4  # Skip tag
        return struct.unpack_from('>I', self._data, pos)[0]
    
    def _skip_block(self) -> None:
        """Skip the current block."""
        length = self._get_length(self._stack_depth)
        self._stack[self._stack_depth].used += length + 8
    
    def _grow_stack_if_needed(self) -> None:
        """Grow the stack if needed."""
        if self._stack_depth + 1 >= len(self._stack):
            new_entries = [StackEntry() for _ in range(len(self._stack))]
            self._stack.extend(new_entries)
    
    def _adjust_data(self, size: int) -> None:
        """Adjust data buffer size for writing."""
        needed = self._stack[0].length + size
        
        if needed > len(self._data):
            if not self._growable:
                raise IffError(f"Data overflow: need {needed}, have {len(self._data)}")
            
            new_length = len(self._data)
            while new_length < needed:
                new_length *= 2
            
            new_data = bytearray(new_length)
            new_data[:len(self._data)] = self._data
            self._data = new_data
        
        # Update lengths
        for i in range(self._stack_depth + 1):
            self._stack[i].length += size
    
    def _update_chunk_length(self) -> None:
        """Update the length field in the chunk header."""
        if not self._in_chunk or self._stack_depth == 0:
            return
        
        entry = self._stack[self._stack_depth]
        length_offset = entry.start - 4
        struct.pack_into('>I', self._data, length_offset, entry.length)
        
        # Update parent form lengths
        for i in range(self._stack_depth - 1, 0, -1):
            parent = self._stack[i]
            length_offset = parent.start - 8
            struct.pack_into('>I', self._data, length_offset, parent.length + 4)
