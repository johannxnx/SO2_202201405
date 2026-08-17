# 🔒 Sistema de Monitoreo de Seguridad - Servidor HTTP en C

## Cambios Realizados

El backend Node.js/Express ha sido **reemplazado completamente por un servidor HTTP en C** usando `libmicrohttpd` y `jansson` para parsear JSON.

### Arquitectura

```
┌─────────────────────────────────────────────────────────┐
│                    DASHBOARD WEB                        │
│              (HTML/CSS/JavaScript)                       │
│              http://localhost:3000                       │
└──────────────────────────┬──────────────────────────────┘
                           │ GET /api/*
                           ▼
┌─────────────────────────────────────────────────────────┐
│             SERVIDOR HTTP EN C                          │
│  src/server.c (libmicrohttpd + jansson)                 │
│  Puerto: 3000                                           │
│  - Expone endpoints REST                                │
│  - Lee JSONs del daemon en /tmp/                        │
│  - Sirve archivos estáticos del frontend                │
└──────────────────────────┬──────────────────────────────┘
                           │
        ┌──────────────────┼──────────────────┐
        ▼                  ▼                  ▼
   /tmp/               /tmp/              /tmp/
   daemon_             daemon_            daemon_
   monitor.            processes.         files.
   json                json               json
        │                  │                  │
        └──────────────────┼──────────────────┘
                           │
                           ▼
                 ┌──────────────────────┐
                 │   DAEMON MONITOR     │
                 │   (multi-threaded)   │
                 │   Syscalls Kernel    │
                 └──────────────────────┘
```

## Compilación

```bash
cd ProyectoUnico/ProyectoUnico/ProgramaIntermedio/
make clean
make
```

Esto genera:
- ✅ `daemon_monitor` - Daemon de monitoreo
- ✅ `server_http` - Servidor HTTP en C

## Ejecución

### Opción 1: Script Automático (Recomendado)
```bash
./start.sh
```
Esto inicia automáticamente:
1. El daemon monitor (con sudo)
2. El servidor HTTP en puerto 3000
3. Abre el dashboard en http://localhost:3000

### Opción 2: Manual
Terminal 1 (Daemon):
```bash
sudo ./daemon_monitor
```

Terminal 2 (Servidor HTTP):
```bash
./server_http
```

Terminal 3 (Frontend - Opcional, si necesitas desarrollo):
```bash
# Solo si quieres servir desde Python
python3 -m http.server 8080 -d frontend/
```

## Endpoints REST

El servidor HTTP expone los siguientes endpoints:

| Endpoint | Método | Descripción | JSON |
|----------|--------|-------------|------|
| `/` | GET | Página principal (index.html) | HTML |
| `/api/monitor` | GET | Métricas del sistema | `daemon_monitor.json` |
| `/api/files` | GET | Archivos monitoreados | `daemon_files.json` |
| `/api/processes` | GET | Procesos sospechosos | `daemon_processes.json` |
| `/api/alerts` | GET | Alertas de seguridad | `daemon_alerts.json` |
| `/api/quarantine/list` | GET | Archivos en cuarentena | `daemon_quarantine.json` |
| `/style.css` | GET | Estilos | CSS |
| `/app.js` | GET | JavaScript cliente | JavaScript |

## Estructura del Servidor (src/server.c)

### Funciones Principales

- `read_file(path)` - Lee archivos del disco
- `read_json_file(filename)` - Lee y parsea JSON con jansson
- `handle_monitor()` - Expone `/api/monitor`
- `handle_files()` - Expone `/api/files`
- `handle_processes()` - Expone `/api/processes`
- `handle_alerts()` - Expone `/api/alerts`
- `handle_quarantine()` - Expone `/api/quarantine/list`
- `serve_static_file()` - Sirve archivos estáticos (HTML, CSS, JS)
- `request_handler()` - Enrutador principal de requests

### Librerías Utilizadas

```c
#include <microhttpd.h>    // Servidor HTTP
#include <jansson.h>       // Parseo JSON
#include <stdio.h>         // E/S estándar
#include <stdlib.h>        // Utilidades
#include <string.h>        // Strings
#include <unistd.h>        // POSIX API
```

### Compilación Individual (si lo necesitas)

```bash
gcc -Wall -I./include `pkg-config --cflags libmicrohttpd jansson` \
    src/server.c \
    -o server_http \
    `pkg-config --libs libmicrohttpd jansson`
```

## Características

✅ **100% en C** (excepto frontend)  
✅ **Sin dependencias de Node.js**  
✅ **CORS habilitado** para requests del frontend  
✅ **Servicio de archivos estáticos** incluido  
✅ **Manejo de JSON** con jansson  
✅ **Detección automática de content-type** (HTML, CSS, JS, JSON)  
✅ **Manejo de errores** (archivos faltantes, JSON inválido)  
✅ **Multi-threading** con libmicrohttpd  

## Datos en Tiempo Real

El frontend obtiene datos cada **5 segundos** (configurable en `app.js`):

```javascript
const REFRESH_INTERVAL = 5000; // 5 segundos
```

Los datos vienen del daemon que genera JSONs en `/tmp/`:
- `daemon_monitor.json` - Actualizado por `monitor_thread`
- `daemon_processes.json` - Actualizado por `scan_thread`
- `daemon_files.json` - Actualizado según detecciones
- `daemon_alerts.json` - Último evento de seguridad
- `daemon_quarantine.json` - Archivos en cuarentena

## Desarrollo/Depuración

### Ver logs del servidor:
```bash
./server_http 2>&1 | tee server.log
```

### Ver logs del daemon:
```bash
sudo ./daemon_monitor 2>&1 | tee daemon.log
```

### Verificar puertos:
```bash
lsof -i :3000    # Ver qué usa puerto 3000
```

### Limpiar JSONs antiguos:
```bash
rm /tmp/daemon_*.json
```

## Problemas Comunes

### "Error: Address already in use"
Puerto 3000 está en uso. Matar proceso:
```bash
lsof -i :3000 | grep LISTEN | awk '{print $2}' | xargs kill -9
```

### "No se pudo leer archivo de monitoreo"
El daemon aún no ha generado los JSONs. Espera 10 segundos.

### Permisos denegados al iniciar daemon
```bash
sudo ./daemon_monitor
```

## Makefile

El Makefile ahora compila ambos programas:

```makefile
make              # Compila daemon_monitor y server_http
make clean        # Elimina ejecutables
```

## Cambios Respecto a Práctica 6

| Aspecto | Práctica 6 | Proyecto Único |
|---------|-----------|-----------------|
| Backend | Node.js/Express | C + libmicrohttpd |
| Puerto | 3000 | 3000 |
| Servicio estático | Express.static() | serve_static_file() |
| JSON | express.json() | jansson |
| CORS | cors middleware | Headers manuales |
| Threading | Node.js async | libmicrohttpd internal |

## Próximos Pasos

El servidor está completo y funcional. Para agregar más endpoints:

1. Crear función `handle_nuevo_endpoint()`
2. Agregar case en `request_handler()`
3. Actualizar Makefile si agregan archivos fuente

## Referencias

- **libmicrohttpd**: https://www.gnu.org/software/libmicrohttpd/
- **jansson**: https://jansson.readthedocs.io/
- **MHD API**: `/usr/include/microhttpd.h`

---
**Versión**: 2.0 (Con servidor HTTP en C)  
**Fecha**: Abril 2026  
**Autor**: Sistema de Monitoreo  
