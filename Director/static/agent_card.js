class AgentCard {
    constructor(agentData, disconnectCallback) {
        this.agentData = agentData;
        this.disconnectCallback = disconnectCallback;
        this.element = this._createCardElement();
        this.update(agentData);
        this._bindEvents();
    }

    _createCardElement() {
        const card = document.createElement('div');
        card.id = `agent-card-${this.agentData.agent_id}`;
        agentStatusContainer.appendChild(card);
        return card;
    }

    update(agentData) {
        this.agentData = agentData;
        const status = this.agentData.status || 'Connecting...';
        this.element.className = `agent-card status-${status.toLowerCase()}`;

        const progressHtml = status === 'Rendering' ? `
            <div class="progress-bar-container">
                <div class="progress-bar" style="width: ${this.agentData.progress * 100}%"></div>
            </div>
            <div class="status-line">Progress: <strong>${(this.agentData.progress * 100).toFixed(1)}%</strong> (Frame: ${this.agentData.current_frame})</div>
        ` : '';

        this.element.innerHTML = `
            <div class="agent-card-header">
                <h3>AGENT: ${this.agentData.agent_id}</h3>
                <div class="status-line"><strong>Status:</strong> <span class="status-text">${status}</span></div>
            </div>
            <div class="agent-card-details">
                <div class="status-line"><strong>IP Address:</strong> ${this.agentData.ip}</div>
                <div class="status-line"><strong>Current Job:</strong> ${this.agentData.job_id || 'N/A'}</div>
                ${progressHtml}
                <button class="btn-disconnect" data-agent-id="${this.agentData.agent_id}">Disconnect</button>
            </div>
        `;
        // Re-bind events after innerHTML is overwritten
        this._bindEvents();
    }

    _bindEvents() {
        const header = this.element.querySelector('.agent-card-header');
        if (header) {
            header.addEventListener('click', () => {
                this.element.classList.toggle('expanded');
            });
        }

        const disconnectBtn = this.element.querySelector('.btn-disconnect');
        if (disconnectBtn) {
            disconnectBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                if (confirm(`Are you sure you want to disconnect agent ${this.agentData.agent_id}?`)) {
                    this.disconnectCallback(this.agentData.agent_id);
                }
            });
        }
    }

    remove() {
        this.element.remove();
    }
}
