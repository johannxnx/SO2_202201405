// ============ AUTENTICACIÓN Y SESIÓN ============
const API_URL = "http://localhost:3000";
const REFRESH_INTERVAL = 5000; // 5 segundos

// Variables de sesión
let currentSession = null;
const GOLD_TEXT = '#d4af37';

function kbToGb(valueKb) {
    return Number(valueKb || 0) / (1024 * 1024);
}

function formatGb(valueKb) {
    return `${kbToGb(valueKb).toFixed(2)} GB`;
}

function getAuthHeaders(includeJson = false) {
    const headers = {
        'X-User-Role': currentSession?.role || '',
        'X-Username': currentSession?.username || ''
    };

    if (includeJson) {
        headers['Content-Type'] = 'application/json';
    }

    return headers;
}

// PERMISOS POR ROL
const permissions = {
    admin_user: {
        viewDashboard: true,
        viewAlerts: true,
        activateScan: true,
        deactivateScan: true,
        getProcessInfo: true
    },
    common_user: {
        viewDashboard: true,
        viewAlerts: true,
        activateScan: false,
        deactivateScan: false,
        getProcessInfo: false
    }
};

// Verificar sesión al cargar la página
window.onload = function() {
    checkSession();
};

// Verificar si hay sesión activa
function checkSession() {
    const sessionData = localStorage.getItem('auth_session');
    if (sessionData) {
        try {
            currentSession = JSON.parse(sessionData);
            showDashboard();
        } catch (e) {
            localStorage.removeItem('auth_session');
            showLogin();
        }
    } else {
        showLogin();
    }
}

// Mostrar pantalla de login
function showLogin() {
    document.getElementById('login-screen').classList.add('active');
    document.getElementById('dashboard-screen').classList.remove('active');
    currentSession = null;
}

// Mostrar dashboard
function showDashboard() {
    document.getElementById('login-screen').classList.remove('active');
    document.getElementById('dashboard-screen').classList.add('active');
    
    // Actualizar información del usuario
    document.getElementById('user-display').textContent = currentSession.username;
    document.getElementById('role-display').textContent = currentSession.role === 'admin_user' ? 'Admin' : 'Usuario';
    
    // Aplicar restricciones de permisos
    applyPermissions();
    
    // Cargar datos iniciales
    loadMonitor();
    
    // Auto-refresh cada 5 segundos
    setInterval(loadMonitor, REFRESH_INTERVAL);
}

// Manejar login
async function handleLogin(event) {
    event.preventDefault();
    
    const username = document.getElementById('username').value;
    const password = document.getElementById('password').value;
    const errorDiv = document.getElementById('login-error');
    
    errorDiv.style.display = 'none';
    
    try {
        // Llamar al endpoint de login en el servidor
        const response = await fetch(`${API_URL}/api/login`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                username: username,
                password: password
            })
        });
        
        const data = await response.json();
        
        if (!response.ok || !data.success) {
            throw new Error(data.error || 'Usuario o contraseña incorrectos');
        }
        
        // Guardar sesión
        currentSession = {
            username: data.username,
            role: data.role,
            token: data.token || null
        };
        
        localStorage.setItem('auth_session', JSON.stringify(currentSession));
        document.getElementById('username').value = '';
        document.getElementById('password').value = '';
        
        showDashboard();
    } catch (err) {
        errorDiv.textContent = err.message;
        errorDiv.style.display = 'block';
        console.error('Error en login:', err);
    }
}

// Manejar logout
function handleLogout() {
    localStorage.removeItem('auth_session');
    currentSession = null;
    showLogin();
}

// Verificar permiso
function checkPermission(permission) {
    if (!currentSession) return false;
    
    const userPermissions = permissions[currentSession.role];
    return userPermissions && userPermissions[permission];
}

