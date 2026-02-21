/**
 * SWG Terrain Viewer
 * 
 * Full-page Three.js terrain viewer for Star Wars Galaxies
 * Features:
 *   - Terrain rendering with height maps
 *   - Shader-based terrain texturing
 *   - Buildout object placement
 *   - Player building markers (from database)
 *   - Navigation controls with home button
 *   - Render distance slider
 *   - Camera panning and anchoring
 *   - Legend panel
 *   - Live tracking support
 */

class SWGTerrainViewer {
    constructor(container, options = {}) {
        this.container = typeof container === 'string'
            ? document.getElementById(container)
            : container;

        this.options = {
            // Scene configuration
            sceneName: options.sceneName || 'tatooine',
            mapSize: options.mapSize || 16384,
            chunkSize: options.chunkSize || 256,
            
            // Visual options
            backgroundColor: options.backgroundColor || 0x87ceeb,
            terrainColor: options.terrainColor || 0xc2a366,
            waterColor: options.waterColor || 0x4080ff,
            waterHeight: options.waterHeight || 0,
            
            // Camera options
            initialPosition: options.initialPosition || { x: 0, y: 500, z: 0 },
            minHeight: options.minHeight || 10,
            maxHeight: options.maxHeight || 20000,

            // Features
            showGrid: options.showGrid !== false,
            showMinimap: options.showMinimap !== false,
            showLegend: options.showLegend !== false,
            showBuildouts: options.showBuildouts !== false,
            showPlayerBuildings: options.showPlayerBuildings !== false,
            
            // Render distance settings
            minRenderDistance: options.minRenderDistance || 1000,
            maxRenderDistance: options.maxRenderDistance || 50000,
            viewDistance: options.viewDistance || 20000,

            // LOD settings
            lodLevels: options.lodLevels || 4,

            // Update settings
            updateInterval: options.updateInterval || 30000, // 30 seconds
            
            ...options
        };


        // Core objects
        this.scene = null;
        this.camera = null;
        this.renderer = null;
        this.controls = null;
        this.animationId = null;
        
        // Terrain data (1:1 C++ accurate)
        this.terrainData = null;
        this.heightMap = null;
        this.terrainMesh = null;
        this.terrainChunks = new Map();
        this.terrainTextures = [];
        this.shaderFamilies = [];
        this.floraFamilies = [];
        
        // Environment data
        this.environments = [];
        this.currentEnvironment = null;

        // Objects
        this.buildoutObjects = [];
        this.playerBuildings = [];
        this.objectMarkers = new Map();
        this.floraInstances = [];
        
        // UI elements
        this.uiContainer = null;
        this.minimapCanvas = null;
        this.legendPanel = null;
        this.renderDistanceSlider = null;

        // State
        this.isLoading = false;
        this.loadingProgress = 0;
        this.selectedObject = null;
        this.hoveredObject = null;
        this.anchorPoint = new THREE.Vector3(0, 0, 0);
        
        // Audio state
        this.audioEnabled = false;
        this.audioContext = null;
        this.currentMusic = null;
        this.currentAmbient1 = null;
        this.currentAmbient2 = null;
        this.musicVolume = 0.5;
        this.ambientVolume = 0.7;

        // Event handlers
        this.raycaster = new THREE.Raycaster();
        this.mouse = new THREE.Vector2();
        
        console.log('[SWGTerrainViewer] Initializing terrain viewer');
        this.init();
    }

    init() {
        if (typeof THREE === 'undefined') {
            console.error('[SWGTerrainViewer] Three.js not loaded');
            this.showError('Three.js library not loaded');
            return;
        }

        // Create main container structure
        this.createUIStructure();
        
        // Initialize Three.js scene
        this.initScene();
        
        // Initialize audio system
        this.initAudio();
        
        // Setup event listeners
        this.setupEventListeners();
        
        // Start animation loop
        this.animate();
        
        console.log('[SWGTerrainViewer] Viewer initialized');
    }

    createUIStructure() {
        // Clear container
        this.container.innerHTML = '';
        this.container.style.position = 'relative';
        this.container.style.overflow = 'hidden';
        
        // Create canvas container
        this.canvasContainer = document.createElement('div');
        this.canvasContainer.style.cssText = 'position: absolute; inset: 0;';
        this.container.appendChild(this.canvasContainer);
        
        // Create UI overlay container
        this.uiContainer = document.createElement('div');
        this.uiContainer.style.cssText = 'position: absolute; inset: 0; pointer-events: none;';
        this.container.appendChild(this.uiContainer);
        
        // Create controls panel
        this.createControlsPanel();
        
        // Create minimap
        if (this.options.showMinimap) {
            this.createMinimap();
        }
        
        // Create legend
        if (this.options.showLegend) {
            this.createLegend();
        }
        
        // Create loading overlay
        this.createLoadingOverlay();
        
        // Create info panel
        this.createInfoPanel();

        // Create camera info panel
        this.createCameraInfoPanel();
    }

    createControlsPanel() {
        const panel = document.createElement('div');
        panel.style.cssText = `
            position: absolute;
            top: 10px;
            left: 10px;
            display: flex;
            flex-direction: column;
            gap: 5px;
            pointer-events: auto;
            max-width: 200px;
        `;
        
        // Home button
        const homeBtn = this.createButton('🏠 Home', () => this.goHome());
        homeBtn.title = 'Return to center of map';
        panel.appendChild(homeBtn);
        
        // Zoom controls
        const zoomInBtn = this.createButton('+', () => this.zoomIn());
        zoomInBtn.title = 'Zoom in';
        panel.appendChild(zoomInBtn);
        
        const zoomOutBtn = this.createButton('-', () => this.zoomOut());
        zoomOutBtn.title = 'Zoom out';
        panel.appendChild(zoomOutBtn);
        
        // Anchor mode toggle
        const anchorBtn = this.createButton('⚓ Anchor', () => this.toggleAnchorMode());
        anchorBtn.title = 'Toggle anchor mode (click terrain to set rotation point)';
        anchorBtn.id = 'anchorModeBtn';
        panel.appendChild(anchorBtn);

        // Toggle buttons
        const gridBtn = this.createButton('⊞ Grid', () => this.toggleGrid());
        gridBtn.title = 'Toggle grid overlay';
        panel.appendChild(gridBtn);
        
        const buildoutsBtn = this.createButton('🏛️ Buildouts', () => this.toggleBuildouts());
        buildoutsBtn.title = 'Toggle buildout visibility';
        panel.appendChild(buildoutsBtn);
        
        const playersBtn = this.createButton('🏠 Players', () => this.togglePlayerBuildings());
        playersBtn.title = 'Toggle player buildings';
        panel.appendChild(playersBtn);
        
        // Render distance slider
        const sliderContainer = document.createElement('div');
        sliderContainer.style.cssText = `
            background: rgba(30, 30, 40, 0.9);
            padding: 10px;
            border-radius: 4px;
            border: 1px solid #444;
            margin-top: 5px;
        `;

        const sliderLabel = document.createElement('div');
        sliderLabel.style.cssText = 'color: #fff; font-size: 12px; margin-bottom: 5px;';
        sliderLabel.innerHTML = 'Render Distance: <span id="renderDistValue">' + this.options.viewDistance + '</span>m';
        sliderContainer.appendChild(sliderLabel);

        this.renderDistanceSlider = document.createElement('input');
        this.renderDistanceSlider.type = 'range';
        this.renderDistanceSlider.min = this.options.minRenderDistance;
        this.renderDistanceSlider.max = this.options.maxRenderDistance;
        this.renderDistanceSlider.value = this.options.viewDistance;
        this.renderDistanceSlider.style.cssText = 'width: 100%; cursor: pointer;';
        this.renderDistanceSlider.addEventListener('input', (e) => this.setRenderDistance(parseInt(e.target.value)));
        sliderContainer.appendChild(this.renderDistanceSlider);

        // Quick distance buttons
        const distBtnContainer = document.createElement('div');
        distBtnContainer.style.cssText = 'display: flex; gap: 4px; margin-top: 5px;';

        [5000, 15000, 30000, 50000].forEach(dist => {
            const btn = document.createElement('button');
            btn.textContent = dist >= 1000 ? `${dist/1000}k` : dist;
            btn.style.cssText = `
                flex: 1;
                padding: 4px;
                font-size: 10px;
                background: rgba(60, 60, 80, 0.9);
                border: 1px solid #555;
                border-radius: 3px;
                color: #fff;
                cursor: pointer;
            `;
            btn.addEventListener('click', () => this.setRenderDistance(dist));
            distBtnContainer.appendChild(btn);
        });
        sliderContainer.appendChild(distBtnContainer);

        panel.appendChild(sliderContainer);

        // Scene selector
        const sceneSelect = document.createElement('select');
        sceneSelect.style.cssText = `
            padding: 8px;
            border-radius: 4px;
            border: 1px solid #444;
            background: rgba(30, 30, 40, 0.9);
            color: #fff;
            cursor: pointer;
            font-size: 12px;
        `;
        
        const scenes = [
            'corellia', 'dantooine', 'dathomir', 'endor', 'lok',
            'naboo', 'rori', 'talus', 'tatooine', 'yavin4',
            'kashyyyk_main', 'mustafar'
        ];
        
        scenes.forEach(scene => {
            const option = document.createElement('option');
            option.value = scene;
            option.textContent = scene.charAt(0).toUpperCase() + scene.slice(1).replace('_', ' ');
            sceneSelect.appendChild(option);
        });
        
        sceneSelect.value = this.options.sceneName;
        sceneSelect.addEventListener('change', (e) => this.loadScene(e.target.value));
        panel.appendChild(sceneSelect);
        
        this.uiContainer.appendChild(panel);
    }

