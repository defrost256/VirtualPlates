import socket
import threading
import json
import os
from collections import deque

AGENTS_SAVE_FILE = 'director_agents.json'

class DirectorLogic:
    """
    Handles all the backend logic for the Director, including state management,
    a job queue, and communication with render agents.
    """
    def __init__(self, log_callback, event_callbacks):
        self.log = log_callback
        self.events = event_callbacks
        self.agents = {}
        self.job_queue = deque() # Using deque for an efficient queue
        self.agents_lock = threading.Lock()
        self._load_and_connect_agents()

    # --- Public Methods ---

    def get_all_agents(self):
        with self.agents_lock:
            return {agent_id: data['public'] for agent_id, data in self.agents.items()}

    def get_job_queue(self):
        with self.agents_lock:
            return list(self.job_queue)

    def add_job_to_queue(self, job_dict):
        """Adds a new job to the queue and tries to dispatch it."""
        with self.agents_lock:
            self.job_queue.append(job_dict)
            self.log(f"Job '{job_dict['job_id']}' added to the queue. Queue size: {len(self.job_queue)}")
        
        # Notify UI about the queue change and then try to assign jobs.
        self.events['on_queue_update'](list(self.job_queue))
        self._check_queue_and_assign_jobs()

    def connect_to_agent(self, ip_port_str):
        if not ip_port_str: return
        self.log(f"UI requested connection to agent: {ip_port_str}")
        self._add_agent_to_save_file(ip_port_str)
        threading.Thread(target=self._handle_agent_connection, args=(ip_port_str,)).start()

    def disconnect_agent(self, agent_id):
        """Forcibly disconnects an agent and removes it from the save file."""
        with self.agents_lock:
            agent_info = self.agents.get(agent_id)
            if agent_info:
                self.log(f"Forcibly disconnecting agent: {agent_id}")
                try:
                    agent_info['internal']['socket'].shutdown(socket.SHUT_RDWR)
                    agent_info['internal']['socket'].close()
                except socket.error as e:
                    self.log(f"Note: Socket error during manual disconnect (may already be closed): {e}")
                
                self._remove_agent_from_save_file(agent_info['public']['ip'])
            else:
                self.log(f"Could not disconnect: Agent {agent_id} not found.")

    # --- Internal Logic ---

    def _check_queue_and_assign_jobs(self):
        """Finds idle agents and assigns them jobs from the queue."""
        with self.agents_lock:
            if not self.job_queue:
                return # Nothing to do if queue is empty

            # Find all idle agents
            idle_agents = [
                agent_id for agent_id, data in self.agents.items()
                if data['public'].get('status') == 'Idle'
            ]

            for agent_id in idle_agents:
                if not self.job_queue:
                    break # Stop if we run out of jobs
                
                job_to_assign = self.job_queue.popleft()
                self.log(f"Found idle agent '{agent_id}'. Assigning job '{job_to_assign['job_id']}'.")
                
                # We need to call the actual socket send in a new thread
                # to avoid holding the lock during a network operation.
                threading.Thread(target=self._send_job_to_agent, args=(agent_id, job_to_assign)).start()
        
        # After assignments, notify UI of the queue change
        self.events['on_queue_update'](list(self.job_queue))

    def _send_job_to_agent(self, agent_id, job_dict):
        """Sends a job dictionary to a specific agent's socket."""
        try:
            with self.agents_lock:
                # Re-fetch agent info inside the thread to ensure it's still valid
                agent_info = self.agents.get(agent_id)
                if not agent_info:
                    raise ConnectionError("Agent disconnected before job could be sent.")
                agent_socket = agent_info['internal']['socket']

            job_data_str = json.dumps(job_dict)
            agent_socket.sendall(job_data_str.encode('utf-8'))
            self.log(f"Successfully sent job '{job_dict['job_id']}' to agent '{agent_id}'.")
        except (socket.error, ConnectionError) as e:
            self.log(f"Error sending job to agent '{agent_id}': {e}. Re-queuing job.")
            # If sending fails, put the job back at the front of the queue
            with self.agents_lock:
                self.job_queue.appendleft(job_dict)
            self.events['on_queue_update'](list(self.job_queue))

    def _handle_agent_connection(self, ip_port_str):
        # ... (This function remains the same as the previous version) ...
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
                self.agents[agent_id] = {
                    'internal': {'socket': sock},
                    'public': { 'agent_id': agent_id, 'ip': ip_port_str }
                }
                self.agents[agent_id]['public'].update(initial_status)
            
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
        is_now_idle = False
        with self.agents_lock:
            if agent_id in self.agents:
                old_status = self.agents[agent_id]['public'].get('status')
                new_status = status_data.get('status')
                
                if new_status == 'Idle' and old_status != 'Idle':
                    is_now_idle = True

                if new_status == 'Completed':
                    job_id = self.agents[agent_id]['public'].get('job_id')
                    if job_id: self.log(f"Job '{job_id}' on agent '{agent_id}' completed successfully.")
                
                self.agents[agent_id]['public'].update(status_data)
        
        self.events['on_agent_status_update'](agent_id, status_data)
        
        # If an agent just became idle, check if there's work for it.
        if is_now_idle:
            self.log(f"Agent '{agent_id}' is now idle. Checking job queue...")
            self._check_queue_and_assign_jobs()

    def _load_and_connect_agents(self):
        try:
            if os.path.exists(AGENTS_SAVE_FILE):
                with open(AGENTS_SAVE_FILE, 'r') as f:
                    saved_agents = json.load(f)
                self.log(f"Loaded {len(saved_agents)} agents from save file.")
                for ip_port in saved_agents:
                    self.connect_to_agent(ip_port) # connect_to_agent is already thread-safe
        except (IOError, json.JSONDecodeError) as e:
            self.log(f"Could not load agent save file: {e}")

    def _add_agent_to_save_file(self, ip_port_str):
        saved_agents = []
        if os.path.exists(AGENTS_SAVE_FILE):
            try:
                with open(AGENTS_SAVE_FILE, 'r') as f:
                    saved_agents = json.load(f)
            except (IOError, json.JSONDecodeError): pass

        if ip_port_str not in saved_agents:
            saved_agents.append(ip_port_str)
            try:
                with open(AGENTS_SAVE_FILE, 'w') as f:
                    json.dump(saved_agents, f, indent=4)
                self.log(f"Saved new agent '{ip_port_str}' to file.")
            except IOError as e:
                self.log(f"Error saving agent list: {e}")

    def _remove_agent_from_save_file(self, ip_port_str):
        saved_agents = []
        if os.path.exists(AGENTS_SAVE_FILE):
            try:
                with open(AGENTS_SAVE_FILE, 'r') as f:
                    saved_agents = json.load(f)
                if ip_port_str in saved_agents:
                    saved_agents.remove(ip_port_str)
                    with open(AGENTS_SAVE_FILE, 'w') as f:
                        json.dump(saved_agents, f, indent=4)
                    self.log(f"Removed agent '{ip_port_str}' from save file.")
            except (IOError, json.JSONDecodeError) as e:
                self.log(f"Error updating agent save file: {e}")
