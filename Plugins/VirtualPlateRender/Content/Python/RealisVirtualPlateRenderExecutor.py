# Copyright Epic Games, Inc. All Rights Reserved.
# Modified for the Realis Render Farm System.
import unreal
import json

@unreal.uclass()
class RealisVirtualPlateRenderExecutor(unreal.MoviePipelinePythonHostExecutor):
    """
    Custom Movie Pipeline Executor for the Realis render farm.
    This class is instantiated by the engine when launched with the correct
    command-line arguments. It's responsible for parsing a job description
    file and executing a single render job.
    """

    def _post_init(self):
        """Constructor for the executor."""
        self.active_movie_pipeline = None
        unreal.log("RealisVirtualPlateRenderExecutor: Initialized.")

    @unreal.ufunction(override=True)
    def execute_delayed(self, in_pipeline_queue):
        """
        This is the main entry point for the executor, called by the engine
        after the specified map has finished loading.
        """
        unreal.log("RealisVirtualPlateRenderExecutor: execute_delayed started.")

        # --- Parse Command Line ---
        (_, _, cmd_parameters) = unreal.SystemLibrary.parse_command_line(unreal.SystemLibrary.get_command_line())
        job_path = cmd_parameters.get('JobPath')
        graph_path = cmd_parameters.get('GraphPath')

        if not job_path or not graph_path:
            unreal.log_error("RealisVirtualPlateRenderExecutor: Missing -JobPath or -GraphPath arguments. Shutting down.")
            self.on_executor_errored(None, True, "Missing command line arguments.")
            return

        # --- Load Job Definition ---
        try:
            with open(job_path, 'r') as f:
                job_data = json.load(f)
        except Exception as e:
            unreal.log_error(f"RealisVirtualPlateRenderExecutor: Failed to load or parse job file at {job_path}. Error: {e}")
            self.on_executor_errored(None, True, "Failed to load job file.")
            return

        # --- Build and Configure the Job ---
        # We create our own queue to ensure it's clean and owned by this executor.
        self.pipeline_queue = unreal.new_object(unreal.MoviePipelineQueue, outer=self)
        job = self.pipeline_queue.allocate_new_job(unreal.MoviePipelineExecutorJob)

        job.job_name = job_data.get("job_id", "Default_Render_Job")
        job.sequence = unreal.SoftObjectPath(job_data["sequence_path"])
        job.map = unreal.SoftObjectPath(job_data["level_path"])

        graph_preset = unreal.load_asset(graph_path)
        if not isinstance(graph_preset, unreal.MovieGraphConfig):
            unreal.log_error(f"RealisVirtualPlateRenderExecutor: Asset at {graph_path} is not a valid MovieGraphConfig.")
            self.on_executor_errored(None, True, "Invalid Graph Preset.")
            return
        job.set_graph_preset(graph_preset)

        # --- Apply Scene Settings ---
        self.apply_scene_settings(job_data.get("scene_settings", {}))

        # --- Set Exposed Graph Variables ---
        variable_overrides = job.get_or_create_variable_overrides(graph_preset)

        # 1. Set Output path (DirectoryPath struct)
        output_path_var = graph_preset.get_variable_by_name("Output path")
        if output_path_var:
            dir_path_struct = unreal.DirectoryPath()
            dir_path_struct.path = job_data["output_path"]
            variable_overrides.set_variable_assignment_enable_state(output_path_var, True)
            variable_overrides.set_value_serialized_string(output_path_var, dir_path_struct.export_text())
            unreal.log(f"RealisVirtualPlateRenderExecutor: Overrode 'Output path' (DirectoryPath) to: {dir_path_struct.path}")
        else:
            unreal.log_warning("RealisVirtualPlateRenderExecutor: Graph is missing exposed 'Output path' (DirectoryPath) variable.")

        # 2. Set Resolution (MovieGraphNamedResolution struct)
        resolution_var = graph_preset.get_variable_by_name("Resolution")
        if resolution_var:
            res_x, res_y = job_data["resolution"]
            resolution_string = f'(ProfileName="Custom",Resolution=(X={res_x},Y={res_y}),Description="Render Farm Job")'
            variable_overrides.set_variable_assignment_enable_state(resolution_var, True)
            variable_overrides.set_value_serialized_string(resolution_var, resolution_string)
            unreal.log(f"RealisVirtualPlateRenderExecutor: Overrode 'Resolution' (NamedResolution) to: {resolution_string}")
        else:
            unreal.log_warning("RealisVirtualPlateRenderExecutor: Graph is missing exposed 'Resolution' (NamedResolution) variable.")

        # --- Start the Render ---
        world = self.get_last_loaded_world()
        self.active_movie_pipeline = unreal.new_object(self.target_pipeline_class, outer=world)
        self.active_movie_pipeline.on_movie_pipeline_work_finished_delegate.add_function_unique(self, "on_movie_pipeline_finished")

        unreal.log("RealisVirtualPlateRenderExecutor: Initializing pipeline and starting render.")

        if isinstance(self.active_movie_pipeline, unreal.MovieGraphPipeline):
            init_config = unreal.MovieGraphInitConfig()
            self.active_movie_pipeline.initialize(job, init_config)
        else:
            self.active_movie_pipeline.initialize(job)


    def apply_scene_settings(self, settings_dict):
        """Finds BP_SceneSettings actor by tag and sets its public variables."""
        if not settings_dict:
            return

        unreal.log("RealisVirtualPlateRenderExecutor: Applying scene settings...")
        world = self.get_last_loaded_world()

        # Use the more reliable tag-based search.
        # The user must add the tag 'SceneSettingsController' to their BP_SceneSettings actor in the editor.
        actors_with_tag = unreal.GameplayStatics.get_all_actors_with_tag(world, "SceneSettingsController")
        
        if not actors_with_tag:
            unreal.log_error("RealisVirtualPlateRenderExecutor: Could not find any actor with tag 'SceneSettingsController'. Please tag your BP_SceneSettings actor.")
            return

        scene_settings_actor = actors_with_tag[0]
        unreal.log(f"Found actor with tag 'SceneSettingsController': {scene_settings_actor.get_name()}")

        # This approach directly sets public, "Instance Editable" variables on the Blueprint.
        if "time_of_day" in settings_dict:
            try:
                # The property name "time_of_day" must match the variable name in the Blueprint.
                scene_settings_actor.set_editor_property("time_of_day", float(settings_dict["time_of_day"]))
                unreal.log(f"RealisVirtualPlateRenderExecutor: Set property 'time_of_day' to {settings_dict['time_of_day']}")
            except Exception as e:
                unreal.log_warning(f"Could not set 'time_of_day' property on actor. Make sure the variable exists, is a Float, and is 'Instance Editable'. Error: {e}")

        if "cloud_coverage" in settings_dict:
            try:
                # The property name "cloud_coverage" must match the variable name in the Blueprint.
                scene_settings_actor.set_editor_property("cloud_coverage", float(settings_dict["cloud_coverage"]))
                unreal.log(f"RealisVirtualPlateRenderExecutor: Set property 'cloud_coverage' to {settings_dict['cloud_coverage']}")
            except Exception as e:
                unreal.log_warning(f"Could not set 'cloud_coverage' property on actor. Make sure the variable exists, is a Float, and is 'Instance Editable'. Error: {e}")

    @unreal.ufunction(ret=None, params=[unreal.MoviePipelineOutputData])
    def on_movie_pipeline_finished(self, results):
        """Callback for when the active pipeline finishes a job."""
        if results.success:
            unreal.log("RealisVirtualPlateRenderExecutor: Movie pipeline finished successfully.")
            self.on_executor_finished_impl()
        else:
            unreal.log_error("RealisVirtualPlateRenderExecutor: Movie pipeline finished with errors.")
            self.on_executor_errored(None, True, "Rendering failed within the pipeline.")

    @unreal.ufunction(override=True)
    def on_begin_frame(self):
        super(RealisVirtualPlateRenderExecutor, self).on_begin_frame()
