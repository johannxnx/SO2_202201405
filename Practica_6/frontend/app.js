const API_URL = "http://localhost:3000/api/monitor";

let graficaMemoria;
let graficaFallos;
let graficaProcesos;
let graficaPaginas;
let graficaEvolucion;

// Historial de datos para gráfico de evolución (máximo 30 puntos)
let historicalData = {
  timestamps: [],
  memoriaUsada: [],
  memoriaLibre: [],
  swapUsada: []
};

function actualizarVista(data) {
  document.getElementById("memoria_usada").textContent = `${data.memoria_usada} KB`;
  document.getElementById("memoria_libre").textContent = `${data.memoria_libre} KB`;
  document.getElementById("memoria_cache").textContent = `${data.memoria_cache} KB`;
  document.getElementById("swap_usada").textContent = `${data.swap_usada} KB`;
  document.getElementById("fallos_menores").textContent = data.fallos_menores;
  document.getElementById("fallos_mayores").textContent = data.fallos_mayores;
  document.getElementById("paginas_activas").textContent = data.paginas_activas;
  document.getElementById("paginas_inactivas").textContent = data.paginas_inactivas;

  const tabla = document.getElementById("tabla_procesos");
  tabla.innerHTML = "";

  data.procesos_top.forEach(proc => {
    const fila = document.createElement("tr");
    fila.innerHTML = `
      <td>${proc.pid}</td>
      <td>${proc.nombre}</td>
      <td>${proc.mem_kb}</td>
      <td>${proc.mem_percent}%</td>
    `;
    tabla.appendChild(fila);
  });

  const fecha = new Date(data.timestamp * 1000);
  document.getElementById("status").textContent =
    `Última actualización: ${fecha.toLocaleString()}`;

  crearGraficas(data);
}

function crearGraficas(data) {
  const ctxMemoria = document.getElementById("grafica_memoria");
  const ctxFallos = document.getElementById("grafica_fallos");
  const ctxProcesos = document.getElementById("grafica_procesos");
  const ctxPaginas = document.getElementById("grafica_paginas");
  const ctxEvolucion = document.getElementById("grafica_evolucion");

  if (graficaMemoria) graficaMemoria.destroy();
  if (graficaFallos) graficaFallos.destroy();
  if (graficaProcesos) graficaProcesos.destroy();
  if (graficaPaginas) graficaPaginas.destroy();
  if (graficaEvolucion) graficaEvolucion.destroy();

  // Actualizar historial (máximo 30 puntos)
  const hora = new Date(data.timestamp * 1000).toLocaleTimeString();
  historicalData.timestamps.push(hora);
  historicalData.memoriaUsada.push(data.memoria_usada);
  historicalData.memoriaLibre.push(data.memoria_libre);
  historicalData.swapUsada.push(data.swap_usada);

  if (historicalData.timestamps.length > 30) {
    historicalData.timestamps.shift();
    historicalData.memoriaUsada.shift();
    historicalData.memoriaLibre.shift();
    historicalData.swapUsada.shift();
  }

  graficaMemoria = new Chart(ctxMemoria, {
    type: "pie",
    data: {
      labels: ["Memoria usada", "Memoria libre", "Memoria cache", "Swap usada"],
      datasets: [{
        data: [
          data.memoria_usada,
          data.memoria_libre,
          data.memoria_cache,
          data.swap_usada
        ],
        backgroundColor: [
          '#ff6384',
          '#36a2eb',
          '#ffce56',
          '#4bc0c0'
        ]
      }]
    },
    options: {
      responsive: true,
      plugins: {
        legend: {
          labels: {
            color: "#f5f5f5"
          }
        }
      }
    }
  });

  graficaFallos = new Chart(ctxFallos, {
    type: "bar",
    data: {
      labels: ["Fallos menores", "Fallos mayores"],
      datasets: [{
        label: "Cantidad",
        data: [data.fallos_menores, data.fallos_mayores],
        backgroundColor: '#36a2eb'
      }]
    },
    options: {
      responsive: true,
      scales: {
        x: {
          ticks: { color: "#f5f5f5" },
          grid: { color: "#2b3744" }
        },
        y: {
          ticks: { color: "#f5f5f5" },
          grid: { color: "#2b3744" }
        }
      },
      plugins: {
        legend: {
          labels: {
            color: "#f5f5f5"
          }
        }
      }
    }
  });

  graficaPaginas = new Chart(ctxPaginas, {
    type: "pie",
    data: {
      labels: ["Páginas activas", "Páginas inactivas"],
      datasets: [{
        data: [data.paginas_activas, data.paginas_inactivas],
        backgroundColor: ['#4bc0c0', '#ff9f40']
      }]
    },
    options: {
      responsive: true,
      plugins: {
        legend: {
          labels: {
            color: "#f5f5f5"
          }
        }
      }
    }
  });

  graficaEvolucion = new Chart(ctxEvolucion, {
    type: "line",
    data: {
      labels: historicalData.timestamps,
      datasets: [
        {
          label: "Memoria usada (KB)",
          data: historicalData.memoriaUsada,
          borderColor: '#ff6384',
          backgroundColor: 'rgba(255, 99, 132, 0.1)',
          tension: 0.4
        },
        {
          label: "Memoria libre (KB)",
          data: historicalData.memoriaLibre,
          borderColor: '#36a2eb',
          backgroundColor: 'rgba(54, 162, 235, 0.1)',
          tension: 0.4
        },
        {
          label: "Swap usada (KB)",
          data: historicalData.swapUsada,
          borderColor: '#4bc0c0',
          backgroundColor: 'rgba(75, 192, 192, 0.1)',
          tension: 0.4
        }
      ]
    },
    options: {
      responsive: true,
      scales: {
        x: {
          ticks: { color: "#f5f5f5" },
          grid: { color: "#2b3744" }
        },
        y: {
          ticks: { color: "#f5f5f5" },
          grid: { color: "#2b3744" }
        }
      },
      plugins: {
        legend: {
          labels: {
            color: "#f5f5f5"
          }
        }
      }
    }
  });

  graficaProcesos = new Chart(ctxProcesos, {
    type: "bar",
    data: {
      labels: data.procesos_top.map(p => `${p.nombre} (${p.pid})`),
      datasets: [{
        label: "Memoria (KB)",
        data: data.procesos_top.map(p => p.mem_kb),
        backgroundColor: '#ff9f40'
      }]
    },
    options: {
      indexAxis: "y",
      responsive: true,
      scales: {
        x: {
          ticks: { color: "#f5f5f5" },
          grid: { color: "#2b3744" }
        },
        y: {
          ticks: { color: "#f5f5f5" },
          grid: { color: "#2b3744" }
        }
      },
      plugins: {
        legend: {
          labels: {
            color: "#f5f5f5"
          }
        }
      }
    }
  });
}

