from flask import Flask, render_template, send_from_directory
from flask_socketio import SocketIO
from director import DirectorLogic
from job_factory import JobFactory

# --- Basic Setup ---
app = Flask(__name__)
app.config['SECRET_KEY'] = 'a_very_secret_key'
socketio = SocketIO(app, async_mode='threading')

# --- Callback Functions for DirectorLogic ---
def on_agent_connected(agent_id, agent_data):
    log_to_ui(f"Successfully connected to agent: {agent_id}")
    on_agent_status_update(agent_id, agent_data)

def on_agent_disconnected(agent_id):
    log_to_ui(f"Agent '{agent_id}' disconnected.")
    socketio.emit('disconnect_agent', {'agent_id': agent_id})
    socketio.emit('update_agent_dropdown', {'all_agents': director_logic.get_all_agents()})

def on_agent_status_update(agent_id, status_data):
    all_agents_status = director_logic.get_all_agents()
    payload = all_agents_status.get(agent_id, {}).copy()
    payload['all_agents'] = all_agents_status
    socketio.emit('agent_update', payload)

def on_queue_update(queue_data):
    """Callback to send the current job queue to all web clients."""
    socketio.emit('queue_update', {'queue': queue_data})

def log_to_ui(message):
    print(message)
    socketio.emit('log_message', {'message': message})

# --- Instantiate the Backend Logic with Callbacks ---
director_event_callbacks = {
    'on_agent_connected': on_agent_connected,
    'on_agent_disconnected': on_agent_disconnected,
    'on_agent_status_update': on_agent_status_update,
    'on_queue_update': on_queue_update # Add the new callback
}
director_logic = DirectorLogic(log_to_ui, director_event_callbacks)
job_factory = JobFactory()

# --- Web Routes ---
@app.route('/')
def index():
    return render_template('director.html')

# --- SocketIO Handlers for Web UI ---
@socketio.on('connect')
def handle_connect():
    print("Web UI client connected.")
    # Send initial state for agents and the queue
    all_agents = director_logic.get_all_agents()
    for agent_id, agent_info in all_agents.items():
        payload = agent_info.copy()
        payload['all_agents'] = all_agents
        socketio.emit('agent_update', payload)
    socketio.emit('queue_update', {'queue': director_logic.get_job_queue()})

@socketio.on('add_agent')
def add_agent(data):
    director_logic.connect_to_agent(data.get('ip'))

@socketio.on('disconnect_agent_request')
def disconnect_agent(data):
    director_logic.disconnect_agent(data.get('agent_id'))

@socketio.on('submit_job')
def submit_job(data):
    """Handles request from UI to add a new job to the queue."""
    form_data = data.get('form_data')
    if not form_data:
        log_to_ui("Error: Missing form data for job submission.")
        return
        
    job_dict = job_factory.create_job_dict(form_data)
    if job_dict:
        # The logic is now to add to the queue, not assign directly.
        director_logic.add_job_to_queue(job_dict)
    else:
        log_to_ui("Error: Failed to create a valid job from the provided parameters.")

@socketio.on('request_agent_list_update')
def request_agent_list():
    all_agents = director_logic.get_all_agents()
    socketio.emit('update_agent_dropdown', {'all_agents': all_agents})

# --- Main Entry Point ---
if __name__ == '__main__':
    print("Starting Director server...")
    socketio.run(app, host='0.0.0.0', port=5000, debug=True, allow_unsafe_werkzeug=True)
