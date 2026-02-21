# SWG Asset Toolchain - SKT Export Operator
# Copyright (c) Titan Project

import bpy
from ..formats.skt import SktHandler


def execute_export(operator, context):
    """Execute SKT export."""
    filepath = operator.filepath
    
    # Get selected armature
    objects = [obj for obj in context.selected_objects if obj.type == 'ARMATURE']
    
    if not objects:
        operator.report({'ERROR'}, "No armature selected")
        return {'CANCELLED'}
    
    handler = SktHandler(context)
    
    options = {
        'export_version': getattr(operator, 'export_version', '0002'),
    }
    
    result = handler.export_file(filepath, objects, **options)
    
    if not result.success:
        operator.report({'ERROR'}, result.message)
        return {'CANCELLED'}
    
    for warning in result.warnings:
        operator.report({'WARNING'}, warning)
    
    operator.report({'INFO'}, result.message)
    return {'FINISHED'}
