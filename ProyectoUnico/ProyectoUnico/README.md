# Sistema de Seguridad con Kernel Modificado

Sistema integral de seguridad que combina un kernel Linux modificado con syscalls personalizadas, daemon en C y API REST con dashboard interactivo.

## Arquitectura

```
Kernel Linux 6.12.69 (Modificado)
├── 7 Syscalls personalizadas
│   ├── sys_get_system_monitor (464)
│   ├── sys_file_analize (465)
│   ├── sys_scan_processes (466)
│   ├── sys_quarantine_file (467)
│   ├── sys_restore_file (468)
│   ├── sys_get_quarantine_list (469)
│   └── sys_simulate_panic (470)
└── + sys_get_process_info (463)

Daemon en C (ProgramaIntermedio)
├── Monitor Thread (métricas del sistema)
├── Scan Thread (análisis de archivos)
├── Syscalls Wrapper (interfaz a kernel)
└── JSON Generator (/tmp/daemon_*.json)

API REST (Node.js/Express)
├── GET /api/monitor
├── GET /api/files
├── GET /api/processes
├── GET /api/alerts
├── GET /api/quarantine/list
├── POST /api/quarantine
└── POST /api/restore

Frontend (HTML/CSS/JS)
└── Dashboard interactivo
```

## Instalación y Ejecución

### Prerequisitos

```bash
# Instalar Node.js (versión 14+)
sudo apt update
sudo apt install nodejs npm

# El kernel debe estar compielado e instalado
# Ver: ~/Escritorio/Practica_4/src/linux-6.12.69
```

### 1. Compilar el Daemon

```bash
cd ~/Escritorio/mapa/ProyectoUnico/ProyectoUnico/ProgramaIntermedio
make clean && make
```

**Output esperado:**
```
gcc -Wall -I./include src/main.c src/daemon.c ...
[Directorio de destino]: daemon_monitor
```

### 2. Crear directorio de configuración

```bash
cd ~/Escritorio/mapa/ProyectoUnico/ProyectoUnico/ProgramaIntermedio

# Crear carpetas
mkdir -p config/monitor logs

# Crear archivos de prueba en config/monitor
echo "contenido1" > config/monitor/archivo1.txt
echo "contenido2" > config/monitor/archivo2.txt

# Crear blacklist con hashes maliciosos
cat > config/hash_blacklist.json << 'EOF'
{
  "hashes": [
    {
      "hash": "2b416bcbd09617f2e17f1ca5c5184aa98d7d64e7a52fa7b37ad26d1b23d8f5c5",
      "nombre": "Simulated.Test.Threat.1",
      "severidad": "HIGH",
      "descripcion": "Amenaza simulada para pruebas"
    }
  ]
}
EOF
```

### 3. Ejecutar el Daemon

Abrir una terminal y ejecutar:

```bash
cd ~/Escritorio/mapa/ProyectoUnico/ProyectoUnico/ProgramaIntermedio
sudo ./daemon_monitor
```

**Output esperado:**
```
Daemon iniciado con PID hijo: XXXXX
```

El daemon ejecutará:
- Hilo de monitoreo cada 5 segundos → `/tmp/daemon_monitor.json`
- Hilo de escaneo cada 7 segundos → `/tmp/daemon_files.json`, `/tmp/daemon_processes.json`

### 4. Instalar dependencias del Backend

Abrir otra terminal:

```bash
cd ~/Escritorio/mapa/ProyectoUnico/ProyectoUnico/ProgramaIntermedio/backend
npm install
```

### 5. Ejecutar el Backend API

```bash
npm start
# o en modo desarrollo con auto-reload:
npm run dev
```

**Output esperado:**
```
[API] Backend escuchando en http://0.0.0.0:3000
[API] Leyendo JSONs desde /tmp/daemon_*.json
```

### 6. Ejecutar el Frontend

Abrir un navegador y acceder a:

```
file:///home/johan/Escritorio/mapa/ProyectoUnico/ProyectoUnico/ProgramaIntermedio/frontend/index.html
```

O si tienes un servidor web local:

```bash
cd ~/Escritorio/mapa/ProyectoUnico/ProyectoUnico/ProgramaIntermedio/frontend
python3 -m http.server 8000
```

Luego accede a `http://localhost:8000`

## 📡 Endpoints de la API

### GET /api/monitor
Retorna métricas del sistema

**Response:**
```json
{
  "timestamp": 1713638049,
  "memoria": {
    "usada": 2611996,
    "libre": 5334824,
    "cache": 56340,
    "swap": 0
  },
  "fallos": {
    "menores": 809482,
    "mayores": 3290
  },
  "paginas": {
    "activas": 103658,
    "inactivas": 515289
  }
}
```

### GET /api/files
Retorna archivos monitoreados con análisis de malware

**Response:**
```json
{
  "timestamp": 1713638049,
  "archivos": [
    {
      "ruta": "config/monitor/archivo2.txt",
      "size": 17,
      "timestamp": 1776668748,
      "hash": "2b416bcbd...",
      "estado": "SIN CAMBIOS",
      "malware": {
        "detectado": true,
        "nombre": "Simulated.Test.Threat.1",
        "severidad": "HIGH"
      }
    }
  ]
}
```

### GET /api/processes
Retorna procesos sospechosos detectados

**Response:**
```json
{
  "timestamp": 1713638049,
  "procesos": [
    {
      "pid": 1234,
      "nombre": "Xorg",
      "rss": 38000,
      "vm": 150000
    }
  ]
}
```

