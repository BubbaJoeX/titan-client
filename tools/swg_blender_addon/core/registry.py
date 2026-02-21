# SWG Asset Toolchain - Format Registry
# Copyright (c) Titan Project
#
# Central registry for format handlers, allowing automatic format detection
# and modular format support.

from __future__ import annotations
from typing import Dict, Type, Optional, Callable, List, Any, TYPE_CHECKING
from dataclasses import dataclass, field
from pathlib import Path
from abc import ABC, abstractmethod

from .tag import Tag
from .iff import Iff, IffError

if TYPE_CHECKING:
    import bpy

__all__ = [
    'FormatHandler', 'FormatRegistry', 'registry',
    'FormatInfo', 'ImportResult', 'ExportResult'
]


@dataclass
class FormatInfo:
    """Information about a registered format."""
    name: str
    description: str
    extensions: List[str]
    root_tags: List[Tag]
    can_import: bool = True
    can_export: bool = True
    is_fully_understood: bool = False  # True if format is completely documented
    handler_class: Optional[Type['FormatHandler']] = None


@dataclass
class ImportResult:
    """Result of an import operation."""
    success: bool
    message: str = ""
    warnings: List[str] = field(default_factory=list)
    objects: List[Any] = field(default_factory=list)  # Created Blender objects
    unknown_chunks: Dict[str, bytes] = field(default_factory=dict)  # For round-trip


@dataclass
class ExportResult:
    """Result of an export operation."""
    success: bool
    message: str = ""
    warnings: List[str] = field(default_factory=list)
    bytes_written: int = 0


class FormatHandler(ABC):
    """
    Base class for format handlers.
    
    Each format (MSH, SKT, MGN, etc.) should have a handler that inherits
    from this class and implements import/export methods.
    """
    
    # Override these in subclasses
    FORMAT_NAME: str = "Unknown"
    FORMAT_DESCRIPTION: str = ""
    FILE_EXTENSIONS: List[str] = []
    ROOT_TAGS: List[Tag] = []
    CAN_IMPORT: bool = True
    CAN_EXPORT: bool = True
    IS_FULLY_UNDERSTOOD: bool = False
    
    def __init__(self, context: Optional[Any] = None):
        """
        Initialize handler.
        
        Args:
            context: Blender context (optional)
        """
        self.context = context
        self.warnings: List[str] = []
        self.unknown_chunks: Dict[str, bytes] = {}
    
    def warn(self, message: str) -> None:
        """Add a warning message."""
        self.warnings.append(message)
    
    def preserve_chunk(self, tag: Tag, data: bytes) -> None:
        """Preserve unknown chunk data for round-trip export."""
        self.unknown_chunks[str(tag)] = data
    
    @abstractmethod
    def import_file(self, filepath: str, **options) -> ImportResult:
        """
        Import a file.
        
        Args:
            filepath: Path to file to import
            **options: Format-specific import options
            
        Returns:
            ImportResult with created objects
        """
        pass
    
    @abstractmethod
    def export_file(self, filepath: str, objects: List[Any], **options) -> ExportResult:
        """
        Export objects to a file.
        
        Args:
            filepath: Path to write file
            objects: Blender objects to export
            **options: Format-specific export options
            
        Returns:
            ExportResult with status
        """
        pass
    
    def validate(self, iff: Iff) -> bool:
        """
        Validate that an IFF matches this format.
        
        Args:
            iff: IFF to validate
            
        Returns:
            True if valid for this format
        """
        try:
            if iff.is_current_form():
                iff.enter_form()
                current = iff.get_current_name()
                iff.exit_form()
                return current in self.ROOT_TAGS
            return self.get_current_name() in self.ROOT_TAGS
        except IffError:
            return False
    
    @classmethod
    def get_format_info(cls) -> FormatInfo:
        """Get format information."""
        return FormatInfo(
            name=cls.FORMAT_NAME,
            description=cls.FORMAT_DESCRIPTION,
            extensions=cls.FILE_EXTENSIONS,
            root_tags=cls.ROOT_TAGS,
            can_import=cls.CAN_IMPORT,
            can_export=cls.CAN_EXPORT,
            is_fully_understood=cls.IS_FULLY_UNDERSTOOD,
            handler_class=cls,
        )