    createButton(text, onClick) {
        const btn = document.createElement('button');
        btn.textContent = text;
        btn.style.cssText = `
            padding: 8px 12px;
            border-radius: 4px;
            border: 1px solid #444;
            background: rgba(30, 30, 40, 0.9);
            color: #fff;
            cursor: pointer;
            font-size: 14px;
            transition: background 0.2s;
            min-width: 80px;
        `;
        btn.addEventListener('mouseover', () => btn.style.background = 'rgba(60, 60, 80, 0.9)');
        btn.addEventListener('mouseout', () => btn.style.background = 'rgba(30, 30, 40, 0.9)');
        btn.addEventListener('click', onClick);
        return btn;
    }

    createMinimap() {
        const minimapContainer = document.createElement('div');
        minimapContainer.style.cssText = `
            position: absolute;
            bottom: 10px;
            right: 10px;
            width: 200px;
            height: 200px;
            background: rgba(20, 20, 30, 0.9);
            border: 2px solid #444;
            border-radius: 8px;
            overflow: hidden;
            pointer-events: auto;
        `;
        
        this.minimapCanvas = document.createElement('canvas');
        this.minimapCanvas.width = 200;
        this.minimapCanvas.height = 200;
        this.minimapCanvas.style.cssText = 'width: 100%; height: 100%;';
        minimapContainer.appendChild(this.minimapCanvas);
        
        // Minimap click handler
        minimapContainer.addEventListener('click', (e) => {
            const rect = minimapContainer.getBoundingClientRect();
            const x = (e.clientX - rect.left) / rect.width;
            const y = (e.clientY - rect.top) / rect.height;
            
            const halfSize = this.options.mapSize / 2;
            const worldX = (x - 0.5) * this.options.mapSize;
            const worldZ = (y - 0.5) * this.options.mapSize;
            
            this.goToPosition(worldX, worldZ);
        });
        
        this.uiContainer.appendChild(minimapContainer);
    }

    createLegend() {
        this.legendPanel = document.createElement('div');
        this.legendPanel.style.cssText = `
            position: absolute;
            bottom: 10px;
            left: 10px;
            padding: 10px;
            background: rgba(20, 20, 30, 0.9);
            border: 1px solid #444;
            border-radius: 8px;
            color: #fff;
            font-size: 12px;
            pointer-events: auto;
            max-height: 300px;
            overflow-y: auto;
        `;
        
        this.legendPanel.innerHTML = `
            <div style="font-weight: bold; margin-bottom: 8px;">Legend</div>
            <div style="display: flex; align-items: center; margin: 4px 0;">
                <span style="display: inline-block; width: 16px; height: 16px; background: #4a90d9; border-radius: 50%; margin-right: 8px;"></span>
                <span>Buildout Areas</span>
            </div>
            <div style="display: flex; align-items: center; margin: 4px 0;">
                <span style="display: inline-block; width: 16px; height: 16px; background: #6a4; border-radius: 50%; margin-right: 8px;"></span>
                <span>Player Buildings</span>
            </div>
            <div style="display: flex; align-items: center; margin: 4px 0;">
                <span style="display: inline-block; width: 16px; height: 16px; background: #d94; border-radius: 50%; margin-right: 8px;"></span>
                <span>NPC Cities</span>
            </div>
            <div style="display: flex; align-items: center; margin: 4px 0;">
                <span style="display: inline-block; width: 16px; height: 16px; background: #94d; border-radius: 50%; margin-right: 8px;"></span>
                <span>Points of Interest</span>
            </div>
        `;
        
        this.uiContainer.appendChild(this.legendPanel);
    }

    createLoadingOverlay() {
        this.loadingOverlay = document.createElement('div');
        this.loadingOverlay.style.cssText = `
            position: absolute;
            inset: 0;
            background: rgba(0, 0, 0, 0.8);
            display: flex;
            flex-direction: column;
            justify-content: center;
            align-items: center;
            color: #fff;
            font-size: 18px;
            z-index: 1000;
            display: none;
        `;
        
        this.loadingOverlay.innerHTML = `
            <div style="margin-bottom: 20px;">Loading terrain...</div>
            <div style="width: 200px; height: 20px; background: #333; border-radius: 10px; overflow: hidden;">
                <div class="loading-progress" style="width: 0%; height: 100%; background: linear-gradient(90deg, #4a90d9, #6ad); transition: width 0.3s;"></div>
            </div>
            <div class="loading-text" style="margin-top: 10px; font-size: 14px;">Initializing...</div>
        `;
        
        this.uiContainer.appendChild(this.loadingOverlay);
    }

    createInfoPanel() {
        this.infoPanel = document.createElement('div');
        this.infoPanel.style.cssText = `
            position: absolute;
            top: 10px;
            right: 10px;
            padding: 10px 15px;
            background: rgba(20, 20, 30, 0.9);
            border: 1px solid #444;
            border-radius: 8px;
            color: #fff;
            font-size: 12px;
            pointer-events: auto;
            min-width: 150px;
            display: none;
        `;
        
        this.uiContainer.appendChild(this.infoPanel);
    }

