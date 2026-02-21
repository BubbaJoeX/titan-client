# SWG Asset Toolchain - MSH Import Operator
# Copyright (c) Titan Project

from ..formats.msh import MshHandler


def execute_import(operator, context):
    """Execute MSH import."""
    filepath = operator.filepath
    
    handler = MshHandler(context)
    
    options = {
        'import_materials': getattr(operator, 'import_materials', True),
        'import_normals': getattr(operator, 'import_normals', True),
    }
    
    result = handler.import_file(filepath, **options)
    
    if not result.success:
        operator.report({'ERROR'}, result.message)
        return {'CANCELLED'}
    
    for warning in result.warnings:
        operator.report({'WARNING'}, warning)
    
    operator.report({'INFO'}, result.message)
    return {'FINISHED'}