// Aplicar restricciones de permisos a la interfaz
function applyPermissions() {
    const isAdmin = currentSession.role === 'admin_user';
    const adminScanControls = document.getElementById('admin-scan-controls');
    const adminProcessInfo = document.getElementById('admin-process-info');
    
    // Según la tabla:
    // Admin: Ver dashboard ✓, Ver alertas ✓, Activar escaneo ✓, Desactivar escaneo ✓, Obtener info proceso ✓
    // Usuario: Ver dashboard ✓, Ver alertas ✓, Activar escaneo ✗, Desactivar escaneo ✗, Obtener info proceso ✗
    
    if (adminScanControls) {
        adminScanControls.style.display = isAdmin ? 'block' : 'none';
    }

    if (adminProcessInfo) {
        adminProcessInfo.style.display = isAdmin ? 'block' : 'none';
    }
}

// ============ VARIABLES GLOBALES PARA GRÁFICAS ============
let graficaMemoria = null;
let graficaFallos = null;
let graficaPaginas = null;
let graficaProcesos = null;
let graficaEvolucion = null;
let lastMonitorData = null;

// Historial para gráfica de evolución (máximo 30 puntos)
let historicalData = {
    timestamps: [],
    memoriaUsada: [],
    swapUsada: []
};

// ============ FUNCIONES DE GRÁFICAS ============

function destruirGraficas() {
    if (graficaMemoria) graficaMemoria.destroy();
    if (graficaFallos) graficaFallos.destroy();
    if (graficaPaginas) graficaPaginas.destroy();
    if (graficaProcesos) graficaProcesos.destroy();
    if (graficaEvolucion) graficaEvolucion.destroy();
}