async function obtenerDatos() {
  try {
    const res = await fetch(API_URL);
    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }

    const data = await res.json();
    actualizarVista(data);
  } catch (error) {
    document.getElementById("status").textContent =
      "Error al obtener datos del backend";
    console.error("Error en fetch:", error);
  }
}

// Función para consultar proceso específico por PID
async function consultarProceso() {
  const pidInput = document.getElementById("pid_input").value;
  
  if (!pidInput || isNaN(pidInput)) {
    alert("Por favor ingresa un PID válido");
    return;
  }

  try {
    const res = await fetch(`${API_URL}?pid=${pidInput}`);
    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }

    const data = await res.json();
    
    if (data.error) {
      document.getElementById("info_proceso").textContent = `Error: ${data.error}`;
    } else {
      const infoProceso = document.getElementById("info_proceso");
      infoProceso.innerHTML = `
        <strong>Información del proceso PID ${pidInput}:</strong><br>
        Nombre: ${data.nombre || 'N/A'}<br>
        Memoria: ${data.memoria_kb || 'N/A'} KB<br>
        % Memoria: ${data.memoria_percent || 'N/A'}%<br>
        Estado: ${data.estado || 'N/A'}
      `;
    }
    
    document.getElementById("info_proceso").style.display = "block";
  } catch (error) {
    document.getElementById("info_proceso").textContent = "Error al consultar el proceso";
    document.getElementById("info_proceso").style.display = "block";
    console.error("Error:", error);
  }
}

// Event listeners para el botón de consulta
document.addEventListener("DOMContentLoaded", () => {
  const btnConsultar = document.getElementById("btn_consultar");
  const pidInput = document.getElementById("pid_input");
  
  if (btnConsultar) {
    btnConsultar.addEventListener("click", consultarProceso);
  }
  
  if (pidInput) {
    pidInput.addEventListener("keypress", (e) => {
      if (e.key === "Enter") {
        consultarProceso();
      }
    });
  }
});

obtenerDatos();
setInterval(obtenerDatos, 5000);