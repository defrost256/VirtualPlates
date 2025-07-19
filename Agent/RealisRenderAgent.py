import socket
import json
import threading
from agent_logic import AgentLogic

class AgentServer:
    """
    Handles all TCP network communication for the render agent, delegating
    core logic to an AgentLogic instance.
    """
    def __init__(self, config_path='agent_config.json'):
        self.config = self._load_config(config_path)
        
        # --- Callbacks for the Logic Controller ---
        callbacks = {
            'on_status_update': self.broadcast_status,
            'on_job_finished': self.on_job_finished
        }
        self.logic = AgentLogic(self.config, callbacks)
        
        # --- Networking State ---
        self.director_connections = []
        self.connections_lock = threading.Lock()

    def start(self):
        """Starts the main TCP server to listen for Director connections."""
        host = self.config.get('listen_host', '0.0.0.0')
        port = self.config.get('listen_port', 9999)
        
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.bind((host, port))
        server_socket.listen(5)
        print(f"Agent Server listening on {host}:{port}")

        while True:
            conn, addr = server_socket.accept()
            print(f"Accepted connection from Director at {addr}")
            threading.Thread(target=self.handle_director, args=(conn, addr)).start()

    def handle_director(self, conn: socket.socket, addr):
        """Manages the lifecycle of a single Director connection."""
        with self.connections_lock:
            self.director_connections.append(conn)

        try:
            # On connect, immediately send the agent's current status.
            initial_status = self.logic.get_current_status()
            conn.sendall((json.dumps(initial_status) + '\n').encode('utf-8'))
        except socket.error as e:
            print(f"Error sending initial status to {addr}: {e}")
            self._cleanup_connection(conn)
            return

        # --- Main Loop: Listen for new jobs from this director ---
        while True:
            try:
                data = conn.recv(4096)
                if not data:
                    print(f"Director {addr} disconnected.")
                    break

                job_data = json.loads(data.decode('utf-8'))
                
                if not self.logic.start_job(job_data):
                    # If logic controller rejected the job, inform the director.
                    response = {"status": "error", "message": "Agent is busy with another job."}
                    conn.sendall((json.dumps(response) + '\n').encode('utf-8'))

            except (json.JSONDecodeError, KeyError) as e:
                print(f"Received invalid job data from {addr}: {e}")
            except socket.error:
                print(f"Director {addr} connection lost.")
                break
        
        self._cleanup_connection(conn)

    # --- Callback Implementations ---
    
    def broadcast_status(self, status_data):
        """Sends a status update to all connected Directors."""
        with self.connections_lock:
            dead_connections = []
            for conn in self.director_connections:
                try:
                    conn.sendall((json.dumps(status_data) + '\n').encode('utf-8'))
                except socket.error:
                    dead_connections.append(conn)
            
            for conn in dead_connections:
                self._cleanup_connection(conn)

    def on_job_finished(self):
        """Callback triggered by AgentLogic when a job is complete."""
        # This could be used for any post-job logic on the server side if needed.
        print("Server notified that job has finished.")

    # --- Helper Methods ---

    def _cleanup_connection(self, conn: socket.socket):
        """Removes a director's connection from the active list."""
        with self.connections_lock:
            if conn in self.director_connections:
                self.director_connections.remove(conn)
        conn.close()

    def _load_config(self, path):
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

if __name__ == '__main__':
    agent_server = AgentServer()
    agent_server.start()