### GET /api/alerts
Retorna historial de alertas

**Response:**
```json
{
  "timestamp": 1713638049,
  "alertas": [
    {
      "timestamp": 1713638045,
      "tipo": "archivo",
      "severidad": "HIGH",
      "mensaje": "Hash malicioso detectado",
      "archivo": "config/monitor/archivo2.txt"
    }
  ]
}
```

### GET /api/quarantine/list
Retorna archivos en cuarentena

**Response:**
```json
{
  "timestamp": 1713638049,
  "archivos": [
    {
      "ruta": "config/monitor/archivo2.txt",
      "timestamp_cuarentena": 1713638048
    }
  ]
}
```

### POST /api/quarantine
Pone un archivo en cuarentena

**Request:**
```json
{
  "path": "config/monitor/archivo.txt"
}
```

**Response:**
```json
{
  "accion": "quarantine",
  "path": "config/monitor/archivo.txt",
  "estado": "pendiente",
  "mensaje": "Archivo marcado para cuarentena..."
}
```

### POST /api/restore
Restaura un archivo desde cuarentena

**Request:**
```json
{
  "path": "config/monitor/archivo.txt"
}
```

**Response:**
```json
{
  "accion": "restore",
  "path": "config/monitor/archivo.txt",
  "estado": "pendiente",
  "mensaje": "Archivo marcado para restauración..."
}
```

## Pruebas

### Generar una amenaza detectada

1. Copiar el hash real de `archivo2.txt` al blacklist:

```bash
# Ver hash actual
tail -20 logs/daemon.log | grep "SHA-256"

# Actualizar config/hash_blacklist.json con ese hash
```

2. El daemon detectará automáticamente y:
   - Generará alerta HIGH
   - Pondrá archivo en cuarentena
   - Simulará kernel panic (visible en dmesg)

### Verificar logs

```bash
# Ver logs del daemon
tail -f logs/daemon.log

# Ver eventos del kernel
sudo dmesg | tail -20
```

## 📋 Estructura de Directorios

```
ProyectoUnico/
├── Kernel/              # Headers del kernel (referencia)
├── ProgramaIntermedio/  
│   ├── src/
│   │   ├── main.c       # Punto de entrada
│   │   ├── daemon.c     # Daemonización
│   │   ├── monitor_thread.c  # Métricas → JSON
│   │   ├── scan_thread.c     # Escaneo → JSON
│   │   ├── syscalls_wrapper.c
│   │   ├── blacklist.c
│   │   ├── file_state.c
│   │   └── alerts.c
│   ├── include/
│   │   ├── syscalls_wrapper.h
│   │   ├── system_monitor.h
│   │   ├── file_analyze.h
│   │   ├── process_scan.h
│   │   ├── quarantine.h
│   │   ├── alerts.h
│   │   └── ... (más headers)
│   ├── config/
│   │   ├── monitor/     # Archivos a monitorear
│   │   └── hash_blacklist.json
│   ├── logs/
│   │   └── daemon.log   # Logs de ejecución
│   ├── backend/
│   │   ├── server.js    # API REST (Express)
│   │   └── package.json
│   ├── frontend/
│   │   ├── index.html   # Dashboard
│   │   ├── style.css
│   │   └── app.js       # Lógica del frontend
│   ├── Makefile
│   └── daemon_monitor   # Binary compilado
└── Dashboard/           # (pendiente)
```

## Configuración Avanzada

### Umbrales de procesos sospechosos

Editar `Practica_6/scan_processes.c` en el kernel:

```c
#define RSS_THRESHOLD 15000   // KB
#define VM_THRESHOLD 120000   // KB
```

### Whitelist de procesos

Editar la lista en `Practica_6/scan_processes.c`:

```c
static const char *whitelist[] = {
    "systemd",
    "Xorg",
    // agregar más...
    NULL
};
```

## Troubleshooting

**API retorna "No se pudo leer"**
- Verificar que el daemon esté ejecutándose: `ps aux | grep daemon_monitor`
- Verificar permisos de `/tmp/daemon_*.json`

**Daemon no genera JSONs**
- Verificar permisos en `config/` y `logs/`
- Revisar `logs/daemon.log` para errores

**Frontend no se conecta a API**
- Verificar que el backend está corriendo: `lsof -i :3000`
- Verificar CORS en servidor.js
- Revisar consola del navegador (F12 → Console)

## Referencias

- [Kernel Custom Syscalls](../Practica_4/src/linux-6.12.69/)
- [Syscalls Kernel](../Practica_4/src/linux-6.12.69/arch/x86/entry/syscalls/syscall_64.tbl)
- [System Monitor](../Practica_6/)

## Desarrollo

Para agregar nuevas funcionalidades:

1. **Nueva syscall en kernel**: añadir en `Practica_6/` y registrar en `syscall_64.tbl`
2. **Wrapper en daemon**: actualizar `syscalls_wrapper.c`
3. **Endpoint en API**: agregar ruta en `backend/server.js`
4. **UI en frontend**: actualizar `frontend/index.html` y `app.js`

## Notas

- El daemon consigue permisos root para acceder a información del kernel
- Los JSONs en `/tmp/` se sobrescriben cada ciclo
- El sistema está diseñado para educación y demostraciones
- No usar en producción sin auditoría de seguridad

---

**Proyecto Único** © 2026 - Sistema de Seguridad con Kernel Modificado
