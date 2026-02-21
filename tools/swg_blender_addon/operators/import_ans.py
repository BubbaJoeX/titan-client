# SWG Asset Toolchain - ANS Import Operator
# Copyright (c) Titan Project

from ..formats.ans import AnsHandler


def execute_import(operator, context):
    """Execute ANS import."""
    filepath = operator.filepath
    
    handler = AnsHandler(context)
    
    options = {
        'target_armature': getattr(operator, 'target_armature', ''),
    }
    
    result = handler.import_file(filepath, **options)
    
    if not result.success:
        operator.report({'ERROR'}, result.message)
        return {'CANCELLED'}
    
    for warning in result.warnings:
        operator.report({'WARNING'}, warning)
    
    operator.report({'INFO'}, result.message)
    return {'FINISHED'}