    createCameraInfoPanel() {
        this.cameraInfoPanel = document.createElement('div');
        this.cameraInfoPanel.style.cssText = `
            position: absolute;
            bottom: 220px;
            right: 10px;
            padding: 8px 12px;
            background: rgba(20, 20, 30, 0.9);
            border: 1px solid #444;
            border-radius: 8px;
            color: #fff;
            font-size: 11px;
            pointer-events: none;
            font-family: monospace;
        `;
        this.uiContainer.appendChild(this.cameraInfoPanel);
    }

    initScene() {
        // Scene
        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(this.options.backgroundColor);

        // Camera
        const width = this.container.clientWidth || window.innerWidth;
        const height = this.container.clientHeight || window.innerHeight;
        
        // Use larger far plane for full render distance
        this.camera = new THREE.PerspectiveCamera(60, width / height, 1, this.options.maxRenderDistance * 1.5);
        this.camera.position.set(
            this.options.initialPosition.x,
            this.options.initialPosition.y,
            this.options.initialPosition.z + 500
        );
        
        // Renderer
        this.renderer = new THREE.WebGLRenderer({ antialias: true, logarithmicDepthBuffer: true });
        this.renderer.setSize(width, height);
        this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
        this.renderer.shadowMap.enabled = true;
        this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
        this.canvasContainer.appendChild(this.renderer.domElement);
        
        // Set initial fog based on view distance
        this.updateFog();

        // Controls with improved settings
        if (typeof THREE.OrbitControls !== 'undefined') {
            this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
            this.controls.enableDamping = true;
            this.controls.dampingFactor = 0.08;
            this.controls.screenSpacePanning = true;
            this.controls.panSpeed = 1.5;
            this.controls.rotateSpeed = 0.8;
            this.controls.zoomSpeed = 1.2;
            this.controls.minDistance = 10;
            this.controls.maxDistance = this.options.maxRenderDistance;
            this.controls.maxPolarAngle = Math.PI / 2 - 0.02; // Allow nearly horizontal view
            this.controls.minPolarAngle = 0.05; // Prevent looking straight down
            this.controls.target.set(0, 0, 0);
            this.controls.enablePan = true;

            // Mouse button configuration
            // Left: Rotate, Middle: Dolly, Right: Pan
            this.controls.mouseButtons = {
                LEFT: THREE.MOUSE.ROTATE,
                MIDDLE: THREE.MOUSE.DOLLY,
                RIGHT: THREE.MOUSE.PAN
            };
        } else {
            this.setupManualControls();
        }
        
        // Lighting
        this.setupLighting();
        
        // Grid
        if (this.options.showGrid) {
            this.createGrid();
        }
        
        // Water plane
        this.createWaterPlane();
        
        // Create placeholder terrain
        this.createPlaceholderTerrain();

        // Anchor point marker (hidden by default)
        this.createAnchorMarker();
    }

    updateFog() {
        // Update fog based on current view distance
        const fogNear = this.options.viewDistance * 0.6;
        const fogFar = this.options.viewDistance * 1.2;
        this.scene.fog = new THREE.Fog(this.options.backgroundColor, fogNear, fogFar);
    }

    setRenderDistance(distance) {
        this.options.viewDistance = distance;

        // Update fog
        this.updateFog();

        // Update controls max distance
        if (this.controls) {
            this.controls.maxDistance = distance * 1.5;
        }

        // Update camera far plane
        this.camera.far = distance * 1.5;
        this.camera.updateProjectionMatrix();

        // Update slider and label
        if (this.renderDistanceSlider) {
            this.renderDistanceSlider.value = distance;
        }
        const label = document.getElementById('renderDistValue');
        if (label) {
            label.textContent = distance >= 1000 ? `${(distance/1000).toFixed(1)}k` : distance;
        }

        console.log('[SWGTerrainViewer] Render distance set to:', distance);
    }

    createAnchorMarker() {
        const geometry = new THREE.SphereGeometry(10, 16, 16);
        const material = new THREE.MeshBasicMaterial({
            color: 0xff6600,
            transparent: true,
            opacity: 0.8
        });
        this.anchorMarker = new THREE.Mesh(geometry, material);
        this.anchorMarker.visible = false;
        this.scene.add(this.anchorMarker);

        // Anchor mode state
        this.anchorMode = false;
    }

    toggleAnchorMode() {
        this.anchorMode = !this.anchorMode;
        const btn = document.getElementById('anchorModeBtn');
        if (btn) {
            btn.style.background = this.anchorMode ? 'rgba(100, 60, 20, 0.9)' : 'rgba(30, 30, 40, 0.9)';
            btn.style.borderColor = this.anchorMode ? '#f80' : '#444';
        }

        if (!this.anchorMode) {
            this.anchorMarker.visible = false;
        }

        console.log('[SWGTerrainViewer] Anchor mode:', this.anchorMode ? 'ON' : 'OFF');
    }

    setAnchorPoint(x, y, z) {
        this.anchorPoint.set(x, y, z);
        this.anchorMarker.position.copy(this.anchorPoint);
        this.anchorMarker.visible = true;

        if (this.controls) {
            this.controls.target.copy(this.anchorPoint);
        }

        console.log('[SWGTerrainViewer] Anchor point set:', { x, y, z });
    }

    setupLighting() {
        // Ambient light for base illumination
        this.ambientLight = new THREE.AmbientLight(0xffffff, 0.6);
        this.scene.add(this.ambientLight);

        // Main directional light (sun)
        this.sunLight = new THREE.DirectionalLight(0xffffff, 1.0);
        this.sunLight.position.set(5000, 8000, 3000);
        this.sunLight.castShadow = true;

        // Shadow settings
        this.sunLight.shadow.mapSize.width = 2048;
        this.sunLight.shadow.mapSize.height = 2048;
        this.sunLight.shadow.camera.near = 100;
        this.sunLight.shadow.camera.far = 20000;
        this.sunLight.shadow.camera.left = -8000;
        this.sunLight.shadow.camera.right = 8000;
        this.sunLight.shadow.camera.top = 8000;
        this.sunLight.shadow.camera.bottom = -8000;

        this.scene.add(this.sunLight);

        // Hemisphere light for sky/ground ambient
        this.hemiLight = new THREE.HemisphereLight(0x87ceeb, 0x8b7355, 0.4);
        this.scene.add(this.hemiLight);

        console.log('[SWGTerrainViewer] Lighting setup complete');
    }

    createGrid() {
        const halfSize = this.options.mapSize / 2;
        const divisions = 32;
        
        this.gridHelper = new THREE.GridHelper(
            this.options.mapSize,
            divisions,
            0x444444,
            0x333333
        );
        this.gridHelper.position.y = 1;
        this.scene.add(this.gridHelper);
    }

    createWaterPlane() {
        const halfSize = this.options.mapSize / 2;
        const geometry = new THREE.PlaneGeometry(this.options.mapSize, this.options.mapSize);
        const material = new THREE.MeshStandardMaterial({
            color: this.options.waterColor,
            transparent: true,
            opacity: 0.7,
            metalness: 0.1,
            roughness: 0.3,
        });
        
        this.waterMesh = new THREE.Mesh(geometry, material);
        this.waterMesh.rotation.x = -Math.PI / 2;
        this.waterMesh.position.y = this.options.waterHeight;
        this.scene.add(this.waterMesh);
    }

