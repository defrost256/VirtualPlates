import subprocess
import threading
import json
import time
import os

class AgentLogic:
    """
    Handles the core state and process management for the render agent.
    This class is completely decoupled from the networking layer.
    """
    def __init__(self, config, callbacks):
        """
        Initializes the agent's logic controller.
        :param config: A dictionary containing configuration like UE path, jobs directory.
        :param callbacks: A dictionary of functions to call for events.
                          Expected keys: 'on_status_update', 'on_job_finished'.
        """
        self.config = config
        self.callbacks = callbacks
        
        # --- State Management ---
        self.state_lock = threading.Lock()
        self.is_busy = False
        self.current_job_data = None
        self.last_known_status = self._get_idle_status()

    def get_current_status(self):
        """Returns the last known status of the agent."""
        with self.state_lock:
            return self.last_known_status

    def start_job(self, job_data):
        """
        Attempts to start a new render job.
        :param job_data: A dictionary containing the job definition.
        :return: True if the job was started, False if the agent was busy.
        """
        with self.state_lock:
            if self.is_busy:
                print("Logic: Agent is busy, rejecting job.")
                return False
            
            print(f"Logic: Accepted new job: {job_data.get('job_id')}")
            self.is_busy = True
            self.current_job_data = job_data
            # The monitoring process will run in a separate thread.
            threading.Thread(target=self._execute_and_monitor_job).start()
            return True

    def _execute_and_monitor_job(self):
        """The private method that launches and monitors the UE process."""
        job_data = self.current_job_data
        job_id = job_data.get('job_id', f'job_{int(time.time())}')
        
        # Prepare file paths
        job_file_path = os.path.join(self.config['jobs_directory'], f"{job_id}.json")
        progress_file_path = os.path.join(self.config['jobs_directory'], f"{job_id}.stat")

        with open(job_file_path, 'w') as f:
            json.dump(job_data, f)

        if os.path.exists(progress_file_path):
            os.remove(progress_file_path)

        # Construct and execute the command
        command = [
            f'"{self.config["unreal_editor_path"]}"',
            f'"{job_data["project_path"]}"',
            "-game", "-MoviePipelineClass=/Script/MovieRenderPipelineCore.MovieGraphPipeline",
            "-MoviePipelineLocalExecutorClass=/Script/MovieRenderPipelineCore.MoviePipelinePythonHostExecutor",
            f"-ExecutorPythonClass=/Engine/PythonTypes.RealisVirtualPlateRenderExecutor",
            "-AllowCommandletRendering", "-windowed", "-resx=1280", "-resy=720", "-log",
            f'-JobPath="{job_file_path}"', f'-GraphPath="{job_data["graph_path"]}"',
            f'-ProgressFile="{progress_file_path}"'
        ]
        
        process = subprocess.Popen(' '.join(command), shell=True)

        # --- Monitoring Loop ---
        while process.poll() is None:
            time.sleep(1)
            self._check_progress_file(progress_file_path)

        # --- Final Status Check ---
        return_code = process.returncode
        if return_code != 0 and (not self.last_known_status or self.last_known_status.get("status") != "Error"):
            error_status = {
                "timestamp": time.time(), "job_id": job_id, "status": "Error",
                "reason": f"Process crashed with exit code {return_code}"
            }
            self._update_status(error_status)

        # --- Cleanup ---
        with self.state_lock:
            self.is_busy = False
            self.current_job_data = None
            # Set status back to idle and notify directors
            self._update_status(self._get_idle_status())
            print(f"Logic: Job {job_id} finished. Agent is now idle.")
            self.callbacks['on_job_finished']()

    def _check_progress_file(self, progress_file):
        """Reads the last line of the progress file and triggers status update."""
        try:
            if not os.path.exists(progress_file) or os.path.getsize(progress_file) == 0:
                return
            
            with open(progress_file, 'r') as f:
                lines = f.readlines()
            
            if lines:
                last_line = lines[-1].strip()
                status_data = json.loads(last_line)
                if status_data != self.last_known_status:
                    self._update_status(status_data)
        except (IOError, json.JSONDecodeError) as e:
            print(f"Warning: Could not read or parse progress file: {e}")

    def _update_status(self, status_data):
        """Thread-safe method to update internal status and call the broadcast callback."""
        with self.state_lock:
            self.last_known_status = status_data
        self.callbacks['on_status_update'](status_data)

    def _get_idle_status(self):
        """Generates a standard 'Idle' status dictionary."""
        return {
            "timestamp": time.time(), "job_id": None,
            "status": "Idle", "agent_id": self.config.get('agent_id'),
            "progress": 0, "current_frame": 0
        }