function crearGraficas(data, procesos = []) {
    destruirGraficas();
    const totalMemoriaFisica = Math.max(1, data.memoria.usada + data.memoria.libre + data.memoria.cache);

    // Actualizar historial (máximo 30 puntos)
    const hora = new Date(data.timestamp * 1000).toLocaleTimeString();
    historicalData.timestamps.push(hora);
    historicalData.memoriaUsada.push(kbToGb(data.memoria.usada));
    historicalData.swapUsada.push(kbToGb(data.memoria.swap));

    if (historicalData.timestamps.length > 30) {
        historicalData.timestamps.shift();
        historicalData.memoriaUsada.shift();
        historicalData.swapUsada.shift();
    }

    // GRÁFICA 1: DISTRIBUCIÓN DE MEMORIA (Pie)
    const ctxMemoria = document.getElementById("grafica_memoria");
    if (ctxMemoria) {
        graficaMemoria = new Chart(ctxMemoria, {
            type: "pie",
            data: {
                labels: ["Memoria usada", "Memoria libre", "Cache"],
                datasets: [{
                    data: [
                        data.memoria.usada,
                        data.memoria.libre,
                        data.memoria.cache
                    ],
                    backgroundColor: [
                        '#ff6384',
                        '#36a2eb',
                        '#ffce56'
                    ]
                }]
            },
            options: {
                responsive: true,
                plugins: {
                    legend: {
                        labels: {
                            color: GOLD_TEXT,
                            font: { size: 12 }
                        }
                    }
                }
            }
        });
    }

    // GRÁFICA 2: FALLOS DE PÁGINA (Bar)
    const ctxFallos = document.getElementById("grafica_fallos");
    if (ctxFallos) {
        graficaFallos = new Chart(ctxFallos, {
            type: "bar",
            data: {
                labels: ["Fallos menores", "Fallos mayores"],
                datasets: [{
                    label: "Cantidad",
                    data: [data.fallos.menores, data.fallos.mayores],
                    backgroundColor: ['#36a2eb', '#ff6384']
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: {
                        beginAtZero: true,
                        ticks: { color: GOLD_TEXT },
                        grid: { color: "#f0f0f0" }
                    },
                    x: {
                        ticks: { color: GOLD_TEXT },
                        grid: { color: "#f0f0f0" }
                    }
                },
                plugins: {
                    legend: {
                        labels: { color: GOLD_TEXT }
                    }
                }
            }
        });
    }

    // GRÁFICA 3: ESTADO DE PÁGINAS (Pie)
    const ctxPaginas = document.getElementById("grafica_paginas");
    if (ctxPaginas) {
        graficaPaginas = new Chart(ctxPaginas, {
            type: "pie",
            data: {
                labels: ["Activas", "Inactivas"],
                datasets: [{
                    data: [data.paginas.activas, data.paginas.inactivas],
                    backgroundColor: ['#4bc0c0', '#ff9f40']
                }]
            },
            options: {
                responsive: true,
                plugins: {
                    legend: {
                        labels: { color: GOLD_TEXT, font: { size: 12 } }
                    }
                }
            }
        });
    }

    // GRÁFICA 4: PROCESOS SOSPECHOSOS (Bar horizontal)
    const ctxProcesos = document.getElementById("grafica_procesos");
    if (ctxProcesos && procesos.length > 0) {
        // Ordenar por uso porcentual de memoria y tomar top 10
        const topProcesos = procesos
            .map(p => ({
                ...p,
                porcentaje_memoria: ((p.rss || 0) / totalMemoriaFisica) * 100
            }))
            .sort((a, b) => b.porcentaje_memoria - a.porcentaje_memoria)
            .slice(0, 10);

        graficaProcesos = new Chart(ctxProcesos, {
            type: "bar",
            data: {
                labels: topProcesos.map(p => `${p.nombre} (PID: ${p.pid})`),
                datasets: [{
                    label: "Uso de memoria (%)",
                    data: topProcesos.map(p => Number(p.porcentaje_memoria.toFixed(2))),
                    backgroundColor: '#ff9f40'
                }]
            },
            options: {
                indexAxis: "y",
                responsive: true,
                scales: {
                    x: {
                        beginAtZero: true,
                        ticks: { color: GOLD_TEXT },
                        grid: { color: "#f0f0f0" }
                    },
                    y: {
                        ticks: { color: GOLD_TEXT },
                        grid: { color: "#f0f0f0" }
                    }
                },
                plugins: {
                    legend: {
                        labels: { color: GOLD_TEXT }
                    }
                }
            }
        });
    }

    // GRÁFICA 5: EVOLUCIÓN TEMPORAL (Line)
    const ctxEvolucion = document.getElementById("grafica_evolucion");
    if (ctxEvolucion) {
        graficaEvolucion = new Chart(ctxEvolucion, {
            type: "line",
            data: {
                labels: historicalData.timestamps,
                datasets: [
                    {
                        label: "Memoria física usada (GB)",
                        data: historicalData.memoriaUsada,
                        borderColor: '#ff6384',
                        backgroundColor: 'rgba(255, 99, 132, 0.1)',
                        tension: 0.4,
                        borderWidth: 2,
                        fill: true
                    },
                    {
                        label: "Swap usada (GB)",
                        data: historicalData.swapUsada,
                        borderColor: '#4bc0c0',
                        backgroundColor: 'rgba(75, 192, 192, 0.1)',
                        tension: 0.4,
                        borderWidth: 2,
                        fill: true
                    }
                ]
            },
            options: {
                responsive: true,
                interaction: {
                    mode: 'index',
                    intersect: false,
                },
                scales: {
                    y: {
                        beginAtZero: true,
                        ticks: { color: GOLD_TEXT },
                        grid: { color: "#f0f0f0" }
                    },
                    x: {
                        ticks: { color: GOLD_TEXT },
                        grid: { color: "#f0f0f0" }
                    }
                },
                plugins: {
                    legend: {
                        labels: { color: GOLD_TEXT, font: { size: 12 } }
                    }
                }
            }
        });
    }
}

// ============ CAMBIAR ENTRE PESTAÑAS ============
function showTab(tabName) {
    // Ocultar todas las pestañas
    document.querySelectorAll('.tab-content').forEach(tab => {
        tab.classList.remove('active');
    });

    // Desactivar todos los botones
    document.querySelectorAll('.tab-button').forEach(btn => {
        btn.classList.remove('active');
    });

    // Activar la pestaña seleccionada
    document.getElementById(tabName).classList.add('active');
    event.target.classList.add('active');

    // Cargar datos cuando se cambia de pestaña
    loadTabData(tabName);
}

// Cargar datos según la pestaña activa
function loadTabData(tabName) {
    switch (tabName) {
        case 'monitor':
            loadMonitor();
            break;
        case 'files':
            loadFiles();
            break;
        case 'processes':
            loadProcesses();
            break;
        case 'alerts':
            loadAlerts();
            break;
        case 'quarantine':
            loadQuarantine();
            break;
    }
}

// MONITOREO
async function loadMonitor() {
    try {
        // Obtener datos de monitoreo (memoria, fallos, páginas)
        const resMonitor = await fetch(`${API_URL}/api/monitor`);
        if (!resMonitor.ok) throw new Error(`HTTP ${resMonitor.status}`);
        const dataMonitor = await resMonitor.json();
        lastMonitorData = dataMonitor;
        
        // Obtener datos de procesos (para gráfica de procesos sospechosos)
        let procesos = [];
        try {
            const resProcesos = await fetch(`${API_URL}/api/processes`);
            if (resProcesos.ok) {
                const dataProcesos = await resProcesos.json();
                procesos = dataProcesos.procesos || [];
            }
        } catch (err) {
            console.warn('No se pudieron cargar procesos:', err);
        }
        
        // Actualizar tarjetas de métricas
        document.getElementById('mem-used').textContent = formatGb(dataMonitor.memoria.usada);
        document.getElementById('mem-free').textContent = formatGb(dataMonitor.memoria.libre);
        document.getElementById('mem-cache').textContent = formatGb(dataMonitor.memoria.cache);
        document.getElementById('swap-used').textContent = formatGb(dataMonitor.memoria.swap);
        document.getElementById('faults-minor').textContent = dataMonitor.fallos.menores.toLocaleString();
        document.getElementById('faults-major').textContent = dataMonitor.fallos.mayores.toLocaleString();
        document.getElementById('pages-active').textContent = dataMonitor.paginas.activas.toLocaleString();
        document.getElementById('pages-inactive').textContent = dataMonitor.paginas.inactivas.toLocaleString();
        
        // Mostrar deltas de fallos (últimos 5 segundos)
        const deltaMinores = dataMonitor.fallos.delta_menores || 0;
        const deltaMayores = dataMonitor.fallos.delta_mayores || 0;
        document.getElementById('faults-minor-delta').textContent = `Δ: ${deltaMinores.toLocaleString()} cada 5s`;
        document.getElementById('faults-major-delta').textContent = `Δ: ${deltaMayores.toLocaleString()} cada 5s`;
        
        // Mostrar páginas en GB (equivalentes en memoria)
        const activasGb = (dataMonitor.paginas.activas_gb || 0).toFixed(2);
        const inactivasGb = (dataMonitor.paginas.inactivas_gb || 0).toFixed(2);
        document.getElementById('pages-active-gb').textContent = `${activasGb} GB`;
        document.getElementById('pages-inactive-gb').textContent = `${inactivasGb} GB`;
        
        const date = new Date(dataMonitor.timestamp * 1000).toLocaleTimeString();
        document.getElementById('update-time').textContent = date;

        if (checkPermission('activateScan')) {
            loadScanStatus();
        }
        
        // Crear y actualizar gráficas
        crearGraficas(dataMonitor, procesos);
        
        updateStatus('ok');
    } catch (err) {
        console.error('Error cargando monitoreo:', err);
        updateStatus('error', err.message);
    }
}

// ARCHIVOS
async function loadFiles() {
    try {
        const res = await fetch(`${API_URL}/api/files`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        
        const data = await res.json();
        const tbody = document.getElementById('files-body');
        const threatsBody = document.getElementById('threats-body');
        tbody.innerHTML = '';
        if (threatsBody) threatsBody.innerHTML = '';

        if (!data.archivos || data.archivos.length === 0) {
            tbody.innerHTML = '<tr><td colspan="7">Sin archivos monitoreados</td></tr>';
            if (threatsBody) {
                threatsBody.innerHTML = '<tr><td colspan="4">Sin amenazas detectadas</td></tr>';
            }
            return;
        }

        const threats = [];

        data.archivos.forEach(file => {
            const row = document.createElement('tr');
            const isMalware = !!file?.malware?.detectado;
            const visualState = isMalware
                ? 'SOSPECHOSO'
                : (file.estado === 'MODIFICADO' ? 'MODIFICADO' : 'LIMPIO');
            const statusClass = visualState === 'SOSPECHOSO'
                ? 'status-modified'
                : (visualState === 'MODIFICADO' ? 'status-modified' : 'status-clean');
            
            const malwareStatus = isMalware ? 
                '<span class="malware-detected">✓ DETECTADO</span>' : 
                '<span class="malware-clean">✓ LIMPIO</span>';
            
            const severidad = isMalware ? 
                `<span class="severity-${String(file.malware.severidad || 'low').toLowerCase()}">${file.malware.severidad || 'LOW'}</span>` : 
                '-';

            const hashPartial = file.hash ? `${file.hash.slice(0, 12)}...` : '-';
            const fileDate = file.timestamp
                ? new Date(Number(file.timestamp) * 1000).toLocaleString()
                : '-';

            row.innerHTML = `
                <td><code>${file.ruta}</code></td>
                <td>${file.size.toLocaleString()}</td>
                <td><code>${hashPartial}</code></td>
                <td>${fileDate}</td>
                <td><span class="${statusClass}">${visualState}</span></td>
                <td>${malwareStatus}</td>
                <td>${severidad}</td>
            `;
            tbody.appendChild(row);

            if (isMalware) {
                threats.push({
                    nombre: file?.malware?.nombre || 'Amenaza desconocida',
                    archivo: file?.ruta || '-',
                    severidad: file?.malware?.severidad || 'LOW',
                    descripcion: file?.malware?.descripcion || 'Sin descripción'
                });
            }
        });

        if (threatsBody) {
            if (threats.length === 0) {
                threatsBody.innerHTML = '<tr><td colspan="4">Sin amenazas detectadas</td></tr>';
            } else {
                threats.forEach(threat => {
                    const row = document.createElement('tr');
                    row.innerHTML = `
                        <td>${threat.nombre}</td>
                        <td><code>${threat.archivo}</code></td>
                        <td><span class="severity-${String(threat.severidad).toLowerCase()}">${threat.severidad}</span></td>
                        <td>${threat.descripcion}</td>
                    `;
                    threatsBody.appendChild(row);
                });
            }
        }

        updateStatus('ok');
    } catch (err) {
        console.error('Error cargando archivos:', err);
        updateStatus('error', err.message);
    }
}

// PROCESOS
async function loadProcesses() {
    try {
        const res = await fetch(`${API_URL}/api/processes`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        
        const data = await res.json();
        const tbody = document.getElementById('processes-body');
        tbody.innerHTML = '';

        if (!data.procesos || data.procesos.length === 0) {
            tbody.innerHTML = '<tr><td colspan="5">Sin procesos sospechosos</td></tr>';
            return;
        }

        const totalMemoriaFisica = lastMonitorData
            ? Math.max(1, lastMonitorData.memoria.usada + lastMonitorData.memoria.libre + lastMonitorData.memoria.cache)
            : 1;

        data.procesos.forEach(proc => {
            const row = document.createElement('tr');
            const porcentajeMemoria = ((proc.rss || 0) / totalMemoriaFisica) * 100;
            row.innerHTML = `
                <td><strong>${proc.pid}</strong></td>
                <td>${proc.nombre}</td>
                <td>${kbToGb(proc.rss).toFixed(2)}</td>
                <td>${kbToGb(proc.vm).toFixed(2)}</td>
                <td>${porcentajeMemoria.toFixed(2)}%</td>
            `;
            tbody.appendChild(row);
        });

        updateStatus('ok');
    } catch (err) {
        console.error('Error cargando procesos:', err);
        updateStatus('error', err.message);
    }
}

// ALERTAS
async function loadAlerts() {
    try {
        const res = await fetch(`${API_URL}/api/alerts`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        
        const data = await res.json();
        const tbody = document.getElementById('alerts-body');
        tbody.innerHTML = '';

        if (!data.alertas || data.alertas.length === 0) {
            tbody.innerHTML = '<tr><td colspan="5">Sin alertas registradas</td></tr>';
            return;
        }

        data.alertas.forEach(alert => {
            const row = document.createElement('tr');
            const date = new Date(alert.timestamp * 1000).toLocaleTimeString();
            const severity = alert.severidad || 'LOW';
            const severityClass = `severity-${String(severity).toLowerCase()}`;
            const tipo = alert.tipo || (String(alert.evento || '').toLowerCase().includes('proceso') ? 'proceso' : 'archivo');
            const mensaje = alert.mensaje || alert.descripcion || alert.detalle || alert.evento || 'Sin descripción';
            
            row.innerHTML = `
                <td>${date}</td>
                <td>${tipo}</td>
                <td><span class="${severityClass}">${severity}</span></td>
                <td>${mensaje}</td>
                <td><code>${alert.archivo || '-'}</code></td>
            `;
            tbody.appendChild(row);
        });

        updateStatus('ok');
    } catch (err) {
        console.error('Error cargando alertas:', err);
        updateStatus('error', err.message);
    }
}

// CUARENTENA
async function loadQuarantine() {
    try {
        const res = await fetch(`${API_URL}/api/quarantine/list`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        
        const data = await res.json();
        const tbody = document.getElementById('quarantine-body');
        tbody.innerHTML = '';

        if (!data.archivos || data.archivos.length === 0) {
            tbody.innerHTML = '<tr><td colspan="3">Sin archivos en cuarentena</td></tr>';
            return;
        }

        data.archivos.forEach(file => {
            const row = document.createElement('tr');
            const timestamp = Number(file.timestamp_cuarentena ?? file.timestamp ?? 0);
            const date = timestamp > 0
                ? new Date(timestamp * 1000).toLocaleString()
                : 'Fecha no disponible';
            const actionsHtml = checkPermission('activateScan')
                ? `<button class="btn btn-success" onclick="restoreFile('${file.ruta}')">Restaurar</button>`
                : '<span>Sin permisos</span>';
            
            row.innerHTML = `
                <td><code>${file.ruta}</code></td>
                <td>${date}</td>
                <td>
                    ${actionsHtml}
                </td>
            `;
            tbody.appendChild(row);
        });

        updateStatus('ok');
    } catch (err) {
        console.error('Error cargando cuarentena:', err);
        updateStatus('error', err.message);
    }
}

// Restaurar archivo
async function restoreFile(path) {
    if (!checkPermission('activateScan')) {
        alert('Permiso denegado: requiere rol administrador.');
        return;
    }

    try {
        const res = await fetch(`${API_URL}/api/restore`, {
            method: 'POST',
            headers: getAuthHeaders(true),
            body: JSON.stringify({ path })
        });

        const data = await res.json();
        if (!res.ok || !data.success) {
            throw new Error(data.error || 'No se pudo restaurar');
        }
        alert(`Restauración iniciada: ${data.mensaje}`);
        loadQuarantine();
    } catch (err) {
        console.error('Error restaurando archivo:', err);
        alert(`Error: ${err.message}`);
    }
}

async function startScan() {
    if (!checkPermission('activateScan')) {
        alert('Permiso denegado: requiere rol administrador.');
        return;
    }

    try {
        const res = await fetch(`${API_URL}/api/scan/start`, {
            method: 'POST',
            headers: getAuthHeaders(true),
            body: JSON.stringify({})
        });
        const data = await res.json();
        if (!res.ok || !data.success) throw new Error(data.error || 'No se pudo activar escaneo');
        loadScanStatus();
    } catch (err) {
        alert(`Error al activar escaneo: ${err.message}`);
    }
}

async function stopScan() {
    if (!checkPermission('deactivateScan')) {
        alert('Permiso denegado: requiere rol administrador.');
        return;
    }

    try {
        const res = await fetch(`${API_URL}/api/scan/stop`, {
            method: 'POST',
            headers: getAuthHeaders(true),
            body: JSON.stringify({})
        });
        const data = await res.json();
        if (!res.ok || !data.success) throw new Error(data.error || 'No se pudo desactivar escaneo');
        loadScanStatus();
    } catch (err) {
        alert(`Error al desactivar escaneo: ${err.message}`);
    }
}

async function loadScanStatus() {
    const statusEl = document.getElementById('scan-status-text');
    if (!statusEl) return;

    try {
        const res = await fetch(`${API_URL}/api/scan/status`);
        const data = await res.json();
        if (!res.ok || !data.success) throw new Error(data.error || 'Sin estado');
        statusEl.textContent = `Estado escaneo: ${data.enabled ? 'ACTIVO' : 'DETENIDO'}`;
        statusEl.style.color = data.enabled ? '#4CAF50' : '#F44336';
    } catch (err) {
        statusEl.textContent = `Estado escaneo: error (${err.message})`;
        statusEl.style.color = '#F44336';
    }
}

async function fetchProcessInfo() {
    if (!checkPermission('getProcessInfo')) {
        alert('Permiso denegado: requiere rol administrador.');
        return;
    }

    const pidInput = document.getElementById('pid-input');
    const resultEl = document.getElementById('pid-result');
    if (!pidInput || !resultEl) return;

    const pid = Number(pidInput.value);
    if (!Number.isInteger(pid) || pid <= 0) {
        resultEl.textContent = 'PID inválido.';
        resultEl.style.color = '#F44336';
        return;
    }

    try {
        const res = await fetch(`${API_URL}/api/process/info`, {
            method: 'POST',
            headers: getAuthHeaders(true),
            body: JSON.stringify({ pid })
        });
        const data = await res.json();
        if (!res.ok || !data.success) throw new Error(data.error || 'No encontrado');

        resultEl.style.color = '#4CAF50';
        resultEl.textContent = `PID ${data.pid} | Nombre: ${data.name} | Memoria: ${kbToGb(data.mem_kb).toFixed(2)} GB | Tiempo ejecución: ${Number(data.exec_time_ms).toLocaleString()} ms`;
    } catch (err) {
        resultEl.style.color = '#F44336';
        resultEl.textContent = `Error: ${err.message}`;
    }
}

// Actualizar estado de conexión
function updateStatus(status, message = '') {
    const statusEl = document.getElementById('status');
    
    if (status === 'ok') {
        statusEl.textContent = '✓ Estado: Conectado';
        statusEl.style.color = '#4CAF50';
        statusEl.style.borderColor = '#4CAF50';
    } else if (status === 'error') {
        statusEl.textContent = `✗ Error: ${message}`;
        statusEl.style.color = '#F44336';
        statusEl.style.borderColor = '#F44336';
    }
}

// Auto-refresh
setInterval(() => {
    const activeTab = document.querySelector('.tab-content.active').id;
    loadTabData(activeTab);
}, REFRESH_INTERVAL);

// Cargar al iniciar
window.addEventListener('DOMContentLoaded', () => {
    loadMonitor();
});
