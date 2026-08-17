# Manual Técnico - Práctica #6: Monitor de Sistema

**Nombre:** Johan Moises Cardona Rosales  
**Carnet:** 202201405  
**Curso:** SO 2  
**Fecha:** Marzo 2026

---

## Tabla de Contenidos

1. [Descripción General](#descripción-general)
2. [Arquitectura del Sistema](#arquitectura-del-sistema)
3. [Syscalls Implementadas](#syscalls-implementadas)
4. [Flujo de Datos](#flujo-de-datos)
5. [Herramientas Utilizadas](#herramientas-utilizadas)

---

## Descripción General

Sistema de monitoreo de memoria en tiempo real que combina un módulo personalizado del kernel de Linux con una interfaz web moderna. El sistema recolecta información de memoria del sistema operativo mediante syscalls, la procesa a través de un daemon y la muestra en un dashboard interactivo con gráficas en vivo.

**Características principales:**
- Monitoreo continuo de memoria (RAM y SWAP)
- Detección automática de procesos que más consumen
- Gráficas interactivas en tiempo real
- Histórico de 30 puntos de datos
- Consulta de procesos específicos por PID
- Estadísticas de fallos de página
- Dashboard con 5 gráficas y 8 métricas

---

## Arquitectura del Sistema

### Diagrama de Componentes

Arriba puedes ver el **Diagrama de Arquitectura** que muestra las 4 capas del sistema:
- **Capa de Usuario:** Frontend en navegador
- **Capa de Aplicación:** Backend Node.js
- **Capa de Sistema:** Daemon y almacenamiento
- **Capa de Kernel:** Módulo y subsistema de memoria

### Componentes Principales

| Componente | Descripción | Tecnología |
|-----------|-------------|-----------|
| **Frontend** | Dashboard interactivo con gráficas | HTML/CSS/JS + Chart.js |
| **Backend** | Servidor HTTP que sirve datos | Node.js + Express |
| **Almacenamiento** | Archivo JSON temporal | `/tmp/system_monitor.json` |
| **Daemon** | Recolector de datos del kernel | C + Syscalls |
| **Módulo Kernel** | Recolecta información de memoria | Módulo Linux personalizado |
| **Subsistema Memoria** | APIs internas del kernel | Linux Memory Management |

### Capas del Sistema

| Capa | Componente | Lenguaje | Responsabilidad |
|------|-----------|----------|-----------------|
| **Presentación** | Frontend | HTML/CSS/JS | Interfaz visual, gráficas |
| **Aplicación** | Backend | Node.js | Servidor HTTP, API REST |
| **Sistema** | Daemon | C | Interfaz kernel-usuario |
| **Kernel** | Módulo | C | Recolecta datos de memoria |

---

## Syscalls Implementadas

### Syscall Principal: `sys_get_system_monitor`

```c
SYSCALL_DEFINE1(get_system_monitor, struct system_monitor_info __user *, info)
```

**Número de Syscall:** 464  
**Ubicación:** `get_system_monitor.c`  
**Parámetro:** Puntero a estructura en user space  

### Estructura de Datos: `system_monitor_info`

```c
struct system_monitor_info {
    // Memoria (en KB)
    unsigned long memoria_usada;      // RAM en uso
    unsigned long memoria_libre;      // RAM disponible
    unsigned long memoria_cache;      // Buffer memory
    unsigned long swap_usada;         // SWAP utilizado
    
    // Fallos de página
    unsigned long fallos_menores;     // Minor page faults
    unsigned long fallos_mayores;     // Major page faults
    
    // Estado de páginas
    unsigned long paginas_activas;    // Páginas activas
    unsigned long paginas_inactivas;  // Páginas inactivas
    
    // Top procesos
    int num_top_processes;            // Número de procesos
    struct top_process_info procesos_top[5];  // Top 5
};

struct top_process_info {
    int pid;                          // Process ID
    char name[64];                    // Nombre del ejecutable
    unsigned long mem_kb;             // Memoria en KB
    unsigned long mem_percent;        // % de memoria del sistema
};
```

### APIs del Kernel Utilizadas

| API | Función |
|-----|---------|
| `si_meminfo()` | Obtiene información de RAM |
| `si_swapinfo()` | Obtiene información de SWAP |
| `global_node_page_state()` | Estado de páginas activas/inactivas |
| `for_each_process()` | Itera sobre todos los procesos |
| `get_mm_rss()` | Obtiene RSS (Resident Set Size) del proceso |
| `copy_to_user()` | Copia datos a user space de forma segura |
| `rcu_read_lock()` | Bloqueo RCU para iteración segura |

### Diagrama de Flujo de la Syscall

Arriba puedes ver el **Diagrama de Estructura de Datos** que muestra cómo se transfieren los datos de la syscall:
- Recolección en **KERNEL SPACE**
- Copia segura mediante **`copy_to_user()`**
- Recepción en **USER SPACE** (Daemon)

---

## Flujo de Datos

### Diagrama de Secuencia

Arriba puedes ver el **Diagrama de Secuencia** que muestra:
- El ciclo del **Frontend** solicitando datos cada 5 segundos
- El ciclo del **Daemon** invocando syscalls cada 3 segundos
- La interacción completa entre todas las capas

### Ciclo Completo del Sistema (cada 3 segundos)

```
TIEMPO T (Monitor Daemon)
├─ Invoca: syscall(__NR_get_system_monitor, &info)
│  
TIEMPO T (Kernel - sys_get_system_monitor)
├─ Ejecuta si_meminfo()
│  └─ total_ram_kb = (si.totalram * si.mem_unit) / 1024
│  └─ free_ram_kb = (si.freeram * si.mem_unit) / 1024
│  └─ buffer_ram_kb = (si.bufferram * si.mem_unit) / 1024
│
├─ Ejecuta si_swapinfo()
│  └─ total_swap_kb = (si.totalswap * si.mem_unit) / 1024
│  └─ free_swap_kb = (si.freeswap * si.mem_unit) / 1024
│
├─ Calcula memoria_usada = total_ram_kb - free_ram_kb
│
├─ Obtiene páginas activas
│  └─ global_node_page_state(NR_ACTIVE_FILE) + NR_ACTIVE_ANON
│
├─ Obtiene páginas inactivas
│  └─ global_node_page_state(NR_INACTIVE_FILE) + NR_INACTIVE_ANON
│
├─ Itera procesos: for_each_process(task)
│  ├─ mem_kb = (get_mm_rss(task->mm) * PAGE_SIZE) / 1024
│  ├─ fallos_menores += task->min_flt
│  ├─ fallos_mayores += task->maj_flt
│  ├─ mem_percent = (mem_kb * 100) / total_ram_kb
│  └─ Si top 5: guarda en procesos_top[]
│
└─ copy_to_user(info, &kinfo, sizeof(kinfo))
   Copia estructura a user space
   └─ Retorna 0 (éxito) o -EFAULT (error)

TIEMPO T (Daemon - Retorna)
├─ Recibe struct system_monitor_info en stack
│
├─ Formatea a JSON:
│  {
│    "timestamp": <unix_time>,
│    "memoria_usada": <valor>,
│    "memoria_libre": <valor>,
│    "memoria_cache": <valor>,
│    "swap_usada": <valor>,
│    "fallos_menores": <valor>,
│    "fallos_mayores": <valor>,
│    "paginas_activas": <valor>,
│    "paginas_inactivas": <valor>,
│    "procesos_top": [
│      {"pid": N, "nombre": "...", "mem_kb": ..., "mem_percent": ...},
│      ...
│    ]
│  }
│
└─ Escribe a /tmp/system_monitor.json

EN PARALELO - TIEMPO T+5s (Frontend)
├─ fetch('/api/monitor')
│  
TIEMPO T+5s (Backend - GET /api/monitor)
├─ fs.readFile('/tmp/system_monitor.json')
├─ JSON.parse(data)
├─ Si ?pid=N: busca en procesos_top
└─ res.json(datos) - Envía HTTP response

TIEMPO T+5s (Frontend - Recibe datos)
├─ actualizarVista(data)
│  ├─ Actualiza tarjetas (memoria_usada, etc)
│  ├─ Actualiza tabla de procesos
│  └─ Agrega valores al historial (máx 30)
│
├─ crearGraficas(data)
│  ├─ Destruye gráficas anteriores
│  ├─ Chart.js - Pie: Desglose de memoria
│  ├─ Chart.js - Bar: Fallos de página
│  ├─ Chart.js - Pie: Páginas activas/inactivas
│  ├─ Chart.js - Line: Evolución histórica
│  └─ Chart.js - Bar: Top procesos
│
└─ Usuario ve dashboard actualizado
```

### Intervalos de Actualización

```
Kernel ──→ Daemon: 3 segundos (configurable)             [SYSCALL]
Daemon ──→ JSON:   3 segundos (inmediato después kernel) [FILESYSTEM]
Backend ─→ JSON:   Bajo demanda (cuando se solicita)    [LECTURA]
Frontend ─→ API:   5 segundos (configurable)            [HTTP]
```

---

## Herramientas Utilizadas

### Nivel del Kernel

| Herramienta | Versión | Uso |
|-------------|---------|-----|
| **Linux Kernel** | 6.12.69 | Base del sistema |
| **GCC** | Sistema | Compilación de módulo C |
| **Make** | Sistema | Build system del kernel |
| **Linux Headers** | 6.12.69 | APIs del kernel |

**Headers necesarios en `get_system_monitor.c`:**
```c
#include <linux/kernel.h>        // Definiciones del kernel
#include <linux/syscalls.h>      // SYSCALL_DEFINE macros
#include <linux/mm.h>            // Memory management
#include <linux/mmzone.h>        // Zoning
#include <linux/swap.h>          // SWAP info
#include <linux/sched/signal.h>  // task_struct
#include <linux/uaccess.h>       // copy_to_user()
#include <linux/sysinfo.h>       // si_meminfo()
#include <linux/vmstat.h>        // global_node_page_state()
```

### Nivel de Usuario

| Herramienta | Versión | Uso |
|-------------|---------|-----|
| **GCC** | Sistema | Compilación del daemon |
| **C Standard** | C99/C11 | Lenguaje del daemon |

**Librerías usadas en `monitor_daemon.c`:**
```c
#include <stdio.h>      // fprintf, fopen, fclose
#include <unistd.h>     // sleep, fork
#include <sys/syscall.h> // syscall()
#include <string.h>     // strncpy
#include <errno.h>      // Manejo de errores
#include <time.h>       // time()
#include <stdlib.h>     // atoi, malloc
```

### Backend

| Herramienta | Versión | Uso |
|-------------|---------|-----|
| **Node.js** | 14+ | Runtime de JavaScript |
| **Express.js** | 4.18+ | Framework web HTTP |
| **CORS** | 2.8+ | Cross-Origin Resource Sharing |

**Dependencias en `package.json`:**
```json
{
  "dependencies": {
    "express": "^4.18.2",
    "cors": "^2.8.5"
  }
}
```

### Frontend

| Herramienta | Tipo | Uso |
|-------------|------|-----|
| **HTML5** | Markup | Estructura del dashboard |
| **CSS3** | Estilos | Diseño moderno y responsive |
| **JavaScript ES6** | Lógica | Interactividad y actualización |
| **Chart.js** | Librería | Gráficas interactivas (CDN) |

**URL de CDN:**
```html
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
```

### Almacenamiento

| Componente | Uso |
|-----------|-----|
| `/tmp/system_monitor.json` | Almacenamiento temporal de datos |
| **Formato:** JSON | Intercambio de datos |

---

## Compilación e Instalación

### 1. Compilar Módulo Kernel

```bash
cd /home/johan/Escritorio/Practica_4/src/linux-6.12.69/Practica_6/
make
```

### 2. Compilar Daemon

```bash
cd user/
gcc -o monitor_daemon monitor_daemon.c
```

### 3. Instalar Backend

```bash
cd ../backend/
npm install
```

### 4. Ejecutar Sistema (3 terminales)

**Terminal 1 - Daemon:**
```bash
cd user/
sudo ./monitor_daemon 3  # cada 3 segundos
```

**Terminal 2 - Backend:**
```bash
cd backend/
npm start
```

**Terminal 3 - Navegador:**
```
http://localhost:3000/
```

---

## Métricas del Dashboard

### 8 Tarjetas de Información

1. **Memoria Usada (KB)** - RAM en uso actual
2. **Memoria Libre (KB)** - RAM disponible
3. **Memoria Cache (KB)** - Buffer memory del sistema
4. **Swap Usada (KB)** - Almacenamiento virtual utilizado
5. **Fallos Menores** - Minor page faults acumulados
6. **Fallos Mayores** - Major page faults acumulados
7. **Páginas Activas** - Páginas en uso frecuente
8. **Páginas Inactivas** - Páginas no usadas recientemente

### 5 Gráficas Principales

1. **Pie Chart - Desglose de Memoria**
   - Usada, Libre, Cache, Swap
   - Visualiza distribución total

2. **Bar Chart - Fallos de Página**
   - Menores vs Mayores
   - Indicador de presión de memoria

3. **Pie Chart - Estado de Páginas**
   - Activas vs Inactivas
   - Revela uso del sistema

4. **Line Chart - Evolución en el Tiempo**
   - Memoria usada, libre, swap
   - Últimas 30 muestras
   - Identifica tendencias

5. **Bar Chart Horizontal - Top Procesos**
   - Ranking de procesos por memoria
   - Top 5 procesos

### Tabla de Procesos

| Columna | Descripción |
|---------|-------------|
| PID | Process ID único |
| Nombre | Nombre del ejecutable |
| Memoria (KB) | Uso en kilobytes |
| % Memoria | Porcentaje del total |

---

## API REST del Backend

```
GET /
    Retorna: {"mensaje": "...", "endpoints": [...]}

GET /api/health
    Retorna: {"status": "ok", "backend": "running"}

GET /api/monitor
    Retorna: Todos los datos del sistema_monitor.json

GET /api/monitor?pid=<numero>
    Retorna: Información del proceso específico o error 404
```

---

## Información Técnica de Contacto

**Estudiante:** Johan Moises Cardona Rosales  
**Carnet:** 202201405  
**Asignatura:** Tipos de Sistemas Operativos  
**Institución:** Universidad  
**Proyecto:** Práctica #6 - Monitor de Sistema  
**Fecha:** Marzo 2026  

---

**Fin del Manual Técnico**