    createPlaceholderTerrain() {
        // Create a simple plane as placeholder
        const halfSize = this.options.mapSize / 2;
        const geometry = new THREE.PlaneGeometry(
            this.options.mapSize,
            this.options.mapSize,
            128,
            128
        );
        
        // Add some random height variation
        const positions = geometry.attributes.position.array;
        for (let i = 0; i < positions.length; i += 3) {
            const x = positions[i];
            const z = positions[i + 1];
            
            // Simple noise-like height
            const height = 
                Math.sin(x * 0.001) * 50 +
                Math.cos(z * 0.001) * 50 +
                Math.sin((x + z) * 0.002) * 25;
            
            positions[i + 2] = height;
        }
        
        geometry.computeVertexNormals();
        
        const material = new THREE.MeshStandardMaterial({
            color: this.options.terrainColor,
            flatShading: false,
            side: THREE.DoubleSide,
        });
        
        this.terrainMesh = new THREE.Mesh(geometry, material);
        this.terrainMesh.rotation.x = -Math.PI / 2;
        this.terrainMesh.receiveShadow = true;
        this.scene.add(this.terrainMesh);
    }

    setupManualControls() {
        // Fallback controls when OrbitControls not available
        const canvas = this.renderer.domElement;
        
        let isMouseDown = false;
        let lastX = 0, lastY = 0;
        let cameraAngleX = 0, cameraAngleY = Math.PI / 4;
        let cameraDistance = 1000;
        let targetX = 0, targetZ = 0;
        
        canvas.addEventListener('mousedown', (e) => {
            isMouseDown = true;
            lastX = e.clientX;
            lastY = e.clientY;
        });
        
        canvas.addEventListener('mousemove', (e) => {
            if (!isMouseDown) return;
            
            const deltaX = e.clientX - lastX;
            const deltaY = e.clientY - lastY;
            lastX = e.clientX;
            lastY = e.clientY;
            
            if (e.buttons === 1) {
                // Left click - rotate
                cameraAngleX -= deltaX * 0.01;
                cameraAngleY = Math.max(0.1, Math.min(Math.PI / 2 - 0.1, cameraAngleY - deltaY * 0.01));
            } else if (e.buttons === 2) {
                // Right click - pan
                targetX -= deltaX * 2;
                targetZ -= deltaY * 2;
            }
            
            this.updateManualCamera(cameraAngleX, cameraAngleY, cameraDistance, targetX, targetZ);
        });
        
        canvas.addEventListener('mouseup', () => isMouseDown = false);
        canvas.addEventListener('mouseleave', () => isMouseDown = false);
        
        canvas.addEventListener('wheel', (e) => {
            e.preventDefault();
            cameraDistance = Math.max(100, Math.min(10000, cameraDistance + e.deltaY));
            this.updateManualCamera(cameraAngleX, cameraAngleY, cameraDistance, targetX, targetZ);
        });
        
        canvas.addEventListener('contextmenu', (e) => e.preventDefault());
        
        this.updateManualCamera(cameraAngleX, cameraAngleY, cameraDistance, targetX, targetZ);
    }

    updateManualCamera(angleX, angleY, distance, targetX, targetZ) {
        const x = targetX + distance * Math.sin(angleY) * Math.sin(angleX);
        const y = distance * Math.cos(angleY);
        const z = targetZ + distance * Math.sin(angleY) * Math.cos(angleX);
        
        this.camera.position.set(x, y, z);
        this.camera.lookAt(targetX, 0, targetZ);
    }

    setupEventListeners() {
        // Resize handler
        window.addEventListener('resize', () => this.onResize());
        
        // Mouse events for object picking
        this.renderer.domElement.addEventListener('mousemove', (e) => this.onMouseMove(e));
        this.renderer.domElement.addEventListener('click', (e) => this.onMouseClick(e));
        
        // Keyboard shortcuts
        window.addEventListener('keydown', (e) => this.onKeyDown(e));
    }

    onResize() {
        const width = this.container.clientWidth || window.innerWidth;
        const height = this.container.clientHeight || window.innerHeight;
        
        this.camera.aspect = width / height;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(width, height);
    }

    onMouseMove(e) {
        const rect = this.renderer.domElement.getBoundingClientRect();
        this.mouse.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
        this.mouse.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;
        
        // Raycast for hovering
        this.raycaster.setFromCamera(this.mouse, this.camera);
        const intersects = this.raycaster.intersectObjects(Array.from(this.objectMarkers.values()));
        
        if (intersects.length > 0) {
            this.hoveredObject = intersects[0].object.userData;
            this.showObjectInfo(this.hoveredObject);
            document.body.style.cursor = 'pointer';
        } else {
            this.hoveredObject = null;
            this.hideObjectInfo();
            document.body.style.cursor = 'default';
        }
    }

    onMouseClick(e) {
        // Handle anchor mode - click on terrain to set anchor point
        if (this.anchorMode && this.terrainMesh) {
            this.raycaster.setFromCamera(this.mouse, this.camera);
            const intersects = this.raycaster.intersectObject(this.terrainMesh);

            if (intersects.length > 0) {
                const point = intersects[0].point;
                this.setAnchorPoint(point.x, point.y, point.z);
                return;
            }
        }

        // Handle object selection
        if (this.hoveredObject) {
            this.selectedObject = this.hoveredObject;
            this.showObjectDetails(this.selectedObject);
        }
    }

    onKeyDown(e) {
        switch (e.key) {
            case 'Home':
                this.goHome();
                break;
            case 'g':
            case 'G':
                this.toggleGrid();
                break;
            case 'Escape':
                this.hideObjectInfo();
                break;
        }
    }

    animate() {
        this.animationId = requestAnimationFrame(() => this.animate());
        
        if (this.controls) {
            this.controls.update();
        }
        
        // Update minimap
        this.updateMinimap();
        
        // Update camera info panel
        this.updateCameraInfo();

        // Render
        this.renderer.render(this.scene, this.camera);
    }

    updateCameraInfo() {
        if (!this.cameraInfoPanel) return;

        const pos = this.camera.position;
        const target = this.controls ? this.controls.target : new THREE.Vector3(0, 0, 0);

        this.cameraInfoPanel.innerHTML = `
            <div>Cam: ${pos.x.toFixed(0)}, ${pos.y.toFixed(0)}, ${pos.z.toFixed(0)}</div>
            <div>Target: ${target.x.toFixed(0)}, ${target.y.toFixed(0)}, ${target.z.toFixed(0)}</div>
            <div>Dist: ${pos.distanceTo(target).toFixed(0)}m</div>
        `;
    }

