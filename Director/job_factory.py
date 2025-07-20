import time
import os
from itertools import product

class JobFactory:
    """
    A class responsible for creating batches of render job dictionaries
    from a complex set of UI inputs.
    """
    def create_job_batch(self, form_data):
        """
        Constructs a list of job dictionaries by creating a permutation of all
        enabled sequences, cameras, scene presets, and resolution presets.

        :param form_data: A dictionary containing the UI form data.
        :return: A list of job dictionaries, or an empty list if no valid permutations exist.
        """
        try:
            # --- Extract common settings ---
            project_path = form_data.get('project_path', '')
            graph_path = form_data.get('graph_path', '')
            level_path = form_data.get('level_path', '')
            project_dir = os.path.dirname(project_path) if project_path.endswith('.uproject') else project_path

            # --- Filter enabled presets ---
            enabled_sequences = self._get_enabled_sequences(form_data.get('sequences', []))
            enabled_scene_presets = [p for p in form_data.get('scene_presets', []) if p.get('enabled')]
            enabled_resolution_presets = [p for p in form_data.get('resolution_presets', []) if p.get('enabled')]

            if not all([enabled_sequences, enabled_scene_presets, enabled_resolution_presets]):
                print("Warning: One or more preset lists are empty or disabled. No jobs will be created.")
                return []

            # --- Generate all permutations ---
            all_permutations = product(enabled_sequences, enabled_scene_presets, enabled_resolution_presets)

            job_list = []
            for combo in all_permutations:
                sequence_info, scene_preset, resolution_preset = combo
                
                job_name = f"job_{int(time.time() * 1000)}_{len(job_list)}"
                output_path = os.path.join(project_dir, 'Saved', 'RenderJobs', job_name, 'export').replace('\\', '/')

                job_dict = {
                    "job_id": job_name,
                    "project_path": project_path,
                    "graph_path": graph_path,
                    "level_path": level_path,
                    "sequence_path": sequence_info['path'],
                    "camera_actor_name": sequence_info['camera'],
                    "output_path": output_path,
                    "resolution": [
                        int(resolution_preset.get('res_x', 1920)),
                        int(resolution_preset.get('res_y', 1080))
                    ],
                    "scene_settings": scene_preset.get('settings', {})
                }
                job_list.append(job_dict)

            return job_list
        except (ValueError, TypeError, KeyError) as e:
            print(f"Error creating job batch: {e}")
            return []

    def _get_enabled_sequences(self, sequences_data):
        """Helper to flatten the sequence tree into a list of {path, camera} dicts."""
        flat_list = []
        for seq in sequences_data:
            for cam in seq.get('cameras', []):
                flat_list.append({'path': seq.get('path'), 'camera': cam})
        return flat_list

