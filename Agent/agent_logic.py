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
        :param callbacks: A dictionary of functions for events.
                          Expected keys: 'on_status_update', 'on_job_finished'.
        """
        self.config = config
        self.callbacks = callbacks
        
        # --- State Management ---
        self.state_lock = threading.Lock()
        self.is_busy = False
        self.current_job_data = None
        self.last_known_status = self._get_idle_status()

    # --- Public Methods ---

    def get_current_status(self):
        """
        Thread-safely gets the last known status report of the agent.
        This is one of only two methods that should acquire the lock.
        """
        with self.state_lock:
            return self.last_known_status.copy()

    def start_job(self, job_data):
        """
        Attempts to start a new render job.
        :param job_data: A dictionary containing the job definition.
        :return: True if the job was started, False if the agent was busy.
        """
        job_id = job_data.get('job_id', 'unknown_job')
        
        # Atomically check if busy and set the new state if not.
        if not self._set_state_to_busy(job_data):
            print("Logic: Agent is busy, rejecting job.")
            return False

        print(f"Logic: Accepted new job: {job_id}")
        
        # Immediately report that the agent is starting the job.
        starting_status = self._get_idle_status()
        starting_status.update({
            "status": "Starting",
            "job_id": job_id,
            "timestamp": time.time()
        })
        self._update_and_broadcast_status(starting_status)
        
        # The monitoring process will run in a separate thread.
        threading.Thread(target=self._execute_and_monitor_job).start()
        return True

    # --- Private, Thread-Safe State Modifiers ---

    def _update_and_broadcast_status(self, new_status_report):
        """
        Thread-safely updates the agent's status and calls the broadcast callback.
        This is one of only two methods that should acquire the lock.
        """
        with self.state_lock:
            self.last_known_status = new_status_report
        
        # Broadcast the update *after* releasing the lock.
        self.callbacks['on_status_update'](new_status_report)
        
    def _set_state_to_busy(self, job_data):
        """Atomically checks and sets the agent to a busy state."""
        with self.state_lock:
            if self.is_busy:
                return False
            self.is_busy = True
            self.current_job_data = job_data
            return True
            
    def _set_state_to_idle(self):
        """Atomically sets the agent to an idle state."""
        with self.state_lock:
            self.is_busy = False
            self.current_job_data = None
        
        # Create and broadcast the new idle status
        idle_status = self._get_idle_status()
        self._update_and_broadcast_status(idle_status)

    # --- Core Job Execution Logic ---

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
        last_status = self.get_current_status()

        if return_code != 0 and last_status.get("status") != "Error":
            error_status = {
                "timestamp": time.time(), "job_id": job_id, "status": "Error",
                "reason": f"Process crashed with exit code {return_code}"
            }
            self._update_and_broadcast_status(error_status)

        # --- Cleanup ---
        job_was_completed = last_status.get("status") == "Completed"

        # If the job just completed, wait 2 seconds before becoming idle.
        # This is done OUTSIDE any lock.
        if job_was_completed:
            print(f"Logic: Job {job_id} completed. Waiting 2 seconds before becoming idle.")
            time.sleep(2)

        # Atomically reset the agent state to idle.
        self._set_state_to_idle()
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
                # Only broadcast if the status has actually changed.
                if status_data != self.get_current_status():
                    self._update_and_broadcast_status(status_data)
        except (IOError, json.JSONDecodeError) as e:
            print(f"Warning: Could not read or parse progress file: {e}")

    def _get_idle_status(self):
        """Generates a standard 'Idle' status dictionary."""
        return {
            "timestamp": time.time(), "job_id": None,
            "status": "Idle", "agent_id": self.config.get('agent_id'),
            "progress": 0, "current_frame": 0
        }
