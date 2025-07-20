class AgentCard {
    constructor(agentData, disconnectCallback) {
        this.agentData = agentData;
        this.disconnectCallback = disconnectCallback;
        this.expanded = false; // Track expanded state
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
        // Persist expanded state
        const wasExpanded = this.element.classList.contains('expanded') || this.expanded;
        this.element.className = `agent-card status-${status.toLowerCase()}`;
        if (wasExpanded) {
            this.element.classList.add('expanded');
            this.expanded = true;
        } else {
            this.expanded = false;
        }

        // Always show circular progress indicator if progress is available
        let progressBarHtml = '';
        if (status === 'Rendering' && typeof this.agentData.progress === 'number') {
            const percent = Math.max(0, Math.min(100, this.agentData.progress * 100));
            const radius = 32;
            const stroke = 8;
            const normalizedRadius = radius - stroke / 2;
            const circumference = 2 * Math.PI * normalizedRadius;
            const angle = percent * 3.6; // 100% = 360deg
            // SVG arc for pie chart
            const progressArc = percent > 0 ? describeArc(40, 40, 32, 0, angle) : '';
            progressBarHtml = `
                <div class="circular-progress-container">
                    <svg width="80" height="80" class="circular-progress-bg">
                        <circle cx="40" cy="40" r="32" fill="#222" />
                        <path d="${progressArc}" fill="#2ecc71" />
                    </svg>
                    <div class="circular-progress-inner">
                        <span class="circular-progress-label">${percent.toFixed(1)}%</span>
                    </div>
                </div>
            `;
        }

        // Helper for SVG arc
        function describeArc(cx, cy, r, startAngle, endAngle) {
            const start = polarToCartesian(cx, cy, r, endAngle);
            const end = polarToCartesian(cx, cy, r, startAngle);
            const largeArcFlag = endAngle - startAngle <= 180 ? "0" : "1";
            return [
                "M", cx, cy,
                "L", start.x, start.y,
                "A", r, r, 0, largeArcFlag, 0, end.x, end.y,
                "Z"
            ].join(" ");
        }
        function polarToCartesian(cx, cy, r, angleInDegrees) {
            const angleInRadians = (angleInDegrees-90) * Math.PI / 180.0;
            return {
                x: cx + (r * Math.cos(angleInRadians)),
                y: cy + (r * Math.sin(angleInRadians))
            };
        }
        // Show progress details only if rendering and expanded
        let progressDetailsHtml = '';
        if (status === 'Rendering' && wasExpanded) {
            progressDetailsHtml = `
                <div class="status-line">Progress: <strong>${(this.agentData.progress * 100).toFixed(1)}%</strong> (Frame: ${this.agentData.current_frame})</div>
            `;
        }

        // Move circular progress outside of .agent-card-details so it's always visible
        this.element.innerHTML = `
            <div class="agent-card-header">
                <h3>AGENT: ${this.agentData.agent_id}</h3>
                <div class="status-line"><strong>Status:</strong> <span class="status-text">${status}</span></div>
            </div>
            ${progressBarHtml}
            <div class="agent-card-details">
                <div class="status-line"><strong>IP Address:</strong> ${this.agentData.ip}</div>
                <div class="status-line"><strong>Current Job:</strong> ${this.agentData.job_id || 'N/A'}</div>
                ${progressDetailsHtml}
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
                this.expanded = !this.expanded;
                this.element.classList.toggle('expanded', this.expanded);
                // Re-render to show/hide extra details
                this.update(this.agentData);
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
