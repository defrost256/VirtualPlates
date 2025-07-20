// This script should be loaded after agent_card.js and job_factory.js

document.addEventListener('DOMContentLoaded', () => {
    const socket = io();

    // --- Global UI Elements ---
    const logOutput = document.getElementById('log-output');
    const addAgentBtn = document.getElementById('btn-add-agent');
    const agentIpInput = document.getElementById('agent-ip');
    const jobQueueList = document.getElementById('job-queue-list');
    window.agentStatusContainer = document.getElementById('agent-status-container'); // Make global for AgentCard

    // --- State Management ---
    let agentCards = {}; // Stores AgentCard instances

    // --- Component Initialization ---
    const jobFactory = new JobFactoryUI((formData) => {
        socket.emit('submit_job', { form_data: formData });
    });

    // --- Socket.IO Event Handlers ---
    socket.on('connect', () => addLogMessage('Successfully connected to Director backend.'));
    socket.on('log_message', (data) => addLogMessage(data.message));

    socket.on('agent_update', (data) => {
        const agentId = data.agent_id;
        if (agentCards[agentId]) {
            agentCards[agentId].update(data);
        } else {
            agentCards[agentId] = new AgentCard(data, (id) => {
                socket.emit('disconnect_agent_request', { agent_id: id });
            });
        }
    });

    socket.on('disconnect_agent', (data) => {
        const agentId = data.agent_id;
        if (agentCards[agentId]) {
            agentCards[agentId].remove();
            delete agentCards[agentId];
        }
    });

    socket.on('queue_update', (data) => {
        updateJobQueue(data.queue);
    });

    // --- Global Event Listeners ---
    addAgentBtn.addEventListener('click', () => {
        const ip = agentIpInput.value;
        if (ip) {
            socket.emit('add_agent', { ip: ip });
        }
    });

    // --- Helper Functions ---
    function addLogMessage(message) {
        const time = new Date().toLocaleTimeString();
        logOutput.innerHTML += `<div>[${time}] ${message}</div>`;
        logOutput.scrollTop = logOutput.scrollHeight;
    }

    function updateJobQueue(queue) {
        jobQueueList.innerHTML = '';
        if (!queue || queue.length === 0) {
            jobQueueList.innerHTML = '<div class="queue-empty-message">The job queue is empty.</div>';
            return;
        }
        queue.forEach(job => {
            const jobItem = document.createElement('div');
            jobItem.className = 'job-queue-item';
            jobItem.textContent = `Queued: ${job.job_id}`;
            jobQueueList.appendChild(jobItem);
        });
    }
});
