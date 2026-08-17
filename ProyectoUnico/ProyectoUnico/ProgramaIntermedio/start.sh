#!/bin/bash

# ============ SCRIPT DE INICIO - SISTEMA DE MONITOREO ============

set -e

PROJECT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DAEMON="$PROJECT_DIR/daemon_monitor"
SERVER="$PROJECT_DIR/server_http"
FRONTEND="$PROJECT_DIR/frontend"

echo "════════════════════════════════════════════════════════════════"
echo "🔒 Sistema Integral de Monitoreo, Análisis y Respuesta de Seguridad"
echo "════════════════════════════════════════════════════════════════"
echo ""

# Verificar que existen los ejecutables
if [ ! -f "$DAEMON" ]; then
    echo "❌ Error: No se encontró $DAEMON"
    echo "   Por favor ejecuta: make"
    exit 1
fi

if [ ! -f "$SERVER" ]; then
    echo "❌ Error: No se encontró $SERVER"
    echo "   Por favor ejecuta: make"
    exit 1
fi

if [ ! -d "$FRONTEND" ]; then
    echo "❌ Error: No se encontró la carpeta $FRONTEND"
    exit 1
fi

echo "✅ Todos los componentes encontrados"
echo ""

# Función para limpiar al salir
cleanup() {
    echo ""
    echo "🛑 Deteniendo servicios..."
    
    # Matar el daemon si está corriendo
    if [ ! -z "$DAEMON_PID" ]; then
        echo "   Deteniendo daemon_monitor (PID: $DAEMON_PID)..."
        kill $DAEMON_PID 2>/dev/null || true
    fi
    
    # Matar el servidor si está corriendo
    if [ ! -z "$SERVER_PID" ]; then
        echo "   Deteniendo server_http (PID: $SERVER_PID)..."
        kill $SERVER_PID 2>/dev/null || true
    fi
    
    echo "✅ Servicios detenidos"
    exit 0
}

trap cleanup EXIT INT TERM

# ======= INICIAR DAEMON =======
echo "🚀 Iniciando daemon_monitor..."
echo "   Comando: sudo $DAEMON"
echo ""

# El daemon hace fork, así que necesitamos capturar el output
OUTPUT=$(sudo $DAEMON 2>&1)
echo "$OUTPUT"

# Extraer el PID del daemon del output
DAEMON_PID=$(echo "$OUTPUT" | grep "PID hijo:" | awk '{print $NF}')

if [ -z "$DAEMON_PID" ]; then
    echo "❌ No se pudo extraer el PID del daemon"
    exit 1
fi

# Esperar un poco para que se estabilice
sleep 2

# Verificar que está corriendo
if ! kill -0 $DAEMON_PID 2>/dev/null; then
    echo "❌ El daemon falló al iniciarse"
    exit 1
fi

echo "✅ Daemon iniciado en background (PID: $DAEMON_PID)"
echo ""

# ======= INICIAR SERVIDOR HTTP =======
echo "🚀 Iniciando server_http en puerto 3000..."
echo "   Frontend: $FRONTEND"
echo "   Comando: $SERVER"
echo ""

cd "$PROJECT_DIR"
$SERVER &
SERVER_PID=$!

sleep 1

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "❌ El servidor HTTP falló al iniciarse"
    exit 1
fi

echo "✅ Servidor HTTP iniciado (PID: $SERVER_PID)"
echo ""

# ======= INFORMACIÓN FINAL =======
echo "════════════════════════════════════════════════════════════════"
echo "🎯 SISTEMA ACTIVO"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "📊 Dashboard:        http://localhost:3000"
echo "🔐 Daemon Monitor:   Activo (PID $DAEMON_PID)"
echo "🌐 HTTP Server:      Activo (PID $SERVER_PID, puerto 3000)"
echo ""
echo "📂 Directorio JSON:  /tmp/daemon_*.json"
echo "   - /tmp/daemon_monitor.json     (Métricas del sistema)"
echo "   - /tmp/daemon_processes.json   (Procesos sospechosos)"
echo "   - /tmp/daemon_files.json       (Archivos monitoreados)"
echo "   - /tmp/daemon_alerts.json      (Alertas de seguridad)"
echo "   - /tmp/daemon_quarantine.json  (Archivos en cuarentena)"
echo ""
echo "🛑 Para detener: Presiona Ctrl+C"
echo ""
echo "════════════════════════════════════════════════════════════════"
echo ""

# Mantener scripts activos
wait
