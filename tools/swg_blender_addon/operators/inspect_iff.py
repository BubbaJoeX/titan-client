# SWG Asset Toolchain - IFF Inspection Operator
# Copyright (c) Titan Project

from pathlib import Path
from ..core.iff import Iff


def execute_inspect(operator, context):
    """Execute IFF inspection."""
    filepath = operator.filepath
    
    try:
        iff = Iff.from_file(filepath)
        tree = iff.parse_tree()
        
        # Build inspection report
        report_lines = [
            f"=== IFF Inspection: {Path(filepath).name} ===",
            f"Size: {iff.raw_data_size} bytes",
            "",
            "Structure:",
        ]
        
        def print_block(block, indent=0):
            prefix = "  " * indent
            type_str = "FORM" if block.is_form() else "CHUNK"
            report_lines.append(f"{prefix}[{type_str}] {block.tag} ({block.length} bytes)")
            
            for child in block.children:
                print_block(child, indent + 1)
        
        for child in tree.children:
            print_block(child)
        
        # Print to console
        for line in report_lines:
            print(line)
        
        operator.report({'INFO'}, f"Inspected {Path(filepath).name} - see console for details")
        return {'FINISHED'}
        
    except Exception as e:
        operator.report({'ERROR'}, f"Failed to inspect: {str(e)}")
        return {'CANCELLED'}
