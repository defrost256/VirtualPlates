import socket
import json
import subprocess
import time
import os
import threading

class RenderAgent:
    """
    A Render Agent that listens for jobs from a Director, executes them
    using Unreal Engine, and reports progress.
    """
    def __init__(self, config_path='agent_config.json'):
        """Initializes the agent by loading its configuration."""
        self.config = self.load_config(config_path)
        self.agent_id = self.config.get('agent_id', 'default-agent')
        self.host = self.config.get('listen_host', '0.0.0.0')
        self.port = self.config.get('listen_port', 9999)
        self.ue_path = self.config.get('unreal_editor_path')
        self.jobs_dir = self.config.get('jobs_directory', 'C:/RenderJobs')
        self.is_busy = False
        os.makedirs(self.jobs_dir, exist_ok=True)
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
        """Starts the TCP server to listen for connections from the Director."""
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.bind((self.host, self.port))
        server_socket.listen(1)
        print(f"Agent '{self.agent_id}' listening on {self.host}:{self.port}")

        while True:
            try:
                conn, addr = server_socket.accept()
                print(f"Connection from Director at {addr}")
                # Use a thread to handle the client so the server can accept new connections if needed,
                # although our current logic is one job at a time.
                threading.Thread(target=self.handle_job_request, args=(conn,)).start()
            except Exception as e:
                print(f"Error in server loop: {e}")

    def handle_job_request(self, conn: socket.socket):
        """Handles a single job request from a Director connection."""
        if self.is_busy:
            print("Agent is busy, rejecting job.")
            conn.sendall(json.dumps({"status": "error", "message": "Agent is busy"}).encode('utf-8'))
            conn.close()
            return
        
        self.is_busy = True
        try:
            # Receive job data from the director
            data = conn.recv(4096) # 4KB buffer should be enough for a job definition
            if not data:
                raise ConnectionError("Director disconnected without sending data.")

            job_data = json.loads(data.decode('utf-8'))
            job_id = job_data.get('job_id', f'job_{int(time.time())}')
            print(f"Received job: {job_id}")

            # Prepare file paths for the job
            job_file_path = os.path.join(self.jobs_dir, f"{job_id}.json")
            progress_file_path = os.path.join(self.jobs_dir, f"{job_id}.stat")

            # Write the received job data to a local file
            with open(job_file_path, 'w') as f:
                json.dump(job_data, f)

            # Clean up old progress file if it exists
            if os.path.exists(progress_file_path):
                os.remove(progress_file_path)

            # Construct the command to run the UE render
            command = [
                f"\"{self.ue_path}\"",
                f'"{job_data["project_path"]}"',
                "-game",
                "-MoviePipelineClass=/Script/MovieRenderPipelineCore.MovieGraphPipeline",
                "-MoviePipelineLocalExecutorClass=/Script/MovieRenderPipelineCore.MoviePipelinePythonHostExecutor",
                f"-ExecutorPythonClass=/Engine/PythonTypes.RealisVirtualPlateRenderExecutor",
                "-AllowCommandletRendering",
                "-windowed", "-resx=1280", "-resy=720", "-log",
                f'-JobPath="{job_file_path}"',
                f'-GraphPath="{job_data["graph_path"]}"',
                f'-ProgressFile="{progress_file_path}"'
            ]
            
            print(f"Executing command: {' '.join(command)}")
            
            # Launch the Unreal Engine process
            process = subprocess.Popen(' '.join(command), shell=True)

            # Monitor progress and report back to director
            self.monitor_and_report_progress(process, progress_file_path, conn)

        except (json.JSONDecodeError, KeyError) as e:
            print(f"Error processing job data: {e}")
            conn.sendall(json.dumps({"status": "error", "message": f"Invalid job data: {e}"}).encode('utf-8'))
        except Exception as e:
            print(f"An error occurred while handling the job: {e}")
        finally:
            print("Job finished or failed. Cleaning up.")
            conn.close()
            self.is_busy = False

    def monitor_and_report_progress(self, process: subprocess.Popen, progress_file: str, conn: socket.socket):
        """Monitors the progress file and sends updates to the director."""
        last_reported_status = None

        while process.poll() is None:
            time.sleep(1) # Check for updates every second
            try:
                if not os.path.exists(progress_file):
                    continue
                
                with open(progress_file, 'r') as f:
                    lines = f.readlines()
                
                if not lines:
                    continue

                # Get the last valid JSON object from the file
                last_line = lines[-1].strip()
                if last_line:
                    status_data = json.loads(last_line)
                    if status_data != last_reported_status:
                        print(f"Reporting status: {status_data}")
                        conn.sendall((json.dumps(status_data) + '\n').encode('utf-8'))
                        last_reported_status = status_data

                        if status_data.get("status") in ["Completed", "Error"]:
                            return # Exit the monitoring loop

            except (IOError, json.JSONDecodeError) as e:
                print(f"Warning: Could not read or parse progress file: {e}")
            except ConnectionError as e:
                print(f"Director disconnected: {e}")
                process.terminate() # Stop the render if the director disconnects
                return

        # Final check after process has terminated
        return_code = process.returncode
        if return_code != 0:
            print(f"Unreal Engine process exited with non-zero code: {return_code}")
            error_status = {
                "timestamp": time.time(),
                "job_id": last_reported_status.get('job_id', 'unknown'),
                "status": "Error",
                "reason": f"Process crashed with exit code {return_code}"
            }
            conn.sendall((json.dumps(error_status) + '\n').encode('utf-8'))


if __name__ == '__main__':
    agent = RenderAgent()
    agent.start_server()

