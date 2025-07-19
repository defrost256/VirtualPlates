from flask import Flask, render_template, send_from_directory
from flask_socketio import SocketIO
from director import DirectorLogic
from job_factory import JobFactory

# --- Basic Setup ---
app = Flask(__name__)
app.config['SECRET_KEY'] = 'a_very_secret_key'
socketio = SocketIO(app, async_mode='threading')

# --- Callbacks and Instances ---
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

def log_to_ui(message):
    print(message)
    socketio.emit('log_message', {'message': message})

director_event_callbacks = {
    'on_agent_connected': on_agent_connected,
    'on_agent_disconnected': on_agent_disconnected,
    'on_agent_status_update': on_agent_status_update,
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
    all_agents = director_logic.get_all_agents()
    for agent_id, agent_info in all_agents.items():
        payload = agent_info.copy()
        payload['all_agents'] = all_agents
        socketio.emit('agent_update', payload)

@socketio.on('add_agent')
def add_agent(data):
    director_logic.connect_to_agent(data.get('ip'))

@socketio.on('disconnect_agent_request')
def disconnect_agent(data):
    director_logic.disconnect_agent(data.get('agent_id'))

@socketio.on('submit_job')
def submit_job(data):
    agent_id = data.get('agent_id')
    form_data = data.get('form_data')
    if not agent_id or not form_data:
        log_to_ui("Error: Missing agent ID or form data for job submission.")
        return
        
    job_dict = job_factory.create_job_dict(form_data)
    if job_dict:
        director_logic.submit_job_to_agent(agent_id, job_dict)
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
