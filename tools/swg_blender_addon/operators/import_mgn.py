# SWG Asset Toolchain - MGN Import Operator
# Copyright (c) Titan Project

from ..formats.mgn import MgnHandler


def execute_import(operator, context):
    """Execute MGN import."""
    filepath = operator.filepath
    
    handler = MgnHandler(context)
    
    options = {
        'import_skeleton': getattr(operator, 'import_skeleton', True),
        'import_blend_shapes': getattr(operator, 'import_blend_shapes', True),
        'import_materials': True,
    }
    
    result = handler.import_file(filepath, **options)
    
    if not result.success:
        operator.report({'ERROR'}, result.message)
        return {'CANCELLED'}
    
    for warning in result.warnings:
        operator.report({'WARNING'}, warning)
    
    operator.report({'INFO'}, result.message)
    return {'FINISHED'}
