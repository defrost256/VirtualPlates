# Copyright Epic Games, Inc. All Rights Reserved.
# Modified for the Realis Render Farm System.
import unreal
import json
import time
import os

@unreal.uclass()
class RealisVirtualPlateRenderExecutor(unreal.MoviePipelinePythonHostExecutor):
    """
    Custom Movie Pipeline Executor for the Realis render farm.
    This class is instantiated by the engine when launched with the correct
    command-line arguments. It's responsible for parsing a job description
    file, executing a single render job, and reporting progress to a file.
    """
    # --- UPROPERTY Declarations ---
    # These decorators tell Unreal's Garbage Collector that these Python
    # attributes hold references or data that must persist across frames and
    # function calls, preventing them from being reset or destroyed.
    active_movie_pipeline = unreal.uproperty(unreal.MoviePipelineBase)
    progress_file_path = unreal.uproperty(str)
    job_id = unreal.uproperty(str)
    last_progress = unreal.uproperty(float)

    def _post_init(self):
        """Constructor for the executor."""
        # Set initial default values for the properties.
        self.active_movie_pipeline = None
        self.progress_file_path = ""
        self.job_id = ""
        self.last_progress = -1.0
        unreal.log("RealisVirtualPlateRenderExecutor: Initialized.")

    def write_status(self, status_dict):
        """Appends a JSON status object to the progress file."""
        if not self.progress_file_path:
            return

        try:
            # Ensure the directory exists
            os.makedirs(os.path.dirname(self.progress_file_path), exist_ok=True)
            
            # Append the status as a new line in the file
            with open(self.progress_file_path, 'a') as f:
                f.write(json.dumps(status_dict) + '\n')
        except Exception as e:
            unreal.log_error(f"RealisVirtualPlateRenderExecutor: Could not write to progress file. Error: {e}")


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
        self.progress_file_path = cmd_parameters.get('ProgressFile')

        if not job_path or not graph_path or not self.progress_file_path:
            unreal.log_error("RealisVirtualPlateRenderExecutor: Missing -JobPath, -GraphPath, or -ProgressFile arguments. Shutting down.")
            self.on_executor_errored(None, True, "Missing command line arguments.")
            return

        # --- Load Job Definition ---
        try:
            with open(job_path, 'r') as f:
                job_data = json.load(f)
            self.job_id = job_data.get("job_id", "unknown_job")
        except Exception as e:
            unreal.log_error(f"RealisVirtualPlateRenderExecutor: Failed to load or parse job file at {job_path}. Error: {e}")
            self.on_executor_errored(None, True, "Failed to load job file.")
            return

        # --- Build and Configure the Job ---
        self.pipeline_queue = unreal.new_object(unreal.MoviePipelineQueue, outer=self)
        job = self.pipeline_queue.allocate_new_job(unreal.MoviePipelineExecutorJob)

        job.job_name = self.job_id
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
        
        # Set Output path (DirectoryPath struct)
        output_path_var = graph_preset.get_variable_by_name("Output path")
        if output_path_var:
            dir_path_struct = unreal.DirectoryPath()
            dir_path_struct.path = job_data["output_path"]
            variable_overrides.set_variable_assignment_enable_state(output_path_var, True)
            variable_overrides.set_value_serialized_string(output_path_var, dir_path_struct.export_text())
        
        # Set Resolution (MovieGraphNamedResolution struct)
        resolution_var = graph_preset.get_variable_by_name("Resolution")
        if resolution_var:
            res_x, res_y = job_data["resolution"]
            resolution_string = f'(ProfileName="Custom",Resolution=(X={res_x},Y={res_y}),Description="Render Farm Job")'
            variable_overrides.set_variable_assignment_enable_state(resolution_var, True)
            variable_overrides.set_value_serialized_string(resolution_var, resolution_string)

        # --- Start the Render ---
        world = self.get_last_loaded_world()
        self.active_movie_pipeline = unreal.new_object(self.target_pipeline_class, outer=world)
        self.active_movie_pipeline.on_movie_pipeline_work_finished_delegate.add_function_unique(self, "on_movie_pipeline_finished")

        self.write_status({"timestamp": time.time(), "job_id": self.job_id, "status": "Initializing"})
        unreal.log("RealisVirtualPlateRenderExecutor: Initializing pipeline and starting render.")

        if isinstance(self.active_movie_pipeline, unreal.MovieGraphPipeline):
            init_config = unreal.MovieGraphInitConfig()
            self.active_movie_pipeline.initialize(job, init_config)
        else:
            self.active_movie_pipeline.initialize(job)


    def apply_scene_settings(self, settings_dict):
        if not settings_dict: return
        world = self.get_last_loaded_world()
        actors_with_tag = unreal.GameplayStatics.get_all_actors_with_tag(world, "SceneSettings")
        if not actors_with_tag:
            unreal.log_error("Could not find actor with tag 'SceneSettings'.")
            return

        scene_settings_actor = actors_with_tag[0]
        settings_were_applied = False
        if "time_of_day" in settings_dict:
            scene_settings_actor.set_editor_property("time_of_day", float(settings_dict["time_of_day"]))
            settings_were_applied = True
        if "cloud_coverage" in settings_dict:
            scene_settings_actor.set_editor_property("cloud_coverage", float(settings_dict["cloud_coverage"]))
            settings_were_applied = True
        if settings_were_applied:
            scene_settings_actor.set_editor_property("dirty_settings", True)


    @unreal.ufunction(ret=None, params=[unreal.MoviePipelineOutputData])
    def on_movie_pipeline_finished(self, results):
        """Callback for when the active pipeline finishes a job."""
        if results.success:
            unreal.log("RealisVirtualPlateRenderExecutor: Movie pipeline finished successfully.")
            self.write_status({"timestamp": time.time(), "job_id": self.job_id, "status": "Completed"})
            self.on_executor_finished_impl()
        else:
            unreal.log_error("RealisVirtualPlateRenderExecutor: Movie pipeline finished with errors.")
            self.write_status({"timestamp": time.time(), "job_id": self.job_id, "status": "Error", "reason": "Pipeline reported failure."})
            self.on_executor_errored(None, True, "Rendering failed within the pipeline.")

    @unreal.ufunction(override=True)
    def on_begin_frame(self):
        """Called every frame. Used to report progress."""
        super(RealisVirtualPlateRenderExecutor, self).on_begin_frame()
        
        # Ensure we have a valid pipeline object and that it is a MovieGraphPipeline
        if self.active_movie_pipeline and isinstance(self.active_movie_pipeline, unreal.MovieGraphPipeline):
            # Check if the pipeline is actually in the rendering state.
            if self.active_movie_pipeline.get_pipeline_state() == unreal.MovieRenderPipelineState.PRODUCING_FRAMES:
                
                # --- MODIFICATION ---
                # Use the new MovieGraphBlueprintLibrary for getting progress information.
                progress = unreal.MovieGraphLibrary.get_completion_percentage(self.active_movie_pipeline)
                
                # To avoid spamming the file, only write if progress has changed meaningfully.
                if progress > self.last_progress + 0.01:
                    self.last_progress = progress
                    
                    # Get current frame number using the graph library
                    current_frame_struct = unreal.MovieGraphLibrary.get_current_shot_frame_number(self.active_movie_pipeline)
                    current_frame = current_frame_struct.value if hasattr(current_frame_struct, 'value') else 0

                    status_update = {
                        "timestamp": time.time(),
                        "job_id": self.job_id,
                        "status": "Rendering",
                        "progress": round(progress, 4),
                        "current_frame": current_frame
                    }
                    self.write_status(status_update)
