import socket
import json
import subprocess
import time
import os
import threading

class RenderAgent:
    """
    A multi-client aware Render Agent that listens for jobs from Directors,
    executes them using Unreal Engine, and broadcasts progress to all
    connected Directors.
    """
    def __init__(self, config_path='agent_config.json'):
        """Initializes the agent by loading its configuration."""
        self.config = self.load_config(config_path)
        self.agent_id = self.config.get('agent_id', 'default-agent')
        self.host = self.config.get('listen_host', '0.0.0.0')
        self.port = self.config.get('listen_port', 9999)
        self.ue_path = self.config.get('unreal_editor_path')
        self.jobs_dir = self.config.get('jobs_directory', 'C:/RenderJobs')
        os.makedirs(self.jobs_dir, exist_ok=True)

        # --- State Management ---
        self.state_lock = threading.Lock()
        self.director_connections = []
        self.is_busy = False
        self.current_job_data = None
        self.last_known_status = None
        
        print(f"Agent '{self.agent_id}' initialized. Jobs directory: {self.jobs_dir}")

    def load_config(self, path):
        """Loads the agent's JSON configuration file."""
        try:
            with open(path, 'r') as f:
                return json.load(f)
        except FileNotFoundError:
            print(f"ERROR: Config file not found at {path}. Exiting.")
            exit(1)
        except json.JSONDecodeError:
            print(f"ERROR: Could not parse config file at {path}. Exiting.")
            exit(1)

    def start_server(self):
        """Starts the main TCP server loop to accept Director connections."""
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.bind((self.host, self.port))
        server_socket.listen(5)
        print(f"Agent '{self.agent_id}' listening on {self.host}:{self.port}")

        while True:
            try:
                conn, addr = server_socket.accept()
                print(f"Accepted connection from Director at {addr}")
                threading.Thread(target=self.handle_director, args=(conn, addr)).start()
            except Exception as e:
                print(f"Error in server accept loop: {e}")

    def handle_director(self, conn: socket.socket, addr):
        """Manages the lifecycle of a single Director connection."""
        with self.state_lock:
            self.director_connections.append(conn)

        try:
            with self.state_lock:
                if self.is_busy and self.last_known_status:
                    # If busy, send the last known status immediately.
                    conn.sendall((json.dumps(self.last_known_status) + '\n').encode('utf-8'))
                else:
                    # If idle, send a complete idle status report.
                    idle_status = {
                        "timestamp": time.time(), "job_id": None,
                        "status": "Idle", "agent_id": self.agent_id,
                        "progress": 0, "current_frame": 0
                    }
                    conn.sendall((json.dumps(idle_status) + '\n').encode('utf-8'))
        except socket.error as e:
            print(f"Error sending initial status to {addr}: {e}")
            self.cleanup_connection(conn)
            return

        # --- Main Loop: Listen for new jobs from this director ---
        while True:
            try:
                data = conn.recv(4096)
                if not data:
                    print(f"Director {addr} disconnected.")
                    break

                job_data = json.loads(data.decode('utf-8'))
                
                with self.state_lock:
                    if self.is_busy:
                        print(f"Agent is busy. Rejecting job from {addr}.")
                        response = {"status": "error", "message": "Agent is busy with another job."}
                        conn.sendall((json.dumps(response) + '\n').encode('utf-8'))
                    else:
                        print(f"Accepted new job from {addr}.")
                        self.is_busy = True
                        self.current_job_data = job_data
                        threading.Thread(target=self.execute_and_monitor_job).start()

            except (json.JSONDecodeError, KeyError) as e:
                print(f"Received invalid job data from {addr}: {e}")
            except socket.error:
                print(f"Director {addr} connection lost.")
                break
        
        self.cleanup_connection(conn)

    def cleanup_connection(self, conn: socket.socket):
        """Removes a director's connection from the active list."""
        with self.state_lock:
            if conn in self.director_connections:
                self.director_connections.remove(conn)
        conn.close()

    def broadcast_status(self, status_data):
        """Sends a status update to all connected Directors."""
        with self.state_lock:
            self.last_known_status = status_data
            dead_connections = []
            for conn in self.director_connections:
                try:
                    conn.sendall((json.dumps(status_data) + '\n').encode('utf-8'))
                except socket.error:
                    dead_connections.append(conn)
            
            for conn in dead_connections:
                self.cleanup_connection(conn)

    def execute_and_monitor_job(self):
        """Launches the UE process and monitors its progress file."""
        job_data = self.current_job_data
        job_id = job_data.get('job_id', f'job_{int(time.time())}')
        
        job_file_path = os.path.join(self.jobs_dir, f"{job_id}.json")
        progress_file_path = os.path.join(self.jobs_dir, f"{job_id}.stat")

        with open(job_file_path, 'w') as f:
            json.dump(job_data, f)

        if os.path.exists(progress_file_path):
            os.remove(progress_file_path)

        command = [
            f'"{self.ue_path}"',
            f'"{job_data["project_path"]}"',
            "-game", "-MoviePipelineClass=/Script/MovieRenderPipelineCore.MovieGraphPipeline",
            "-MoviePipelineLocalExecutorClass=/Script/MovieRenderPipelineCore.MoviePipelinePythonHostExecutor",
            f"-ExecutorPythonClass=/Engine/PythonTypes.RealisVirtualPlateRenderExecutor",
            "-AllowCommandletRendering", "-windowed", "-resx=1280", "-resy=720", "-log",
            f'-JobPath="{job_file_path}"', f'-GraphPath="{job_data["graph_path"]}"',
            f'-ProgressFile="{progress_file_path}"'
        ]
        
        process = subprocess.Popen(' '.join(command), shell=True)

        while process.poll() is None:
            time.sleep(1)
            try:
                if not os.path.exists(progress_file_path) or os.path.getsize(progress_file_path) == 0:
                    continue
                
                with open(progress_file_path, 'r') as f:
                    lines = f.readlines()
                
                if lines:
                    last_line = lines[-1].strip()
                    status_data = json.loads(last_line)
                    if status_data != self.last_known_status:
                        self.broadcast_status(status_data)
                        if status_data.get("status") in ["Completed", "Error"]:
                            break
            except (IOError, json.JSONDecodeError) as e:
                print(f"Warning: Could not read or parse progress file: {e}")
        while process.poll() is None:
            time.sleep(0.1)
        return_code = process.returncode
        if return_code != 0 and (not self.last_known_status or self.last_known_status.get("status") != "Error"):
            print(f"Process error {return_code}")
            error_status = {
                "timestamp": time.time(), "job_id": job_id, "status": "Error",
                "reason": f"Process crashed with exit code {return_code}"
            }
            self.broadcast_status(error_status)

        with self.state_lock:
            self.is_busy = False
            self.current_job_data = None
            self.last_known_status = None
            print(f"Job {job_id} finished. Agent is now idle.")


if __name__ == '__main__':
    agent = RenderAgent()
    agent.start_server()