    updateMinimap() {
        if (!this.minimapCanvas) return;
        
        const ctx = this.minimapCanvas.getContext('2d');
        const w = this.minimapCanvas.width;
        const h = this.minimapCanvas.height;
        
        // Clear
        ctx.fillStyle = '#1a1a2e';
        ctx.fillRect(0, 0, w, h);
        
        // Draw simple terrain representation
        ctx.fillStyle = '#3a3a4e';
        ctx.fillRect(10, 10, w - 20, h - 20);
        
        // Draw camera position
        const halfSize = this.options.mapSize / 2;
        const camX = (this.camera.position.x / this.options.mapSize + 0.5) * w;
        const camZ = (this.camera.position.z / this.options.mapSize + 0.5) * h;
        
        ctx.fillStyle = '#ff0';
        ctx.beginPath();
        ctx.arc(camX, camZ, 5, 0, Math.PI * 2);
        ctx.fill();
        
        // Draw camera direction
        const lookDir = new THREE.Vector3();
        this.camera.getWorldDirection(lookDir);
        ctx.strokeStyle = '#ff0';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(camX, camZ);
        ctx.lineTo(camX + lookDir.x * 20, camZ + lookDir.z * 20);
        ctx.stroke();
        
        // Draw buildout markers
        ctx.fillStyle = '#4a90d9';
        for (const obj of this.buildoutObjects) {
            const x = (obj.x / this.options.mapSize + 0.5) * w;
            const z = (obj.z / this.options.mapSize + 0.5) * h;
            ctx.beginPath();
            ctx.arc(x, z, 2, 0, Math.PI * 2);
            ctx.fill();
        }
        
        // Draw player building markers
        ctx.fillStyle = '#6a4';
        for (const obj of this.playerBuildings) {
            const x = (obj.x / this.options.mapSize + 0.5) * w;
            const z = (obj.z / this.options.mapSize + 0.5) * h;
            ctx.beginPath();
            ctx.arc(x, z, 2, 0, Math.PI * 2);
            ctx.fill();
        }
    }

    // ======================================================================
    // Navigation
    // ======================================================================

    goHome() {
        const target = { x: 0, y: 0, z: 0 };
        
        if (this.controls) {
            this.controls.target.set(target.x, target.y, target.z);
        }
        
        this.camera.position.set(0, 500, 500);
        this.camera.lookAt(0, 0, 0);
        
        console.log('[SWGTerrainViewer] Camera reset to home position');
    }

    goToPosition(x, z, y = null) {
        // Get terrain height at position if y not specified
        const height = y !== null ? y : this.getTerrainHeight(x, z);
        
        if (this.controls) {
            this.controls.target.set(x, height, z);
        }
        
        this.camera.position.set(x, height + 300, z + 300);
        this.camera.lookAt(x, height, z);
        
        console.log('[SWGTerrainViewer] Camera moved to', { x, z, height });
    }

    zoomIn() {
        if (this.controls) {
            const direction = new THREE.Vector3();
            this.camera.getWorldDirection(direction);
            this.camera.position.addScaledVector(direction, 100);
        }
    }

    zoomOut() {
        if (this.controls) {
            const direction = new THREE.Vector3();
            this.camera.getWorldDirection(direction);
            this.camera.position.addScaledVector(direction, -100);
        }
    }

    // ======================================================================
    // Toggle functions
    // ======================================================================

    toggleGrid() {
        if (this.gridHelper) {
            this.gridHelper.visible = !this.gridHelper.visible;
        }
    }

    toggleBuildouts() {
        this.options.showBuildouts = !this.options.showBuildouts;
        this.objectMarkers.forEach((marker, key) => {
            if (marker.userData?.type === 'buildout') {
                marker.visible = this.options.showBuildouts;
            }
        });
    }


    togglePlayerBuildings() {
        this.options.showPlayerBuildings = !this.options.showPlayerBuildings;
        this.objectMarkers.forEach((marker, key) => {
            if (marker.userData?.type === 'player') {
                marker.visible = this.options.showPlayerBuildings;
            }
        });
    }

    // ======================================================================
    // Data loading
    // ======================================================================

    async loadScene(sceneName) {
        this.options.sceneName = sceneName;
        this.showLoading(true, 'Loading terrain data...');
        
        try {
            // Load terrain height data
            await this.loadTerrainData(sceneName);
            
            // Load environment data
            this.setLoadingProgress(60, 'Loading environment...');
            await this.loadEnvironmentData(sceneName);
            
            // Load flora
            this.setLoadingProgress(70, 'Loading flora...');
            await this.loadFloraData(sceneName);
            
            // Load buildout objects
            if (this.options.showBuildouts) {
                this.setLoadingProgress(80, 'Loading buildouts...');
                await this.loadBuildouts(sceneName);
            }
            
            // Load player buildings
            if (this.options.showPlayerBuildings) {
                this.setLoadingProgress(90, 'Loading player buildings...');
                await this.loadPlayerBuildings(sceneName);
            }
            
            // Start audio if enabled
            if (this.audioEnabled && this.currentEnvironment) {
                this.startAudio();
            }
            
            this.setLoadingProgress(100, 'Complete!');
            this.showLoading(false);
            console.log('[SWGTerrainViewer] Scene loaded:', sceneName);
        } catch (error) {
            console.error('[SWGTerrainViewer] Failed to load scene:', error);
            this.showError(`Failed to load scene: ${error.message}`);
            this.showLoading(false);
        }
    }

    /**
     * Load environment data for the scene
     */
    async loadEnvironmentData(sceneName) {
        try {
            const response = await fetch(`/api/terrain/environment/${sceneName}`);
            const result = await response.json();
            
            if (result.success && result.data) {
                this.environments = result.data.environments || [];
                this.currentEnvironment = result.data.currentEnvironment || this.environments[0];
                
                // Apply environment settings
                if (this.currentEnvironment) {
                    this.applyEnvironment(this.currentEnvironment);
                }
                
                console.log('[SWGTerrainViewer] Loaded environment data:', {
                    environmentCount: this.environments.length,
                    current: this.currentEnvironment?.name
                });
            }
        } catch (error) {
            console.warn('[SWGTerrainViewer] Failed to load environment data:', error);
        }
    }

    /**
     * Apply environment settings (fog, lighting, etc.)
     */
    applyEnvironment(environment) {
        if (!environment) return;
        
        // Apply fog settings
        if (environment.fogEnabled && this.scene.fog) {
            const fogDensity = environment.minimumFogDensity || 0.0001;
            this.scene.fog.density = fogDensity;
        }
        
        // Update sun position based on environment
        if (this.sunLight) {
            // Could adjust sun color/intensity based on environment
        }
        
        // Store environment for audio
        this.onEnvironmentChange(environment);
        
        console.log('[SWGTerrainViewer] Applied environment:', environment.name);
    }

    /**
     * Load flora data for the scene
     */
    async loadFloraData(sceneName) {
        try {
            const response = await fetch(`/api/terrain/flora/${sceneName}`);
            const result = await response.json();
            
            if (result.success && result.data) {
                // Clear existing flora
                this.clearFlora();
                
                // Create new flora instances
                if (result.data.flora && result.data.flora.length > 0) {
                    this.createFloraInstances(result.data.flora);
                    console.log('[SWGTerrainViewer] Created flora instances:', result.data.flora.length);
                }
            }
        } catch (error) {
            console.warn('[SWGTerrainViewer] Failed to load flora data:', error);
        }
    }



    async loadTerrainData(sceneName) {
        this.setLoadingProgress(10, 'Fetching terrain data...');
        
        try {
            const response = await fetch(`/api/terrain/heightmap/${sceneName}`);
            const result = await response.json();
            
            if (result.success && result.data) {
                this.terrainData = result.data;
                this.heightMap = result.data.heightMap;
                
                this.setLoadingProgress(50, 'Building terrain mesh...');
                await this.buildTerrainMesh();
            } else {
                console.warn('[SWGTerrainViewer] No terrain data available, using placeholder');
            }
        } catch (error) {
            console.warn('[SWGTerrainViewer] Failed to load terrain data:', error);
        }
    }

