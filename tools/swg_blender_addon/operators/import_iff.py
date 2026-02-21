# SWG Asset Toolchain - Generic IFF Import Operator
# Copyright (c) Titan Project

from pathlib import Path
from ..core.iff import Iff
from ..core.registry import registry


def execute_import(operator, context):
    """Execute generic IFF import with auto-detection."""
    filepath = operator.filepath
    
    # Get handler for file
    handler = registry.get_handler_for_file(filepath, context)
    
    if not handler:
        operator.report({'ERROR'}, f"No handler found for file: {filepath}")
        return {'CANCELLED'}
    
    # Import based on mode
    import_mode = getattr(operator, 'import_mode', 'AUTO')
    
    result = handler.import_file(filepath)
    
    if not result.success:
        operator.report({'ERROR'}, result.message)
        return {'CANCELLED'}
    
    # Report warnings
    for warning in result.warnings:
        operator.report({'WARNING'}, warning)
    
    operator.report({'INFO'}, result.message)
    return {'FINISHED'}
