import socket
import threading
import json
import os

class DirectorLogic:
    """
    Handles all the backend logic for the Director, including state management
    and communication with render agents. This class is UI-agnostic.
    """
    def __init__(self, log_callback, event_callbacks):
        """
        Initializes the logic controller.
        :param log_callback: A function to call for logging messages.
        :param event_callbacks: A dictionary of functions for agent events.
        """
        self.log = log_callback
        self.events = event_callbacks
        self.agents = {}
        self.agents_lock = threading.Lock()

    # --- Methods Called by the UI Controller ---

    def get_all_agents(self):
        """
        Returns a dictionary of public-facing status data for all agents.
        This is safe to send to the UI.
        """
        with self.agents_lock:
            return {agent_id: data['public'] for agent_id, data in self.agents.items()}

    def connect_to_agent(self, ip_port_str):
        """Starts a new thread to manage the connection to an agent."""
        if not ip_port_str:
            return
        self.log(f"UI requested connection to agent: {ip_port_str}")
        threading.Thread(target=self._handle_agent_connection, args=(ip_port_str,)).start()

    def submit_job_to_agent(self, agent_id, job_path):
        """Validates and sends a job file to a specified agent."""
        with self.agents_lock:
            agent_info = self.agents.get(agent_id)
            if not agent_info or agent_info['public'].get('status') == 'Disconnected':
                self.log(f"Error: Agent '{agent_id}' not connected or found.")
                return
            if agent_info['public'].get('status') != 'Idle':
                self.log(f"Error: Agent '{agent_id}' is not Idle and cannot accept new jobs.")
                return
        try:
            with open(job_path, 'r') as f:
                job_data_str = f.read()
                json.loads(job_data_str)  # Quick validation

            agent_socket = agent_info['internal']['socket']
            agent_socket.sendall(job_data_str.encode('utf-8'))
            self.log(f"Successfully sent job '{os.path.basename(job_path)}' to agent '{agent_id}'.")
        except FileNotFoundError:
            self.log(f"Error: Job JSON file not found at '{job_path}'.")
        except json.JSONDecodeError:
            self.log(f"Error: Job file at '{job_path}' is not valid JSON.")
        except socket.error as e:
            self.log(f"Error sending job to agent '{agent_id}': {e}")

    # --- Internal Agent Communication and State Management ---

    def _handle_agent_connection(self, ip_port_str):
        """The core logic for a single agent connection lifecycle."""
        try:
            host, port = ip_port_str.split(':')
            port = int(port)
        except ValueError:
            self.log(f"Invalid agent address format: '{ip_port_str}'. Use HOST:PORT.")
            return

        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        agent_id = None
        try:
            self.log(f"Connecting to agent at {host}:{port}...")
            sock.connect((host, port))
            
            initial_data = sock.recv(4096).decode('utf-8').strip()
            initial_status = json.loads(initial_data.split('\n')[0])
            agent_id = initial_status.get('agent_id', ip_port_str)

            with self.agents_lock:
                # Create the agent's data structure, populating the public
                # part directly with the status received from the agent.
                self.agents[agent_id] = {
                    'internal': {'socket': sock},
                    'public': {
                        'agent_id': agent_id,
                        'ip': ip_port_str
                    }
                }
                # Merge the full initial status into the public data.
                self.agents[agent_id]['public'].update(initial_status)
            
            # Use the callback to notify the UI of the connection and the correct initial state.
            self.events['on_agent_connected'](agent_id, self.agents[agent_id]['public'])
            
            buffer = ""
            while True:
                data = sock.recv(1024).decode('utf-8')
                if not data: break
                
                buffer += data
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    if line:
                        status_update = json.loads(line)
                        self._update_agent_state(agent_id, status_update)
                    
        except socket.error as e:
            self.log(f"Connection error with agent '{agent_id or ip_port_str}': {e}")
        except (json.JSONDecodeError, IndexError) as e:
            self.log(f"Invalid data from agent '{agent_id or ip_port_str}': {e}")
        finally:
            sock.close()
            if agent_id:
                with self.agents_lock:
                    if agent_id in self.agents:
                        del self.agents[agent_id]
                self.events['on_agent_disconnected'](agent_id)

    def _update_agent_state(self, agent_id, status_data):
        """Internal method to update state and trigger the UI callback."""
        with self.agents_lock:
            if agent_id in self.agents:
                self.agents[agent_id]['public'].update(status_data)
        
        self.events['on_agent_status_update'](agent_id, status_data)
