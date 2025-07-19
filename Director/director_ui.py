from flask import Flask, render_template, send_from_directory
from flask_socketio import SocketIO
from director import DirectorLogic

# --- Basic Setup ---
app = Flask(__name__)
app.config['SECRET_KEY'] = 'a_very_secret_key'
socketio = SocketIO(app, async_mode='threading')

# --- Callback Functions for DirectorLogic ---

def on_agent_connected(agent_id, agent_data):
    """Callback triggered when an agent is successfully connected."""
    log_to_ui(f"Successfully connected to agent: {agent_id}")
    on_agent_status_update(agent_id, agent_data)

def on_agent_disconnected(agent_id):
    """Callback triggered when an agent disconnects."""
    log_to_ui(f"Agent '{agent_id}' disconnected.")
    socketio.emit('disconnect_agent', {'agent_id': agent_id})
    # The lock is no longer needed here, as get_all_agents handles it.
    socketio.emit('update_agent_dropdown', {'all_agents': director_logic.get_all_agents()})

def on_agent_status_update(agent_id, status_data):
    """Callback triggered on any status update from an agent."""
    # The lock is no longer needed here.
    all_agents_status = director_logic.get_all_agents()
    
    # Create a shallow copy of the specific agent's data to avoid a circular reference.
    payload = all_agents_status.get(agent_id, {}).copy()
    
    # Add the complete list of agents to the payload for the UI to update the dropdown.
    payload['all_agents'] = all_agents_status
    socketio.emit('agent_update', payload)

def log_to_ui(message):
    """Callback to send a log message to all web clients."""
    print(message)
    socketio.emit('log_message', {'message': message})

# --- Instantiate the Backend Logic with Callbacks ---
director_event_callbacks = {
    'on_agent_connected': on_agent_connected,
    'on_agent_disconnected': on_agent_disconnected,
    'on_agent_status_update': on_agent_status_update,
}
director_logic = DirectorLogic(log_to_ui, director_event_callbacks)


# --- Web Routes ---
@app.route('/')
def index():
    return render_template('director.html')

@app.route('/static/<path:path>')
def send_static(path):
    return send_from_directory('static', path)


# --- SocketIO Handlers for Web UI ---
@socketio.on('connect')
def handle_connect():
    print("Web UI client connected.")
    all_agents = director_logic.get_all_agents()
    for agent_id, agent_info in all_agents.items():
        payload = agent_info.copy()
        payload['all_agents'] = all_agents
        socketio.emit('agent_update', payload)

@socketio.on('add_agent')
def add_agent(data):
    ip_port = data.get('ip')
    director_logic.connect_to_agent(ip_port)

@socketio.on('submit_job')
def submit_job(data):
    agent_id = data.get('agent_id')
    job_path = data.get('job_path')
    director_logic.submit_job_to_agent(agent_id, job_path)

@socketio.on('request_agent_list_update')
def request_agent_list():
    all_agents = director_logic.get_all_agents()
    socketio.emit('update_agent_dropdown', {'all_agents': all_agents})


# --- Main Entry Point ---
if __name__ == '__main__':
    print("Starting Director server...")
    socketio.run(app, host='0.0.0.0', port=5000, debug=True, allow_unsafe_werkzeug=True)
