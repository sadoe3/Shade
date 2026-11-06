"""Read-only asset verification for a separate clean Editor process.

Run only after the temporary authoring source has been removed and ShadeEditor
has been rebuilt. This script loads saved assets; it does not compile/save them.
It writes only Saved/PendingWorks/EnemyVisual/ReloadResult.json.
"""
import json
from pathlib import Path
import unreal

root = "/Game/Shade"
mesh_path = "/Game/ParagonMinions/Characters/Minions/Down_Minions/Meshes/Minion_Lane_Melee_Dawn.Minion_Lane_Melee_Dawn"
skeleton_path = "/Game/ParagonMinions/Characters/Minions/Down_Minions/Meshes/Minion_Lane_Core_Skeleton.Minion_Lane_Core_Skeleton"
bp = unreal.load_asset(root + "/Enemy/BP_ShadeEnemy")
abp = unreal.load_asset(root + "/Enemy/ABP_ShadeEnemy")
blend = unreal.load_asset(root + "/Enemy/BS_ShadeEnemyLocomotion")
data = unreal.load_asset(root + "/Data/DA_EnemyInitialAttributes")
behavior = unreal.load_asset(root + "/Data/DA_EnemyBehavior")
assert bp and abp and blend and data and behavior, "All five own Enemy assets must survive reload"
cdo = unreal.get_default_object(bp.generated_class())
mesh = cdo.get_editor_property("mesh")
assert mesh.get_editor_property("skeletal_mesh_asset").get_path_name() == mesh_path
assert abp.get_editor_property("target_skeleton").get_path_name() == skeleton_path
assert blend.get_editor_property("skeleton").get_path_name() == skeleton_path
assert mesh.get_editor_property("anim_class") == abp.generated_class()
native_animation_base = unreal.load_class(None, "/Script/Shade.ShadeEnemyAnimInstance")
assert native_animation_base and unreal.MathLibrary.class_is_child_of(abp.generated_class(), native_animation_base)
assert cdo.get_editor_property("initial_attributes_data") == data
assert cdo.get_editor_property("behavior_data") == behavior
assert abs(behavior.get_editor_property("max_alertness") - 100.0) < 0.001
assert abs(data.get_editor_property("initial_health") - 100.0) < 0.001
assert abs(data.get_editor_property("walk_speed") - 180.0) < 0.001
assert abs(data.get_editor_property("run_speed") - 300.0) < 0.001
assert abs(data.get_editor_property("braking_deceleration_walking") - 1800.0) < 0.001
cap = cdo.get_editor_property("capsule_component")
offset = mesh.get_editor_property("relative_location")
rotation = mesh.get_editor_property("relative_rotation")
assert abs(offset.z + cap.get_editor_property("capsule_half_height")) < 0.001
assert abs(rotation.yaw + 90.0) < 0.001
report = {
    "verified": True,
    "character_class": bp.generated_class().get_path_name(),
    "animation_class": abp.generated_class().get_path_name(),
    "native_animation_base": native_animation_base.get_path_name(),
    "mesh": mesh_path,
    "skeleton": skeleton_path,
    "blend_space": blend.get_path_name(),
    "initial_attributes": data.get_path_name(),
    "behavior_settings": behavior.get_path_name(),
    "scope": "Clean-process serialized asset reload and reference verification; pose rendering/PIE are separate checks",
}
output = Path(unreal.Paths.project_saved_dir()) / "PendingWorks" / "EnemyVisual" / "ReloadResult.json"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("SHADE_ENEMY_VISUAL_RELOAD " + json.dumps(report))
