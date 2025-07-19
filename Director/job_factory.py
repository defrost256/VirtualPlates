import time
import os

class JobFactory:
    """
    A class responsible for creating render job dictionaries from UI inputs.
    """
    def create_job_dict(self, form_data):
        """
        Constructs a job dictionary based on the provided form data.

        :param form_data: A dictionary containing all the input values from the UI form.
        :return: A dictionary representing the complete job configuration.
        """
        try:
            # --- Auto-generated values ---
            job_name = f"job_{int(time.time())}"
            project_path = form_data.get('project_path', '')
            
            # Derive the project directory from the .uproject file path
            project_dir = os.path.dirname(project_path) if project_path.endswith('.uproject') else project_path
            output_path = os.path.join(project_dir, 'Saved', 'RenderJobs', job_name, 'export').replace('\\', '/')

            # --- Scene Settings ---
            # This structure makes it easy to add more scene parameters in the future
            scene_settings = {}
            scene_params = [
                ('time_of_day', float),
                ('cloud_coverage', float)
            ]
            for param, cast_type in scene_params:
                if form_data.get(param) is not None:
                    scene_settings[param] = cast_type(form_data[param])

            # --- Assemble the final job dictionary ---
            job_dict = {
                "job_id": job_name,
                "project_path": project_path,
                "graph_path": form_data.get('graph_path', ''),
                "level_path": form_data.get('level_path', ''),
                "sequence_path": form_data.get('sequence_path', ''),
                "camera_actor_name": form_data.get('camera_name', ''),
                "output_path": output_path,
                "resolution": [
                    int(form_data.get('res_x', 1920)),
                    int(form_data.get('res_y', 1080))
                ],
                "scene_settings": scene_settings
            }
            return job_dict
        except (ValueError, TypeError) as e:
            print(f"Error creating job dictionary: {e}")
            return None
