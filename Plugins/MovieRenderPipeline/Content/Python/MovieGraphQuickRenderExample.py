# Copyright Epic Games, Inc. All Rights Reserved.
import unreal

# ----------------------------------------------------------------------------------------------------------------------------------------------------
# This example shows how to do a Quick Render of a specific level sequence, using some aspects of the viewport's look, and a custom frame range.
# However, there are many modes and options available for use, so it's worth browsing the help docs. You can view them by running `help` commands in
# the Python console (*not* the Cmd console) within the editor. For example:
# `help(unreal.MovieGraphQuickRenderModeSettings)` or `help(unreal.MovieGraphQuickRenderMode)`
#
# To run this example script in the editor, in the Cmd console, run:
# py "MovieGraphQuickRenderExample.py"
# ----------------------------------------------------------------------------------------------------------------------------------------------------

asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()

# This example uses a sequence-based render, but other modes can be used as well (like doing a viewport render).
quick_render_mode = unreal.MovieGraphQuickRenderMode.CURRENT_SEQUENCE

# Create the settings used to initialize Quick Render. Note that multiple viewport look flags can be OR'd together. This is also being set up to show
# the render immediately after completion, but this can be disabled by specifying DO_NOTHING instead.
quick_render_mode_settings = unreal.MovieGraphQuickRenderModeSettings()
quick_render_mode_settings.post_render_behavior = unreal.MovieGraphQuickRenderPostRenderActionType.PLAY_RENDER_OUTPUT
quick_render_mode_settings.override_viewport_look_flags = True
quick_render_mode_settings.viewport_look_flags = unreal.MovieGraphQuickRenderViewportLookFlags.VIEW_MODE.value | unreal.MovieGraphQuickRenderViewportLookFlags.SHOW_FLAGS.value

# For sequence-based quick renders, a sequence override can optionally be used instead of using the current level sequence that's active in
# Sequencer (which is what will be used if an override is not specified).
sequence_override = asset_registry.get_asset_by_object_path("/Game/TestQuickRenderSequence.TestQuickRenderSequence")
quick_render_mode_settings.level_sequence_override = unreal.SystemLibrary.conv_soft_obj_path_to_soft_obj_ref(sequence_override.to_soft_object_path())

# Since a sequence is being used, an optional custom frame range within the sequence can be specified. Note that the start frame is inclusive, and the
# end frame is exclusive.
quick_render_mode_settings.frame_range_type = unreal.MovieGraphQuickRenderFrameRangeType.CUSTOM
quick_render_mode_settings.custom_start_frame = 100
quick_render_mode_settings.custom_end_frame = 105

# Assign a non-default graph preset. This can be skipped if using the default Quick Render graph is OK.
graph_preset_asset_data = asset_registry.get_asset_by_object_path("/Game/TestQuickRenderGraph.TestQuickRenderGraph")
graph_preset = unreal.SystemLibrary.conv_soft_obj_path_to_soft_obj_ref(graph_preset_asset_data.to_soft_object_path())
quick_render_mode_settings.graph_preset = graph_preset

# Since a custom graph is being used, refresh_variable_assignments() needs to be called in order to update the variable assignments to reflect the
# graph preset change. This must be done after setting `graph_preset`.
unreal.MovieGraphQuickRenderModeSettings.refresh_variable_assignments(quick_render_mode_settings)

# Update one of the graph variables to use a new value for this render. Any variables in your graph can be updated at this point.
num_warmup_frames_var = graph_preset_asset_data.get_asset().get_variable_by_name("NumWarmUpFrames")
graph_variable_assignments = quick_render_mode_settings.get_variable_assignments_for_graph(graph_preset)
graph_variable_assignments.set_value_int32(num_warmup_frames_var, 2)

# Do the render. The media that's generated will be saved in the location specified by the Global Output Settings node within the graph.
quick_render_subsystem = unreal.get_editor_subsystem(unreal.MovieGraphQuickRenderSubsystem)
quick_render_subsystem.begin_quick_render(quick_render_mode, quick_render_mode_settings)