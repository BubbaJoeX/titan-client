# SWG Asset Toolchain - SKT Import Operator
# Copyright (c) Titan Project

from ..formats.skt import SktHandler


def execute_import(operator, context):
    """Execute SKT import."""
    filepath = operator.filepath
    
    handler = SktHandler(context)
    
    options = {
        'bone_display_type': getattr(operator, 'bone_display_type', 'OCTAHEDRAL'),
    }
    
    result = handler.import_file(filepath, **options)
    
    if not result.success:
        operator.report({'ERROR'}, result.message)
        return {'CANCELLED'}
    
    for warning in result.warnings:
        operator.report({'WARNING'}, warning)
    
    operator.report({'INFO'}, result.message)
    return {'FINISHED'}
