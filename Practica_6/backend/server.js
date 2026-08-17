const express = require("express");
const cors = require("cors");
const fs = require("fs");

const app = express();
const PORT = 3000;
const JSON_PATH = "/tmp/system_monitor.json";

app.use(cors());
app.use(express.json());

app.get("/", (req, res) => {
  res.json({
    mensaje: "Backend de monitoreo activo",
    endpoints: [
      "/api/monitor - Obtener datos de monitoreo general",
      "/api/monitor?pid=<numero> - Obtener información de un proceso específico",
      "/api/health - Verificar estado del backend"
    ]
  });
});

app.get("/api/health", (req, res) => {
  res.json({
    status: "ok",
    backend: "running"
  });
});

app.get("/api/monitor", (req, res) => {
  const pidQuery = req.query.pid;

  fs.readFile(JSON_PATH, "utf8", (err, data) => {
    if (err) {
      return res.status(500).json({
        error: "No se pudo leer el archivo JSON del monitoreo",
        detalle: err.message
      });
    }

    try {
      const parsed = JSON.parse(data);

      // Si se proporciona un PID, buscar ese proceso específico
      if (pidQuery) {
        const pidNum = parseInt(pidQuery);
        
        // Buscar en procesos top
        if (parsed.procesos_top) {
          const proceso = parsed.procesos_top.find(p => p.pid === pidNum);
          if (proceso) {
            return res.json({
              pid: proceso.pid,
              nombre: proceso.nombre,
              memoria_kb: proceso.mem_kb,
              memoria_percent: proceso.mem_percent,
              estado: "en ejecución"
            });
          }
        }

        // Si no se encuentra, retornar error
        return res.status(404).json({
          error: `Proceso con PID ${pidNum} no encontrado`,
          detalle: "El PID no está entre los procesos monitoreados"
        });
      }

      return res.json(parsed);
    } catch (parseError) {
      return res.status(500).json({
        error: "El archivo JSON está corrupto o mal formado",
        detalle: parseError.message
      });
    }
  });
});

app.listen(PORT, "0.0.0.0", () => {
  console.log(`Backend escuchando en http://0.0.0.0:${PORT}`);
});