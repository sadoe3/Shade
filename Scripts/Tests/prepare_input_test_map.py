"""Create the local map required by Shade.Player.InputConnectionPIE.

With the Editor closed, run UnrealEditor-Cmd.exe <Shade.uproject>
-run=pythonscript -script=<absolute path to this file> -unattended -nop4.
Then open /Game/Developers/Automation/L_InputConnection and run the test.
The generated map is Git-ignored; existing maps are never overwritten.
"""
import unreal

gm_cls = unreal.load_class(None, '/Game/Shade/Framework/BP_ShadeGameMode.BP_ShadeGameMode_C')
assert gm_cls, 'Build ShadeEditor and restore the input assets first'
level_path='/Game/Developers/Automation/L_InputConnection'
assert not unreal.EditorAssetLibrary.does_asset_exist(level_path), 'Preserve existing verification map'
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.new_level(level_path)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
floor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-50))
floor.set_actor_label('InputTestFloor')
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
floor.set_actor_scale3d(unreal.Vector(100,100,1))
start=actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(0,0,100))
start.set_actor_label('InputTestStart')
light=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,600),unreal.Rotator(-50,-30,0))
actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,500))
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',gm_cls)
assert level.save_current_level()
