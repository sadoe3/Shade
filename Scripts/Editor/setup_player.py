"""Create the first Player assets; preserve imported Paragon source assets.
Run in the Shade Editor with -ExecutePythonScript=<absolute path>.
Re-running updates these project-owned Blueprint defaults but does not rebuild an existing map.
"""
import json
import os
import unreal

BASE = "/Game/Shade"
MAP = BASE + "/Maps/L_PlayerPrototype"
assets = unreal.AssetToolsHelpers.get_asset_tools()
editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def blueprint(name, parent):
    path = BASE + "/Player/" + name
    if editor_assets.does_asset_exist(path):
        return editor_assets.load_asset(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    result = assets.create_asset(name, BASE + "/Player", unreal.Blueprint, factory)
    assert result, path
    return result


player = blueprint("BP_ShadePlayer", unreal.ShadePlayerCharacter)
player_default = unreal.get_default_object(player.generated_class())
mesh = unreal.load_asset("/Game/ParagonGideon/Characters/Heroes/Gideon/Meshes/Gideon")
anim = unreal.load_asset("/Game/Shade/Player/ABP_ShadeGideon")
assert mesh and anim, "Import ParagonGideon before running Player setup"
body = player_default.get_editor_property("mesh")
body.set_skeletal_mesh_asset(mesh)
body.set_relative_location(unreal.Vector(0, 0, -96), False, False)
body.set_relative_rotation(unreal.Rotator(pitch=0, yaw=-90, roll=0), False, False)
body.set_anim_instance_class(anim.generated_class())
body.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
unreal.BlueprintEditorLibrary.compile_blueprint(player)
assert editor_assets.save_loaded_asset(player, False)

mode = blueprint("BP_ShadeGameMode", unreal.ShadeGameMode)
unreal.get_default_object(mode.generated_class()).set_editor_property("default_pawn_class", player.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(mode)
assert editor_assets.save_loaded_asset(mode, False)

if not editor_assets.does_asset_exist(MAP):
    assert levels.new_level(MAP)
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    def box(label, location, scale):
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location))
        actor.set_actor_label(label)
        actor.static_mesh_component.set_static_mesh(cube)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        return actor
    box("Prototype_Floor", (0, 0, -25), (80, 80, 0.5))
    box("Prototype_Wall", (600, 650, 150), (9, 0.5, 3))
    box("Prototype_Cube", (-600, 350, 75), (1.5, 1.5, 1.5))
    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 110))
    start.set_actor_label("PlayerStart")
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(pitch=-45, yaw=-30, roll=0))
    sun.set_actor_label("Prototype_Sun")
    sun.light_component.set_editor_property("intensity", 6.0)
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 400))
    sky.light_component.set_editor_property("real_time_capture", True)
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
else:
    assert levels.load_level(MAP)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", mode.generated_class())
for actor in actors.get_all_level_actors():
    if actor.get_actor_label() == "Prototype_Sun":
        actor.set_actor_rotation(unreal.Rotator(pitch=-45, yaw=-30, roll=0), False)
assert levels.save_current_level()
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
# Reload establishes that the saved map and game mode references survive a load.
assert levels.load_level(MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_world_settings().get_editor_property("default_game_mode") == mode.generated_class()
report = {"map": MAP, "player": player.get_path_name(), "mode": mode.get_path_name(), "mesh": mesh.get_path_name(), "animation": anim.get_path_name(), "saved_and_reloaded": True}
output = os.path.join(unreal.Paths.project_saved_dir(), "PlayerSetup.json")
with open(output, "w", encoding="utf-8") as file:
    json.dump(report, file, indent=2)
unreal.log("SHADE_PLAYER_SETUP_OK " + json.dumps(report))
