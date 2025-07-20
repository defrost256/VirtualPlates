class JobFactoryUI {
    constructor(submitCallback) {
        this.submitCallback = submitCallback;
        this.sequenceTree = document.getElementById('sequence-tree');
        this.scenePresetList = document.getElementById('scene-preset-list');
        this.resolutionPresetList = document.getElementById('resolution-preset-list');
        
        this._bindGlobalEvents();
        this._initializeDefaultPresets();
    }

    _bindGlobalEvents() {
        document.getElementById('btn-submit-job').addEventListener('click', () => {
            const formData = this._gatherFormData();
            this.submitCallback(formData);
        });

        // Tab functionality
        document.querySelectorAll('.tab-link').forEach(button => {
            button.addEventListener('click', (event) => this._openTab(event, button.dataset.tab));
        });

        // Sliders and number inputs link
        document.querySelectorAll('.slider-group').forEach(group => {
            const slider = group.querySelector('.slider');
            const valueInput = group.querySelector('.slider-value');
            slider.oninput = () => valueInput.value = slider.value;
            valueInput.oninput = () => slider.value = valueInput.value;
        });
        
        // Preset add buttons
        document.getElementById('btn-add-sequence').addEventListener('click', () => this.addSequence());
        document.getElementById('btn-add-scene-preset').addEventListener('click', () => {
            const tod = document.getElementById('time_of_day').value;
            const clouds = document.getElementById('cloud_coverage').value;
            this.addScenePreset({ time_of_day: tod, cloud_coverage: clouds });
        });
        document.getElementById('btn-add-resolution-preset').addEventListener('click', () => {
            const resX = document.getElementById('res_x_preset').value;
            const resY = document.getElementById('res_y_preset').value;
            this.addResolutionPreset({ res_x: resX, res_y: resY });
        });
    }
    
    _initializeDefaultPresets() {
        this.addSequence();
        this.addScenePreset({ time_of_day: 1600, cloud_coverage: 0.2 });
        this.addResolutionPreset({ res_x: 1920, res_y: 1080 });
    }

    // --- Sequence Tree Methods ---
    addSequence(path = '/Game/SEQ_MainComp', cameras = ['NewCamera']) {
        const seqId = `seq-${Date.now()}`;
        const seqNode = document.createElement('div');
        seqNode.className = 'preset-item sequence-node';
        seqNode.id = seqId;
        seqNode.innerHTML = `
            <div class="sequence-header">
                <input type="text" class="sequence-path" value="${path}" placeholder="Sequence Path">
                <button class="btn-remove btn-remove-sequence">- Remove Sequence</button>
            </div>
            <div class="camera-list"></div>
            <button class="btn-add-camera">+ Add Camera</button>
        `;
        this.sequenceTree.appendChild(seqNode);
        cameras.forEach(cam => this._addCameraToSequence(seqId, cam));
        this._bindSequenceEvents(seqNode);
    }

    _addCameraToSequence(seqId, name = 'NewCamera') {
        const camId = `cam-${Date.now()}`;
        const camNode = document.createElement('div');
        camNode.className = 'camera-node';
        camNode.id = camId;
        camNode.innerHTML = `
            <input type="text" class="camera-name" value="${name}" placeholder="Camera Name">
            <button class="btn-remove btn-remove-camera">-</button>
        `;
        document.querySelector(`#${seqId} .camera-list`).appendChild(camNode);
        camNode.querySelector('.btn-remove-camera').addEventListener('click', () => camNode.remove());
    }

    _bindSequenceEvents(seqNode) {
        seqNode.querySelector('.btn-remove-sequence').addEventListener('click', () => seqNode.remove());
        seqNode.querySelector('.btn-add-camera').addEventListener('click', () => this._addCameraToSequence(seqNode.id));
    }

    // --- Preset List Methods ---
    addScenePreset(settings, enabled = true) {
        const presetId = `scene-preset-${Date.now()}`;
        const label = `Time: ${settings.time_of_day}, Clouds: ${settings.cloud_coverage}`;
        const node = this._createPresetNode(presetId, label, enabled);
        node.dataset.settings = JSON.stringify(settings);
        this.scenePresetList.appendChild(node);
    }

    addResolutionPreset(res, enabled = true) {
        const presetId = `res-preset-${Date.now()}`;
        const label = `${res.res_x} x ${res.res_y}`;
        const node = this._createPresetNode(presetId, label, enabled);
        node.dataset.resolution = JSON.stringify(res);
        this.resolutionPresetList.appendChild(node);
    }

    _createPresetNode(id, label, enabled) {
        const presetNode = document.createElement('div');
        presetNode.className = 'preset-item';
        presetNode.id = id;
        presetNode.innerHTML = `
            <input type="checkbox" class="preset-enabled" ${enabled ? 'checked' : ''}>
            <span class="preset-label">${label}</span>
            <button class="btn-remove">-</button>
        `;
        presetNode.querySelector('.btn-remove').addEventListener('click', () => presetNode.remove());
        return presetNode;
    }
    
    // --- Data Gathering ---
    _gatherFormData() {
        const sequences = Array.from(document.querySelectorAll('.sequence-node')).map(seqNode => ({
            path: seqNode.querySelector('.sequence-path').value,
            cameras: Array.from(seqNode.querySelectorAll('.camera-name')).map(camInput => camInput.value)
        }));

        const scene_presets = Array.from(document.querySelectorAll('#scene-preset-list .preset-item')).map(pNode => ({
            enabled: pNode.querySelector('.preset-enabled').checked,
            settings: JSON.parse(pNode.dataset.settings)
        }));

        const resolution_presets = Array.from(document.querySelectorAll('#resolution-preset-list .preset-item')).map(pNode => ({
            enabled: pNode.querySelector('.preset-enabled').checked,
            ...JSON.parse(pNode.dataset.resolution)
        }));

        return {
            project_path: document.getElementById('project_path').value,
            graph_path: document.getElementById('graph_path').value,
            level_path: document.getElementById('level_path').value,
            sequences,
            scene_presets,
            resolution_presets,
        };
    }
    
    // --- UI Helpers ---
    _openTab(evt, tabName) {
        document.querySelectorAll('.tab-content').forEach(tab => tab.style.display = "none");
        document.querySelectorAll('.tab-link').forEach(link => link.classList.remove("active"));
        document.getElementById(tabName).style.display = "block";
        evt.currentTarget.classList.add("active");
    }
}
