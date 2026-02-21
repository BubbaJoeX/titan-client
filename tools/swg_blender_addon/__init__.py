# SWG Asset Toolchain for Blender
bl_info = {
    'name': 'SWG Asset Toolchain',
    'author': 'SWG: Titan',
    'version': (1, 0, 0),
    'blender': (4, 5, 6),
    'location': 'File > Import/Export, View3D > Sidebar > SWG',
    'description': 'Complete toolchain for Star Wars Galaxies asset formats',
    'category': 'Import-Export',
}

import bpy
from bpy.props import StringProperty, BoolProperty, EnumProperty
from bpy.types import Operator, Panel, AddonPreferences
from pathlib import Path
import traceback


class SWGAddonPreferences(AddonPreferences):
    bl_idname = __name__
    swg_data_path: StringProperty(name='SWG Data Path', default='', subtype='DIR_PATH')
    debug_mode: BoolProperty(name='Debug Mode', default=False)
    def draw(self, context):
        layout = self.layout
        layout.prop(self, 'swg_data_path')
        layout.prop(self, 'debug_mode')


class SWG_OT_import_msh(Operator):
    '''Import SWG Static Mesh'''
    bl_idname = 'import_mesh.swg_msh'
    bl_label = 'Import SWG Mesh (.msh)'
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}
    filepath: StringProperty(subtype='FILE_PATH')
    filter_glob: StringProperty(default='*.msh', options={'HIDDEN'})
    import_materials: BoolProperty(name='Import Materials', default=True)
    import_normals: BoolProperty(name='Import Normals', default=True)
    def execute(self, context):
        try:
            from .formats.msh import MshHandler
            handler = MshHandler(context)
            result = handler.import_file(self.filepath, import_materials=self.import_materials, import_normals=self.import_normals)
            if result.success:
                self.report({'INFO'}, result.message)
                return {'FINISHED'}
            else:
                self.report({'ERROR'}, result.message)
                return {'CANCELLED'}
        except Exception as e:
            self.report({'ERROR'}, str(e))
            traceback.print_exc()
            return {'CANCELLED'}
    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class SWG_OT_import_mgn(Operator):
    '''Import SWG Mesh Generator'''
    bl_idname = 'import_mesh.swg_mgn'
    bl_label = 'Import SWG Mesh Generator (.mgn)'
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}
    filepath: StringProperty(subtype='FILE_PATH')
    filter_glob: StringProperty(default='*.mgn', options={'HIDDEN'})
    import_blend_shapes: BoolProperty(name='Import Blend Shapes', default=True)
    def execute(self, context):
        try:
            from .formats.mgn import MgnHandler
            handler = MgnHandler(context)
            result = handler.import_file(self.filepath, import_blend_shapes=self.import_blend_shapes)
            if result.success:
                self.report({'INFO'}, result.message)
                return {'FINISHED'}
            else:
                self.report({'ERROR'}, result.message)
                return {'CANCELLED'}
        except Exception as e:
            self.report({'ERROR'}, str(e))
            traceback.print_exc()
            return {'CANCELLED'}
    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class SWG_OT_import_skt(Operator):
    '''Import SWG Skeleton'''
    bl_idname = 'import_armature.swg_skt'
    bl_label = 'Import SWG Skeleton (.skt)'
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}
    filepath: StringProperty(subtype='FILE_PATH')
    filter_glob: StringProperty(default='*.skt', options={'HIDDEN'})
    def execute(self, context):
        try:
            from .formats.skt import SktHandler
            handler = SktHandler(context)
            result = handler.import_file(self.filepath)
            if result.success:
                self.report({'INFO'}, result.message)
                return {'FINISHED'}
            else:
                self.report({'ERROR'}, result.message)
                return {'CANCELLED'}
        except Exception as e:
            self.report({'ERROR'}, str(e))
            traceback.print_exc()
            return {'CANCELLED'}
    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class SWG_OT_import_ans(Operator):
    '''Import SWG Animation'''
    bl_idname = 'import_anim.swg_ans'
    bl_label = 'Import SWG Animation (.ans)'
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}
    filepath: StringProperty(subtype='FILE_PATH')
    filter_glob: StringProperty(default='*.ans', options={'HIDDEN'})
    def execute(self, context):
        try:
            from .formats.ans import AnsHandler
            handler = AnsHandler(context)
            result = handler.import_file(self.filepath)
            if result.success:
                self.report({'INFO'}, result.message)
                return {'FINISHED'}
            else:
                self.report({'ERROR'}, result.message)
                return {'CANCELLED'}
        except Exception as e:
            self.report({'ERROR'}, str(e))
            traceback.print_exc()
            return {'CANCELLED'}
    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class SWG_OT_export_msh(Operator):
    '''Export SWG Static Mesh'''
    bl_idname = 'export_mesh.swg_msh'
    bl_label = 'Export SWG Mesh (.msh)'
    bl_options = {'REGISTER', 'PRESET'}
    filepath: StringProperty(subtype='FILE_PATH')
    filter_glob: StringProperty(default='*.msh', options={'HIDDEN'})
    filename_ext = '.msh'
    apply_modifiers: BoolProperty(name='Apply Modifiers', default=True)
    def execute(self, context):
        try:
            from .formats.msh import MshHandler
            objects = [obj for obj in context.selected_objects if obj.type == 'MESH']
            if not objects:
                self.report({'ERROR'}, 'No mesh objects selected')
                return {'CANCELLED'}
            handler = MshHandler(context)
            result = handler.export_file(self.filepath, objects, apply_modifiers=self.apply_modifiers)
            if result.success:
                self.report({'INFO'}, result.message)
                return {'FINISHED'}
            else:
                self.report({'ERROR'}, result.message)
                return {'CANCELLED'}
        except Exception as e:
            self.report({'ERROR'}, str(e))
            traceback.print_exc()
            return {'CANCELLED'}
    def invoke(self, context, event):
        if not self.filepath:
            self.filepath = 'untitled.msh'
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class SWG_OT_export_skt(Operator):
    '''Export SWG Skeleton'''
    bl_idname = 'export_armature.swg_skt'
    bl_label = 'Export SWG Skeleton (.skt)'
    bl_options = {'REGISTER', 'PRESET'}
    filepath: StringProperty(subtype='FILE_PATH')
    filter_glob: StringProperty(default='*.skt', options={'HIDDEN'})
    filename_ext = '.skt'
    def execute(self, context):
        try:
            from .formats.skt import SktHandler
            objects = [obj for obj in context.selected_objects if obj.type == 'ARMATURE']
            if not objects:
                self.report({'ERROR'}, 'No armature selected')
                return {'CANCELLED'}
            handler = SktHandler(context)
            result = handler.export_file(self.filepath, objects)
            if result.success:
                self.report({'INFO'}, result.message)
                return {'FINISHED'}
            else:
                self.report({'ERROR'}, result.message)
                return {'CANCELLED'}
        except Exception as e:
            self.report({'ERROR'}, str(e))
            traceback.print_exc()
            return {'CANCELLED'}
    def invoke(self, context, event):
        if not self.filepath:
            self.filepath = 'untitled.skt'
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class SWG_OT_inspect_iff(Operator):
    '''Inspect IFF file structure'''
    bl_idname = 'swg.inspect_iff'
    bl_label = 'Inspect IFF'
    bl_options = {'REGISTER'}
    filepath: StringProperty(subtype='FILE_PATH')
    filter_glob: StringProperty(default='*.*', options={'HIDDEN'})
    def execute(self, context):
        try:
            from .core.iff import Iff
            iff = Iff.from_file(self.filepath)
            tree = iff.parse_tree()
            name = Path(self.filepath).name
            size = iff.raw_data_size
            print('IFF: ' + name + ' (' + str(size) + ' bytes)')
            def print_block(block, indent):
                t = 'FORM' if block.is_form() else 'CHUNK'
                spaces = '  ' * indent
                print(spaces + '[' + t + '] ' + str(block.tag) + ' (' + str(block.length) + ')')
                for child in block.children:
                    print_block(child, indent + 1)
            for child in tree.children:
                print_block(child, 0)
            self.report({'INFO'}, 'Inspected - see console')
            return {'FINISHED'}
        except Exception as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class SWG_PT_main_panel(Panel):
    '''SWG Tools Panel'''
    bl_label = 'SWG Tools'
    bl_idname = 'SWG_PT_main_panel'
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'SWG'
    def draw(self, context):
        layout = self.layout
        box = layout.box()
        box.label(text='Import', icon='IMPORT')
        col = box.column(align=True)
        col.operator('import_mesh.swg_msh', text='Mesh (.msh)')
        col.operator('import_mesh.swg_mgn', text='Mesh Gen (.mgn)')
        col.operator('import_armature.swg_skt', text='Skeleton (.skt)')
        col.operator('import_anim.swg_ans', text='Animation (.ans)')
        box = layout.box()
        box.label(text='Export', icon='EXPORT')
        col = box.column(align=True)
        col.operator('export_mesh.swg_msh', text='Mesh (.msh)')
        col.operator('export_armature.swg_skt', text='Skeleton (.skt)')
        box = layout.box()
        box.label(text='Tools', icon='TOOL_SETTINGS')
        box.operator('swg.inspect_iff', text='Inspect IFF')


def menu_func_import(self, context):
    self.layout.separator()
    self.layout.operator(SWG_OT_import_msh.bl_idname, text='SWG Mesh (.msh)')
    self.layout.operator(SWG_OT_import_mgn.bl_idname, text='SWG Mesh Generator (.mgn)')
    self.layout.operator(SWG_OT_import_skt.bl_idname, text='SWG Skeleton (.skt)')
    self.layout.operator(SWG_OT_import_ans.bl_idname, text='SWG Animation (.ans)')


def menu_func_export(self, context):
    self.layout.separator()
    self.layout.operator(SWG_OT_export_msh.bl_idname, text='SWG Mesh (.msh)')
    self.layout.operator(SWG_OT_export_skt.bl_idname, text='SWG Skeleton (.skt)')


classes = (
    SWGAddonPreferences,
    SWG_OT_import_msh,
    SWG_OT_import_mgn,
    SWG_OT_import_skt,
    SWG_OT_import_ans,
    SWG_OT_export_msh,
    SWG_OT_export_skt,
    SWG_OT_inspect_iff,
    SWG_PT_main_panel,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)
    print('SWG Asset Toolchain registered')


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == '__main__':
    register()
