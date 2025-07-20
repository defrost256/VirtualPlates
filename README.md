# VirtualPlates: Unreal Engine 5 Distributed Render Farm

VirtualPlates is a scalable, distributed render farm system designed for automating complex rendering tasks in Unreal Engine 5.6. It enables efficient batch rendering across multiple machines, providing a web-based management interface and robust agent-based job execution.

## Architecture Overview

VirtualPlates is built on a three-tier architecture:

- **Director**: Central Flask web application for job creation, queue management, and real-time monitoring of render nodes.
- **Agent**: Lightweight Python client running on each render node, responsible for job execution and status reporting.
- **Executor**: In-engine Python script for Unreal Engine, directly controlling the Movie Render Graph and scene configuration.

### Data Flow

1. The Director assigns jobs to available Agents via TCP sockets.
2. Agents launch Unreal Engine in headless mode, passing job definitions and progress file paths.
3. The Executor script in Unreal Engine configures the scene, executes the render, and writes progress updates to a file.
4. Agents monitor progress and report status back to the Director, which updates the UI in real time.

## Features

- Web-based UI for job creation, queue management, and agent monitoring
- Real-time status updates via Flask-SocketIO
- Automatic job dispatching to idle agents
- Multi-agent support with persistent reconnection
- Robust error handling and automatic requeueing of failed frames
- Scalable architecture for large render farms

## Dependencies

### Render Node (Agent)

- **Unreal Engine 5.6** (must be installed)
- **Python 3.11+**
- Agent code (see `Agent/` directory)

### Director (Web UI)

- **Python 3.11+**
- **Flask**
- **Flask-SocketIO**
- Director code (see `Director/` directory)

## Installation

### 1. Install Unreal Engine 5.6

Install UE5.6 on each render node and ensure the Python plugin is enabled.

### 2. Set Up Agent

On each render node:
- Clone this repository.
- Edit `Agent/agent_config.json` to set the agent ID, listening port, and path to the Unreal Engine executable.
- Install Python 3.11+.

### 3. Set Up Director

On the machine running the Director UI:
- Clone this repository.
- Install Python 3.11+.
- Install dependencies:
  ```powershell
  pip install flask flask_socketio
  ```
- (Optional) Configure network settings in `Director/` as needed.

## Running the Application

### Start the Director (Web UI)

From the `Director/` directory, run:
```powershell
python director_ui.py
```
Access the web UI from any browser on the network at the displayed address (default: `http://localhost:5000`).

### Start the Agent

On each render node, from the `Agent/` directory, run:
```powershell
python agent.py
```
Agents will automatically connect to the Director and await job assignments.

### Render Workflow

1. Use the Director web UI to create and queue render jobs.
2. Jobs are dispatched to idle agents.
3. Agents launch Unreal Engine with the provided job definition.
4. Progress and completion status are reported back to the Director UI.

## Troubleshooting

- Ensure all machines are on the same network and firewall rules allow TCP communication.
- Check `agent_config.json` for correct paths and settings.
- Review logs in the `Saved/Logs/` directory for error details.

## License

This project is intended for closed-system use. See LICENSE for details.
