# Copyright Your Company, Inc. All Rights Reserved.
import unreal
import json
import sys

# --- Globals to prevent garbage collection ---
# We store references to the subsystem and executor at the global level
# to ensure they persist for the duration of the render process.
G_SUBSYSTEM = None
G_EXECUTOR = None
G_PIPELINE_QUEUE = None


def on_render_finished(executor, success):
    """
    Callback function executed when the entire render queue is finished.
    Cleans up global references and exits the engine.
    """
    global G_SUBSYSTEM, G_EXECUTOR, G_PIPELINE_QUEUE

    if success:
        unreal.log("Render farm job completed successfully.")
    else:
        unreal.log_error("Render farm job failed. Check logs for details.")

    # Clean up global references to allow garbage collection
    del G_SUBSYSTEM
    del G_EXECUTOR
    del G_PIPELINE_QUEUE

    # Exit the engine process once rendering is complete
    unreal.SystemLibrary.quit_editor()


def apply_scene_settings(settings_dict):
    """
    Finds the BP_SceneSettings actor in the level and applies the
    provided settings by calling its functions.
    """
    unreal.log("Applying scene settings...")

    # Find the settings actor in the current level
    # Note: This finds the first actor of this class. For more complex setups,
    # you might need a more specific way to identify the actor (e.g., by tag).
    actor_system = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_system.get_all_level_actors()
    scene_settings_actor = None
    for actor in actors:
        if actor.get_class().get_name() == 'BP_SceneSettings_C':
            scene_settings_actor = actor
            break

    if not scene_settings_actor:
        unreal.log_error("Could not find 'BP_SceneSettings' actor in the level. Skipping scene setup.")
        return

    # Call functions on the actor based on the settings dictionary
    if "time_of_day" in settings_dict:
        # Assuming the actor has a function named 'SetTimeOfDay'
        if hasattr(scene_settings_actor, 'set_time_of_day'):
            time = settings_dict["time_of_day"]
            scene_settings_actor.set_time_of_day(time)
            unreal.log(f"Set time_of_day to: {time}")
        else:
            unreal.log_warning("'BP_SceneSettings' actor does not have a 'set_time_of_day' function.")

    if "cloud_coverage" in settings_dict:
        # Assuming the actor has a function named 'SetCloudCoverage'
        if hasattr(scene_settings_actor, 'set_cloud_coverage'):
            coverage = settings_dict["cloud_coverage"]
            scene_settings_actor.set_cloud_coverage(coverage)
            unreal.log(f"Set cloud_coverage to: {coverage}")
        else:
            unreal.log_warning("'BP_SceneSettings' actor does not have a 'set_cloud_coverage' function.")


def start_render(job_data, graph_path):
    """
    Main function to configure and execute the render job.
    """
    global G_SUBSYSTEM, G_EXECUTOR, G_PIPELINE_QUEUE

    G_SUBSYSTEM = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
    G_PIPELINE_QUEUE = G_SUBSYSTEM.get_queue()

    # Clear any existing jobs in the queue to ensure a clean slate
    G_PIPELINE_QUEUE.delete_all_jobs()

    # Create a new job for our render
    job = G_PIPELINE_QUEUE.allocate_new_job(unreal.MoviePipelineExecutorJob)

    # --- Load and Assign Graph Preset ---
    graph_preset = unreal.load_asset(graph_path, unreal.MovieGraphConfig)
    if not graph_preset:
        unreal.log_error(f"Failed to load MovieGraphConfig asset at path: {graph_path}")
        unreal.SystemLibrary.quit_editor()
        return

    job.set_graph_preset(graph_preset)
    unreal.log(f"Assigned Graph Preset: {graph_path}")

    # --- Set Job-Specific Parameters ---
    job.job_name = job_data.get("job_id", "DefaultJobName")
    job.sequence = unreal.SoftObjectPath(job_data["sequence_path"])
    job.map = unreal.SoftObjectPath(job_data["level_path"])

    # --- Apply Scene Settings ---
    if "scene_settings" in job_data:
        apply_scene_settings(job_data["scene_settings"])

    # --- Set Exposed Graph Variables ---
    variable_overrides = job.get_or_create_variable_overrides(graph_preset)

    # 1. Set Output Path
    output_path_var = graph_preset.get_variable_by_name("OutputDirectory")
    if output_path_var:

        output_path = f"(Path=\"{job_data['output_path']}\")"
        variable_overrides.set_variable_assignment_enable_state(output_path_var, True)
        variable_overrides.set_value_serialized_string(output_path_var, output_path)
        unreal.log(f"Overrode 'Output path' to: {job_data['output_path']}")
    else:
        unreal.log_warning("Could not find exposed variable 'Output path' in the graph.")

    # 2. Set Resolution
    resolution_var = graph_preset.get_variable_by_name("Resolution")
    if resolution_var:
        res_x, res_y = job_data["resolution"]
        # IntPoint needs to be serialized to a string in the format '(X=...,Y=...)'
        # Movie Graph Named Resolution needs to be serialized to the format '(ProfileName="Custom",Resolution=(X=X,Y=1080),Description="")'
        resolution_string = f'(ProfileName="Custom",Resolution=(X={res_x},Y={res_y}),Description="")'

        variable_overrides.set_variable_assignment_enable_state(resolution_var, True)
        variable_overrides.set_value_serialized_string(resolution_var, resolution_string)
        unreal.log(f"Overrode 'Resolution' to: {resolution_string}")
    else:
        unreal.log_warning("Could not find exposed variable 'Resolution' in the graph.")

    # --- Execute Render ---
    G_EXECUTOR = unreal.MoviePipelinePIEExecutor(G_SUBSYSTEM)
    G_EXECUTOR.on_executor_finished_delegate.add_callable_unique(on_render_finished)

    unreal.log("Starting render...")
    G_SUBSYSTEM.render_queue_with_executor_instance(G_EXECUTOR)


if __name__ == "__main__":
    unreal.log("Starting Render Automation Script...")

    # Parse command line arguments to get job and graph paths
    cmd_tokens, cmd_switches, cmd_parameters = unreal.SystemLibrary.parse_command_line(sys.argv[-1])

    job_path = cmd_parameters.get("JobPath", None)
    graph_path = cmd_parameters.get("GraphPath", None)

    if not job_path or not graph_path:
        unreal.log_error("Missing -JobPath or -GraphPath command line arguments. Exiting.")
        unreal.SystemLibrary.quit_editor()
    else:
        try:
            with open(job_path, 'r') as f:
                job_definition = json.load(f)

            # Kick off the render process with the loaded data
            start_render(job_definition, graph_path)

        except FileNotFoundError:
            unreal.log_error(f"Job JSON file not found at path: {job_path}")
            unreal.SystemLibrary.quit_editor()
        except Exception as e:
            unreal.log_error(f"An error occurred: {e}")
            unreal.SystemLibrary.quit_editor()