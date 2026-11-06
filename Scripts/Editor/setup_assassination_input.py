"""Author the E action and assassination settings; keep existing input mappings.
Run with UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file>.
Add -ShadeVerifyOnly to reload and verify without saving.
"""
import unreal
verify = '-ShadeVerifyOnly' in unreal.SystemLibrary.get_command_line()
tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
action_path = '/Game/Shade/Input/IA_Assassinate'
data_path = '/Game/Shade/Data/DA_Assassination'
mapping = unreal.load_asset('/Game/Shade/Input/IMC_Player')
controller = unreal.load_asset('/Game/Shade/Player/BP_ShadePlayerController')
state = unreal.load_asset('/Game/Shade/Player/BP_ShadePlayerState')
assert mapping and controller and state

def load_or_create(path, cls):
    asset = unreal.load_asset(path) if library.does_asset_exist(path) else None
    if asset:
        assert isinstance(asset, cls)
        return asset
    assert not verify, path
    folder, name = path.rsplit('/',1)
    factory=unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class',cls)
    result=tools.create_asset(name,folder,cls,factory)
    assert result
    return result

action=load_or_create(action_path,unreal.InputAction)
data=load_or_create(data_path,unreal.ShadeAssassinationData)
if not verify:
    action.set_editor_property('value_type',unreal.InputActionValueType.BOOLEAN)
    data.set_editor_property('range',220.0)
    defaults=mapping.get_editor_property('default_key_mappings')
    entries=list(defaults.get_editor_property('mappings'))
    existing=[e for e in entries if str(e.get_editor_property('key').get_editor_property('key_name'))=='E']
    if existing:
        assert len(existing)==1 and existing[0].get_editor_property('action')==action, 'Preserve existing E binding'
    else:
        entry=unreal.EnhancedActionKeyMapping()
        entry.set_editor_property('action',action)
        key=unreal.Key();key.set_editor_property('key_name','E')
        entry.set_editor_property('key',key)
        entries.append(entry)
        defaults.set_editor_property('mappings',entries)
        mapping.set_editor_property('default_key_mappings',defaults)
    unreal.BlueprintEditorLibrary.compile_blueprint(controller)
    unreal.BlueprintEditorLibrary.compile_blueprint(state)
    unreal.get_default_object(controller.generated_class()).set_editor_property('assassinate_action',action)
    unreal.get_default_object(state.generated_class()).set_editor_property('assassination_data',data)
    for asset in (action,data,mapping,controller,state):
        assert library.save_loaded_asset(asset,False),asset.get_path_name()

cdo=unreal.get_default_object(controller.generated_class())
assert cdo.get_editor_property('assassinate_action')==action
assert cdo.get_editor_property('run_action').get_name()=='IA_Run'
assert cdo.get_editor_property('move_action').get_name()=='IA_Move'
assert cdo.get_editor_property('look_action').get_name()=='IA_Look'
assert unreal.get_default_object(state.generated_class()).get_editor_property('assassination_data')==data
assert data.get_editor_property('range')==220.0
assert action.get_editor_property('value_type')==unreal.InputActionValueType.BOOLEAN
entries=list(mapping.get_editor_property('default_key_mappings').get_editor_property('mappings'))
assert len(entries)==7
assert {str(e.get_editor_property('key').get_editor_property('key_name')) for e in entries}=={'W','A','S','D','Mouse2D','LeftShift','E'}
for e in entries:
    if str(e.get_editor_property('key').get_editor_property('key_name'))=='Mouse2D':
        mods=e.get_editor_property('modifiers')
        scalars=[m for m in mods if isinstance(m,unreal.InputModifierScalar)]
        assert len(scalars)==1
        scalar=scalars[0].get_editor_property('scalar')
        assert abs(scalar.x-0.3)<0.0001 and abs(scalar.y-0.3)<0.0001
unreal.log('SHADE_ASSASSINATION_INPUT_VERIFIED ' + ('reload' if verify else 'saved'))