    async buildTerrainMesh() {
        if (!this.heightMap) {
            console.warn('[SWGTerrainViewer] No heightMap data to build mesh');
            return;
        }

        console.log('[SWGTerrainViewer] Building terrain mesh', {
            heightMapLength: this.heightMap.length,
            mapSize: this.options.mapSize
        });

        // Remove old terrain
        if (this.terrainMesh) {
            this.scene.remove(this.terrainMesh);
            this.terrainMesh.geometry.dispose();
            if (this.terrainMesh.material.map) {
                this.terrainMesh.material.map.dispose();
            }
            this.terrainMesh.material.dispose();
        }
        
        const resolution = Math.sqrt(this.heightMap.length);
        console.log('[SWGTerrainViewer] Terrain resolution:', resolution);

        const geometry = new THREE.PlaneGeometry(
            this.options.mapSize,
            this.options.mapSize,
            resolution - 1,
            resolution - 1
        );
        
        // Apply height data
        const positions = geometry.attributes.position.array;
        let minH = Infinity, maxH = -Infinity;
        for (let i = 0; i < this.heightMap.length; i++) {
            const h = this.heightMap[i];
            positions[i * 3 + 2] = h;
            if (h < minH) minH = h;
            if (h > maxH) maxH = h;
        }

        console.log('[SWGTerrainViewer] Height range applied:', { min: minH, max: maxH });

        geometry.computeVertexNormals();
        
        // Try to load shader families for coloring
        let material;
        try {
            const shaderData = await this.loadShaderFamilies(this.options.sceneName);
            if (shaderData && shaderData.families && shaderData.families.length > 0) {
                // Apply vertex colors based on height and shader families
                const colors = this.generateTerrainColors(geometry, shaderData.families);
                geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));

                material = new THREE.MeshStandardMaterial({
                    vertexColors: true,
                    flatShading: false,
                    side: THREE.DoubleSide,
                    roughness: 0.8,
                    metalness: 0.1,
                });

                console.log('[SWGTerrainViewer] Applied shader-based terrain coloring');
            } else {
                material = new THREE.MeshStandardMaterial({
                    color: this.options.terrainColor,
                    flatShading: false,
                    side: THREE.DoubleSide,
                    vertexColors: false,
                });
            }
        } catch (error) {
            console.warn('[SWGTerrainViewer] Failed to load shader families:', error);
            material = new THREE.MeshStandardMaterial({
                color: this.options.terrainColor,
                flatShading: false,
                side: THREE.DoubleSide,
                vertexColors: false,
            });
        }

        this.terrainMesh = new THREE.Mesh(geometry, material);
        this.terrainMesh.rotation.x = -Math.PI / 2;
        this.terrainMesh.receiveShadow = true;
        this.scene.add(this.terrainMesh);
        
        console.log('[SWGTerrainViewer] Terrain mesh added to scene', {
            vertexCount: geometry.attributes.position.count,
            materialType: material.type,
            hasVertexColors: material.vertexColors,
            position: this.terrainMesh.position,
            rotation: this.terrainMesh.rotation
        });

        this.setLoadingProgress(70, 'Terrain mesh complete');
    }

    async loadShaderFamilies(sceneName) {
        try {
            const response = await fetch(`/api/terrain/shaders/${sceneName}`);
            const result = await response.json();

            if (result.success && result.data) {
                this.shaderFamilies = result.data.families;
                return result.data;
            }
        } catch (error) {
            console.warn('[SWGTerrainViewer] Failed to load shader families:', error);
        }
        return null;
    }

    generateTerrainColors(geometry, families) {
        const positions = geometry.attributes.position.array;
        const normals = geometry.attributes.normal.array;
        const vertexCount = positions.length / 3;
        const colors = new Float32Array(vertexCount * 3);

        // Find height range
        let minHeight = Infinity, maxHeight = -Infinity;
        for (let i = 0; i < vertexCount; i++) {
            const height = positions[i * 3 + 2];
            minHeight = Math.min(minHeight, height);
            maxHeight = Math.max(maxHeight, height);
        }
        const heightRange = maxHeight - minHeight || 1;

        // Assign colors based on height zones and slope
        for (let i = 0; i < vertexCount; i++) {
            const height = positions[i * 3 + 2];
            const normalY = Math.abs(normals[i * 3 + 1]); // Y component of normal (slope indicator)

            // Normalized height 0-1
            const normalizedHeight = (height - minHeight) / heightRange;

            // Calculate slope (steeper = lower normalY)
            const isFlat = normalY > 0.7;
            const isSteep = normalY < 0.4;

            // Select family based on height and slope
            let family;
            if (families.length >= 3) {
                if (isSteep) {
                    // Rocky/steep terrain - use third family (usually rock)
                    family = families[2];
                } else if (normalizedHeight < 0.3) {
                    // Low areas - first family
                    family = families[0];
                } else if (normalizedHeight < 0.7) {
                    // Mid areas - blend first and second
                    const blend = (normalizedHeight - 0.3) / 0.4;
                    family = this.blendFamilyColors(families[0], families[1], blend);
                } else {
                    // High areas - second family
                    family = families[1];
                }
            } else if (families.length >= 2) {
                const blend = normalizedHeight;
                family = this.blendFamilyColors(families[0], families[1], blend);
            } else {
                family = families[0] || { color: { r: 128, g: 128, b: 100 } };
            }

            // Add some variation based on position
            const variation = (Math.sin(positions[i * 3] * 0.01) * Math.cos(positions[i * 3 + 1] * 0.01)) * 0.1;

            colors[i * 3] = Math.max(0, Math.min(1, (family.color.r / 255) + variation));
            colors[i * 3 + 1] = Math.max(0, Math.min(1, (family.color.g / 255) + variation));
            colors[i * 3 + 2] = Math.max(0, Math.min(1, (family.color.b / 255) + variation));
        }

        return colors;
    }

    blendFamilyColors(family1, family2, t) {
        return {
            color: {
                r: family1.color.r * (1 - t) + family2.color.r * t,
                g: family1.color.g * (1 - t) + family2.color.g * t,
                b: family1.color.b * (1 - t) + family2.color.b * t,
            }
        };
    }

    async loadBuildouts(sceneName) {
        this.setLoadingProgress(75, 'Loading buildout data...');
        
        try {
            const response = await fetch(`/api/terrain/buildouts/${sceneName}`);
            const result = await response.json();
            
            if (result.success && result.data) {
                this.buildoutObjects = result.data.objects || [];
                this.createBuildoutMarkers();
            }
        } catch (error) {
            console.warn('[SWGTerrainViewer] Failed to load buildouts:', error);
        }
    }

    async loadPlayerBuildings(sceneName) {
        this.setLoadingProgress(85, 'Loading player buildings...');
        
        try {
            const response = await fetch(`/api/terrain/objects/${sceneName}`);
            const result = await response.json();
            
            if (result.success && result.data) {
                this.playerBuildings = result.data.buildings || [];
                this.createPlayerBuildingMarkers();
            }
        } catch (error) {
            console.warn('[SWGTerrainViewer] Failed to load player buildings:', error);
        }
    }

    createBuildoutMarkers() {
        // Clear existing markers
        this.objectMarkers.forEach((marker, key) => {
            if (marker.userData?.type === 'buildout') {
                this.scene.remove(marker);
            }
        });
        
        for (const obj of this.buildoutObjects) {
            const marker = this.createObjectMarker(obj, 'buildout', 0x4a90d9);
            this.objectMarkers.set(`buildout_${obj.id}`, marker);
            this.scene.add(marker);
        }
        
        console.log('[SWGTerrainViewer] Created', this.buildoutObjects.length, 'buildout markers');
    }

    createPlayerBuildingMarkers() {
        // Clear existing markers
        this.objectMarkers.forEach((marker, key) => {
            if (marker.userData?.type === 'player') {
                this.scene.remove(marker);
            }
        });
        
        for (const obj of this.playerBuildings) {
            const marker = this.createObjectMarker(obj, 'player', 0x66aa44);
            this.objectMarkers.set(`player_${obj.id}`, marker);
            this.scene.add(marker);
        }
        
        console.log('[SWGTerrainViewer] Created', this.playerBuildings.length, 'player building markers');
    }

    createObjectMarker(obj, type, color) {
        const geometry = new THREE.SphereGeometry(5, 8, 8);
        const material = new THREE.MeshBasicMaterial({ color });
        const marker = new THREE.Mesh(geometry, material);
        
        const height = this.getTerrainHeight(obj.x, obj.z);
        marker.position.set(obj.x, height + 10, obj.z);
        marker.userData = { ...obj, type };
        
        return marker;
    }

    getTerrainHeight(x, z) {
        if (!this.heightMap) return 0;
        
        const halfSize = this.options.mapSize / 2;
        const resolution = Math.sqrt(this.heightMap.length);
        
        const u = (x + halfSize) / this.options.mapSize;
        const v = (z + halfSize) / this.options.mapSize;
        
        const ix = Math.floor(u * (resolution - 1));
        const iz = Math.floor(v * (resolution - 1));
        
        if (ix < 0 || ix >= resolution - 1 || iz < 0 || iz >= resolution - 1) {
            return 0;
        }
        
        return this.heightMap[iz * resolution + ix] || 0;
    }

    // ======================================================================
    // UI helpers
    // ======================================================================

    showLoading(visible, message = '') {
        this.loadingOverlay.style.display = visible ? 'flex' : 'none';
        if (message) {
            this.loadingOverlay.querySelector('.loading-text').textContent = message;
        }
    }

    setLoadingProgress(percent, message = '') {
        this.loadingProgress = percent;
        const progressBar = this.loadingOverlay.querySelector('.loading-progress');
        if (progressBar) {
            progressBar.style.width = `${percent}%`;
        }
        if (message) {
            this.loadingOverlay.querySelector('.loading-text').textContent = message;
        }
    }

    showError(message) {
        const errorDiv = document.createElement('div');
        errorDiv.style.cssText = `
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            padding: 20px 30px;
            background: rgba(200, 50, 50, 0.9);
            color: white;
            border-radius: 8px;
            text-align: center;
        `;
        errorDiv.textContent = message;
        this.uiContainer.appendChild(errorDiv);
        
        setTimeout(() => errorDiv.remove(), 5000);
    }

    showObjectInfo(obj) {
        this.infoPanel.style.display = 'block';
        this.infoPanel.innerHTML = `
            <div style="font-weight: bold; margin-bottom: 8px;">${obj.name || 'Unknown'}</div>
            <div>Type: ${obj.type || 'N/A'}</div>
            <div>Position: (${obj.x?.toFixed(1)}, ${obj.z?.toFixed(1)})</div>
        `;
    }

    hideObjectInfo() {
        this.infoPanel.style.display = 'none';
    }

    showObjectDetails(obj) {
        console.log('[SWGTerrainViewer] Object selected:', obj);
        // Could open a modal or side panel with more details
    }

    // ======================================================================
    // Audio Management
    // ======================================================================

    /**
     * Initialize audio system
     */
    initAudio() {
        this.audioContext = null;
        this.currentMusic = null;
        this.currentAmbient1 = null;
        this.currentAmbient2 = null;
        this.musicVolume = 0.5;
        this.ambientVolume = 0.7;
        this.audioEnabled = false;
        
        // Create audio controls in UI
        this.createAudioControls();
    }

    /**
     * Create audio control panel
     */
    createAudioControls() {
        const audioPanel = document.createElement('div');
        audioPanel.style.cssText = `
            position: absolute;
            bottom: 220px;
            right: 10px;
            padding: 10px;
            background: rgba(20, 20, 30, 0.9);
            border: 1px solid #444;
            border-radius: 8px;
            color: #fff;
            font-size: 12px;
            pointer-events: auto;
            width: 180px;
        `;
        
        audioPanel.innerHTML = `
            <div style="font-weight: bold; margin-bottom: 8px;">🎵 Audio</div>
            <div style="margin-bottom: 8px;">
                <label style="display: flex; align-items: center; cursor: pointer;">
                    <input type="checkbox" id="audioEnabled" style="margin-right: 8px;">
                    Enable Audio
                </label>
            </div>
            <div style="margin-bottom: 8px;">
                <label style="display: block; margin-bottom: 4px;">Music Volume</label>
                <input type="range" id="musicVolume" min="0" max="100" value="50" style="width: 100%;">
            </div>
            <div style="margin-bottom: 8px;">
                <label style="display: block; margin-bottom: 4px;">Ambient Volume</label>
                <input type="range" id="ambientVolume" min="0" max="100" value="70" style="width: 100%;">
            </div>
            <div id="currentTrackInfo" style="font-size: 10px; color: #888; margin-top: 8px;">
                No audio playing
            </div>
        `;
        
        this.uiContainer.appendChild(audioPanel);
        
        // Wire up controls
        const enabledCheckbox = audioPanel.querySelector('#audioEnabled');
        enabledCheckbox.addEventListener('change', (e) => {
            this.audioEnabled = e.target.checked;
            if (this.audioEnabled) {
                this.startAudio();
            } else {
                this.stopAudio();
            }
        });
        
        const musicSlider = audioPanel.querySelector('#musicVolume');
        musicSlider.addEventListener('input', (e) => {
            this.musicVolume = e.target.value / 100;
            this.updateMusicVolume();
        });
        
        const ambientSlider = audioPanel.querySelector('#ambientVolume');
        ambientSlider.addEventListener('input', (e) => {
            this.ambientVolume = e.target.value / 100;
            this.updateAmbientVolume();
        });
        
        this.audioInfoElement = audioPanel.querySelector('#currentTrackInfo');
    }

    /**
     * Start playing audio based on current environment
     */
    async startAudio() {
        if (!this.audioEnabled) return;
        
        try {
            // Initialize audio context on user gesture
            if (!this.audioContext) {
                this.audioContext = new (window.AudioContext || window.webkitAudioContext)();
            }
            
            // Get current environment
            const environment = this.currentEnvironment;
            if (!environment) {
                this.updateAudioInfo('No environment data');
                return;
            }
            
            // Play music
            if (environment.music?.first) {
                await this.playMusic(environment.music.first);
            }
            
            // Play ambient sounds
            const isDay = this.isDay();
            if (environment.ambient?.day1 && isDay) {
                await this.playAmbient(environment.ambient.day1, 1);
            } else if (environment.ambient?.night1 && !isDay) {
                await this.playAmbient(environment.ambient.night1, 1);
            }
            
        } catch (error) {
            console.error('[SWGTerrainViewer] Audio error:', error);
            this.updateAudioInfo('Audio error: ' + error.message);
        }
    }

    /**
     * Stop all audio
     */
    stopAudio() {
        if (this.currentMusic) {
            this.currentMusic.pause();
            this.currentMusic = null;
        }
        if (this.currentAmbient1) {
            this.currentAmbient1.pause();
            this.currentAmbient1 = null;
        }
        if (this.currentAmbient2) {
            this.currentAmbient2.pause();
            this.currentAmbient2 = null;
        }
        this.updateAudioInfo('Audio stopped');
    }

    /**
     * Play music track
     */
    async playMusic(trackPath) {
        if (this.currentMusic) {
            this.currentMusic.pause();
        }
        
        // Convert SWG sound template path to actual audio file
        const audioPath = this.resolveAudioPath(trackPath);
        if (!audioPath) {
            this.updateAudioInfo('Music: ' + trackPath + ' (not found)');
            return;
        }
        
        try {
            this.currentMusic = new Audio(audioPath);
            this.currentMusic.volume = this.musicVolume;
            this.currentMusic.loop = true;
            await this.currentMusic.play();
            
            const trackName = trackPath.split('/').pop().replace('.snd', '');
            this.updateAudioInfo('🎵 ' + trackName);
        } catch (error) {
            console.warn('[SWGTerrainViewer] Failed to play music:', error);
        }
    }

    /**
     * Play ambient sound
     */
    async playAmbient(trackPath, slot) {
        const audioPath = this.resolveAudioPath(trackPath);
        if (!audioPath) return;
        
        try {
            const audio = new Audio(audioPath);
            audio.volume = this.ambientVolume;
            audio.loop = true;
            await audio.play();
            
            if (slot === 1) {
                if (this.currentAmbient1) this.currentAmbient1.pause();
                this.currentAmbient1 = audio;
            } else {
                if (this.currentAmbient2) this.currentAmbient2.pause();
                this.currentAmbient2 = audio;
            }
        } catch (error) {
            console.warn('[SWGTerrainViewer] Failed to play ambient:', error);
        }
    }

    /**
     * Resolve SWG sound path to actual audio file
     */
    resolveAudioPath(swgPath) {
        if (!swgPath) return null;
        
        // Convert sound template name to audio file path
        // e.g., "sound/music/tatooine_desert.snd" -> "/api/audio/sound/music/tatooine_desert.mp3"
        const cleanPath = swgPath
            .replace('.snd', '')
            .replace(/\\/g, '/');
        
        // Return API endpoint that would serve the audio
        return `/api/audio/${cleanPath}`;
    }

    /**
     * Update music volume
     */
    updateMusicVolume() {
        if (this.currentMusic) {
            this.currentMusic.volume = this.musicVolume;
        }
    }

    /**
     * Update ambient volume
     */
    updateAmbientVolume() {
        if (this.currentAmbient1) {
            this.currentAmbient1.volume = this.ambientVolume;
        }
        if (this.currentAmbient2) {
            this.currentAmbient2.volume = this.ambientVolume;
        }
    }

    /**
     * Update audio info display
     */
    updateAudioInfo(text) {
        if (this.audioInfoElement) {
            this.audioInfoElement.textContent = text;
        }
    }

    /**
     * Check if it's day time (simplified)
     */
    isDay() {
        // Could be based on game time or real time
        const hour = new Date().getHours();
        return hour >= 6 && hour < 18;
    }

    /**
     * Update audio based on environment change
     */
    onEnvironmentChange(newEnvironment) {
        this.currentEnvironment = newEnvironment;
        if (this.audioEnabled) {
            this.startAudio();
        }
    }

    // ======================================================================
    // Flora Rendering
    // ======================================================================

    /**
     * Create flora instances from terrain data
     */
    createFloraInstances(floraData) {
        if (!floraData || floraData.length === 0) return;
        
        console.log('[SWGTerrainViewer] Creating flora instances:', floraData.length);
        
        // Group flora by appearance template for instanced rendering
        const floraGroups = new Map();
        
        for (const flora of floraData) {
            const key = flora.appearanceTemplate || 'default';
            if (!floraGroups.has(key)) {
                floraGroups.set(key, []);
            }
            floraGroups.get(key).push(flora);
        }
        
        // Create instanced meshes for each group
        for (const [template, instances] of floraGroups) {
            this.createFloraGroup(template, instances);
        }
    }

    /**
     * Create instanced mesh for a flora group
     */
    createFloraGroup(template, instances) {
        // Create simple placeholder geometry for flora
        // In a full implementation, this would load the actual appearance
        const geometry = new THREE.ConeGeometry(1, 4, 6);
        const material = new THREE.MeshStandardMaterial({
            color: 0x228b22,
            flatShading: true,
        });
        
        const mesh = new THREE.InstancedMesh(geometry, material, instances.length);
        mesh.castShadow = true;
        mesh.receiveShadow = true;
        
        const matrix = new THREE.Matrix4();
        const position = new THREE.Vector3();
        const quaternion = new THREE.Quaternion();
        const scale = new THREE.Vector3();
        
        for (let i = 0; i < instances.length; i++) {
            const flora = instances[i];
            
            position.set(flora.x, flora.y, flora.z);
            quaternion.setFromAxisAngle(new THREE.Vector3(0, 1, 0), flora.rotation || 0);
            scale.set(flora.scale || 1, flora.scale || 1, flora.scale || 1);
            
            matrix.compose(position, quaternion, scale);
            mesh.setMatrixAt(i, matrix);
        }
        
        mesh.instanceMatrix.needsUpdate = true;
        mesh.userData = { type: 'flora', template };
        
        this.scene.add(mesh);
        
        // Store reference for cleanup
        if (!this.floraInstances) {
            this.floraInstances = [];
        }
        this.floraInstances.push(mesh);
    }

    /**
     * Clear all flora instances
     */
    clearFlora() {
        if (this.floraInstances) {
            for (const mesh of this.floraInstances) {
                mesh.geometry.dispose();
                mesh.material.dispose();
                this.scene.remove(mesh);
            }
            this.floraInstances = [];
        }
    }

    // ======================================================================
    // Cleanup
    // ======================================================================

    dispose() {
        console.log('[SWGTerrainViewer] Disposing viewer');
        
        if (this.animationId) {
            cancelAnimationFrame(this.animationId);
        }
        
        // Stop audio
        this.stopAudio();
        if (this.audioContext) {
            this.audioContext.close();
        }
        
        // Clear flora
        this.clearFlora();
        
        // Dispose Three.js objects
        this.objectMarkers.forEach(marker => {
            marker.geometry?.dispose();
            marker.material?.dispose();
        });
        
        if (this.terrainMesh) {
            this.terrainMesh.geometry.dispose();
            this.terrainMesh.material.dispose();
        }
        
        if (this.waterMesh) {
            this.waterMesh.geometry.dispose();
            this.waterMesh.material.dispose();
        }
        
        if (this.gridHelper) {
            this.gridHelper.geometry?.dispose();
            this.gridHelper.material?.dispose();
        }
        
        if (this.controls) {
            this.controls.dispose();
        }
        
        if (this.renderer) {
            this.renderer.dispose();
        }
        
        // Clear container
        this.container.innerHTML = '';
    }
}

// Make available globally
window.SWGTerrainViewer = SWGTerrainViewer;
