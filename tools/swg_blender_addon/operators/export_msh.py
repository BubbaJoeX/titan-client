# SWG Asset Toolchain - MSH Export Operator
# Copyright (c) Titan Project

import bpy
from ..formats.msh import MshHandler


def execute_export(operator, context):
    """Execute MSH export."""
    filepath = operator.filepath
    
    # Get selected mesh objects
    objects = [obj for obj in context.selected_objects 
               if obj.type == 'MESH' or (obj.type == 'EMPTY' and obj.get('swg_hardpoint'))]
    
    if not objects:
        operator.report({'ERROR'}, "No mesh objects selected")
        return {'CANCELLED'}
    
    handler = MshHandler(context)
    
    options = {
        'export_version': getattr(operator, 'export_version', '0005'),
        'apply_modifiers': getattr(operator, 'apply_modifiers', True),
    }
    
    result = handler.export_file(filepath, objects, **options)
    
    if not result.success:
        operator.report({'ERROR'}, result.message)
        return {'CANCELLED'}
    
    for warning in result.warnings:
        operator.report({'WARNING'}, warning)
    
    operator.report({'INFO'}, result.message)
    return {'FINISHED'}
