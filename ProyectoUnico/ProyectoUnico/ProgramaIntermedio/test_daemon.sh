#!/bin/bash

# Script de prueba para el daemon de seguridad
# Verifica que todos los JSONs se generen correctamente

echo "=========================================="
echo "Script de Prueba - Sistema de Seguridad"
echo "=========================================="

DAEMON_DIR="/home/johan/Escritorio/mapa/ProyectoUnico/ProyectoUnico/ProgramaIntermedio"
TEST_DIR="/tmp/security_test_$$"

echo ""
echo "[1] Preparando ambiente de prueba..."
mkdir -p "$TEST_DIR/config/monitor"
mkdir -p "$TEST_DIR/logs"

# Copiar configuraciones necesarias
cp "$DAEMON_DIR/config"/* "$TEST_DIR/config/" 2>/dev/null || true
cp "$DAEMON_DIR/daemon_monitor" "$TEST_DIR/" 2>/dev/null

# Cambiar a directorio de prueba
cd "$TEST_DIR"

echo "[2] Creando archivos de prueba en config/monitor..."
echo "archivo_prueba_1" > config/monitor/test_file_1.txt
echo "archivo_prueba_2" > config/monitor/test_file_2.txt
echo "contenido importante" > config/monitor/importante.conf

echo "[3] Ejecutando daemon durante 15 segundos..."
timeout 15 ./daemon_monitor 2>/dev/null &
DAEMON_PID=$!

echo "Esperando primer ciclo de escaneo (7 segundos)..."
sleep 10

echo ""
echo "=========================================="
echo "Verificando archivos JSON generados..."
echo "=========================================="

# Verificar cada JSON
for json in monitor files processes alerts quarantine; do
    echo ""
    echo "[✓] Contenido de /tmp/daemon_${json}.json:"
    if [ -f "/tmp/daemon_${json}.json" ]; then
        echo "─────────────────────────────────────────"
        cat "/tmp/daemon_${json}.json" | head -20
        echo "─────────────────────────────────────────"
    else
        echo "[✗] Archivo NO encontrado"
    fi
done

echo ""
echo "[4] Verificando logs..."
if [ -f "logs/daemon.log" ]; then
    echo "─────────────────────────────────────────"
    echo "Últimas líneas del log:"
    tail -30 logs/daemon.log
    echo "─────────────────────────────────────────"
else
    echo "[✗] Log no encontrado"
fi

# Limpiar
echo ""
echo "[5] Limpiando ambiente de prueba..."
kill $DAEMON_PID 2>/dev/null || true
wait $DAEMON_PID 2>/dev/null || true
cd /
rm -rf "$TEST_DIR"

echo ""
echo "=========================================="
echo "Prueba completada"
echo "=========================================="
