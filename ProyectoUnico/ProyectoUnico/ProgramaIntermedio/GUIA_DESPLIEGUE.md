# 🚀 GUÍA DE DESPLIEGUE - Sistema de Seguridad

## 1️⃣ Ambiente de Producción (Terminal 1: Daemon)

```bash
# Compilar
cd ~/Escritorio/mapa/ProyectoUnico/ProyectoUnico/ProgramaIntermedio
make clean
make

# Crear directorios necesarios
mkdir -p config/monitor
mkdir -p logs
mkdir -p ~/.security_daemon

# (OPCIONAL) Crear archivo de configuración
cat > config/hash_blacklist.json << 'EOF'
{
  "firmas": [
    {
      "hash": "malware_hash_here",
      "nombre": "Trojano.Generic",
      "severidad": "HIGH",
      "descripcion": "Archivo malicioso conocido"
    }
  ]
}
EOF

# Ejecutar daemon (requeire sudo para syscalls)
sudo ./daemon_monitor

# O ejecutar en background:
sudo ./daemon_monitor &

# Ver logs en tiempo real:
tail -f logs/daemon.log
```

## 2️⃣ API REST (Terminal 2: Backend)

```bash
# Instalar dependencias (solo primera vez)
cd ~/Escritorio/mapa/ProyectoUnico/ProyectoUnico/ProgramaIntermedio/backend
npm install

# Iniciar servidor
npm start

# El API estará disponible en: http://localhost:3000
```

## 3️⃣ Frontend Dashboard (Terminal 3 o Browser)

```bash
# Opción A: Servir con Python (simple)
cd ~/Escritorio/mapa/ProyectoUnico/ProyectoUnico/ProgramaIntermedio/frontend
python3 -m http.server 8080

# Acceder en: http://localhost:8080/index.html

# Opción B: Servir con Node.js
npx http-server -p 8080

# Opción C: Archivo local (no recomendado para cors)
file:///home/johan/Escritorio/mapa/ProyectoUnico/ProyectoUnico/ProgramaIntermedio/frontend/index.html
```

---

## 📊 Verificar que Todo Funciona

### 1. Daemon generando JSONs

```bash
# En otra terminal, verificar archivos:
watch -n 2 'ls -lah /tmp/daemon_*.json'

# Ver contenido:
cat /tmp/daemon_monitor.json | jq '.'
cat /tmp/daemon_alerts.json | jq '.'
cat /tmp/daemon_quarantine.json | jq '.'
```

### 2. API respondiendo

```bash
# Probar endpoints
curl http://localhost:3000/api/monitor
curl http://localhost:3000/api/files
curl http://localhost:3000/api/alerts
curl http://localhost:3000/api/quarantine/list

# Con formato pretty:
curl -s http://localhost:3000/api/monitor | jq '.'
```

### 3. Frontend cargando datos

- Abrir navegador en http://localhost:8080 (o puerto configurado)
- Debería mostrar métricas que se actualizan cada 5 segundos
- Todos los JSONs aparecerán en las pestañas correspondientes

---

## 🧪 Probar Detección de Amenazas

```bash
# 1. Crear archivo de prueba en monitor
echo "contenido_sospechoso" > config/monitor/test.txt

# 2. Agregar hash a blacklist para simular detección
# (Nota: necesitas cambiar hash_blacklist.json con el hash del archivo)

# 3. Ejecutar escaneo manual (el daemon escanea cada 7 segundos)
# Esperar o forzar otro ciclo

# 4. Verificar alertas:
cat /tmp/daemon_alerts.json | jq '.alertas'

# 5. Verificar cuarentena:
cat /tmp/daemon_quarantine.json | jq '.archivos'
```

---

## 🔐 Instalar como Servicio Systemd (Opcional)

```bash
# Crear archivo de servicio
sudo nano /etc/systemd/system/security-daemon.service
```

Contenido:
```ini
[Unit]
Description=Security Daemon Service
After=network.target
Requires=security-api.service

[Service]
Type=simple
User=root
WorkingDirectory=/home/johan/Escritorio/mapa/ProyectoUnico/ProyectoUnico/ProgramaIntermedio
ExecStart=/home/johan/Escritorio/mapa/ProyectoUnico/ProyectoUnico/ProgramaIntermedio/daemon_monitor
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

```bash
# Crear servicio para API
sudo nano /etc/systemd/system/security-api.service
```

Contenido:
```ini
[Unit]
Description=Security API Service
After=network.target

[Service]
Type=simple
User=node
WorkingDirectory=/home/johan/Escritorio/mapa/ProyectoUnico/ProyectoUnico/ProgramaIntermedio/backend
ExecStart=/usr/bin/npm start
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Activar servicios:
```bash
sudo systemctl daemon-reload
sudo systemctl enable security-daemon.service
sudo systemctl enable security-api.service
sudo systemctl start security-daemon.service
sudo systemctl start security-api.service

# Ver estado:
sudo systemctl status security-daemon.service
sudo systemctl status security-api.service

# Ver logs:
sudo journalctl -u security-daemon.service -f
sudo journalctl -u security-api.service -f
```

---

## 📈 Monitoreo en Vivo

```bash
# Terminal nueva: Ver actualizaciones de JSONs en tiempo real
watch -n 1 'echo "=== ALERTS ===" && cat /tmp/daemon_alerts.json | jq ".alertas | length" && echo "=== QUARANTINE ===" && cat /tmp/daemon_quarantine.json | jq ".total"'

# O usar tmux para múltiples vistas:
tmux new-session -d -s security
tmux send-keys -t security "cd ~/Escritorio/mapa/ProyectoUnico/ProyectoUnico/ProgramaIntermedio && tail -f logs/daemon.log" Enter
tmux split-window -h -t security
tmux send-keys -t security "watch -n 2 'ls -lah /tmp/daemon_*.json'" Enter
tmux attach -t security
```

---

## 🔧 Troubleshooting

### Daemon no inicia
```bash
# Ver error completo
sudo ./daemon_monitor 2>&1

# Verificar permisos
ls -la daemon_monitor
chmod +x daemon_monitor

# Verificar directorios
mkdir -p config logs
```

### API no responde
```bash
# Verificar Node.js instalado
node --version
npm --version

# Reinstalar dependencias
cd backend && npm install

# Limpiar y reiniciar
pkill node
npm start
```

### JSONs no se generan
```bash
# Verificar permisos en /tmp
ls -la /tmp/daemon_*

# Daemon debe estar corriendo
ps aux | grep daemon_monitor

# Ver logs del daemon
tail -100 logs/daemon.log

# Crear directorio si no existe
sudo mkdir -p config/monitor
sudo chmod 777 config/monitor
```

### Frontend no carga datos
```bash
# Verificar CORS habilitado
curl -H "Origin: http://localhost:8080" -H "Access-Control-Request-Method: GET" http://localhost:3000

# Verificar API activo
curl http://localhost:3000/api/monitor

# Abrir consola del navegador (F12) - Ver errores
```

---

## 📞 Contacto / Soporte

- **Kernel:** `/home/johan/Escritorio/Practica_4/src/linux-6.12.69/`
- **Daemon Fuente:** `/home/johan/Escritorio/mapa/ProyectoUnico/.../ProgramaIntermedio/`
- **API Fuente:** `/home/johan/Escritorio/mapa/.../backend/`
- **Logs Principal:** `/home/johan/Escritorio/mapa/.../logs/daemon.log`

---

**Versión:** 1.0.0  
**Última Actualización:** 20 de Abril 2026  
**Estado:** ✅ LISTO PARA PRODUCCIÓN