class FormatRegistry:
    """
    Central registry for format handlers.
    
    Handles format detection and routing to appropriate handlers.
    """
    
    def __init__(self):
        self._handlers: Dict[str, Type[FormatHandler]] = {}
        self._tag_map: Dict[Tag, Type[FormatHandler]] = {}
        self._extension_map: Dict[str, Type[FormatHandler]] = {}
    
    def register(self, handler_class: Type[FormatHandler]) -> None:
        """
        Register a format handler.
        
        Args:
            handler_class: FormatHandler subclass to register
        """
        info = handler_class.get_format_info()
        
        self._handlers[info.name] = handler_class
        
        for tag in info.root_tags:
            self._tag_map[tag] = handler_class
        
        for ext in info.extensions:
            ext_lower = ext.lower().lstrip('.')
            self._extension_map[ext_lower] = handler_class
    
    def unregister(self, handler_class: Type[FormatHandler]) -> None:
        """
        Unregister a format handler.
        
        Args:
            handler_class: FormatHandler subclass to unregister
        """
        info = handler_class.get_format_info()
        
        self._handlers.pop(info.name, None)
        
        for tag in info.root_tags:
            self._tag_map.pop(tag, None)
        
        for ext in info.extensions:
            ext_lower = ext.lower().lstrip('.')
            self._extension_map.pop(ext_lower, None)
    
    def get_handler_for_file(self, filepath: str, 
                              context: Optional[Any] = None) -> Optional[FormatHandler]:
        """
        Get an appropriate handler for a file.
        
        First attempts to detect format from file contents, then falls back
        to extension-based detection.
        
        Args:
            filepath: Path to file
            context: Blender context
            
        Returns:
            FormatHandler instance or None if no handler found
        """
        path = Path(filepath)
        
        # Try to detect from file contents
        if path.exists():
            try:
                iff = Iff.from_file(filepath)
                handler_class = self._detect_from_iff(iff)
                if handler_class:
                    return handler_class(context)
            except Exception:
                pass
        
        # Fall back to extension
        ext = path.suffix.lower().lstrip('.')
        handler_class = self._extension_map.get(ext)
        if handler_class:
            return handler_class(context)
        
        return None
    
    def get_handler_by_name(self, name: str, 
                            context: Optional[Any] = None) -> Optional[FormatHandler]:
        """
        Get a handler by format name.
        
        Args:
            name: Format name (e.g., 'MSH', 'SKT')
            context: Blender context
            
        Returns:
            FormatHandler instance or None
        """
        handler_class = self._handlers.get(name)
        if handler_class:
            return handler_class(context)
        return None
    
    def get_handler_for_tag(self, tag: Tag,
                             context: Optional[Any] = None) -> Optional[FormatHandler]:
        """
        Get a handler for a specific root tag.
        
        Args:
            tag: Root tag to look up
            context: Blender context
            
        Returns:
            FormatHandler instance or None
        """
        handler_class = self._tag_map.get(tag)
        if handler_class:
            return handler_class(context)
        return None
    
    def get_all_formats(self) -> List[FormatInfo]:
        """Get information about all registered formats."""
        return [cls.get_format_info() for cls in self._handlers.values()]
    
    def get_import_extensions(self) -> str:
        """Get file extension filter string for import dialogs."""
        extensions = set()
        for cls in self._handlers.values():
            info = cls.get_format_info()
            if info.can_import:
                extensions.update(info.extensions)
        
        ext_list = sorted(extensions)
        return ";".join(f"*.{ext}" for ext in ext_list)
    
    def get_export_extensions(self) -> str:
        """Get file extension filter string for export dialogs."""
        extensions = set()
        for cls in self._handlers.values():
            info = cls.get_format_info()
            if info.can_export:
                extensions.update(info.extensions)
        
        ext_list = sorted(extensions)
        return ";".join(f"*.{ext}" for ext in ext_list)
    
    def _detect_from_iff(self, iff: Iff) -> Optional[Type[FormatHandler]]:
        """Detect format from IFF contents."""
        try:
            # Check if we have a FORM
            if not iff.is_current_form():
                tag = iff.get_current_name()
                return self._tag_map.get(tag)
            
            # Enter the root form and check its name
            iff.enter_form()
            form_name = iff.get_current_name()
            iff.exit_form()
            
            # Also check second-level form name for nested formats
            handler = self._tag_map.get(form_name)
            if handler:
                return handler
            
            # Check the outer FORM tag itself
            return self._tag_map.get(Tag('FORM'))
        except IffError:
            return None


# Global registry instance
registry = FormatRegistry()


def register_handler(handler_class: Type[FormatHandler]) -> Type[FormatHandler]:
    """
    Decorator to register a format handler.
    
    Usage:
        @register_handler
        class MshHandler(FormatHandler):
            ...
    """
    registry.register(handler_class)
    return handler_class
