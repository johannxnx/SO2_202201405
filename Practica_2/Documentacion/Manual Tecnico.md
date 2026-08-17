# Práctica 2 – Exploración del Kernel y Laboratorio de Módulos Básicos

## 1. Datos del Estudiante

* **Nombre:** Johan Moises Cardona Rosales
* **Carnet:** 202201405
* **Curso:** Sistemas Operativos 2
* **Fecha:** 11 de Febrero del 2026

## 2. Introducción

El kernel Linux es el núcleo del sistema operativo y es responsable de administrar los recursos del hardware, gestionar procesos y controlar la interacción entre el sistema y los programas de usuario.

En esta práctica se desarrolló un módulo básico del kernel en lenguaje C con el objetivo de comprender su estructura, proceso de compilación y ciclo de vida (carga y descarga dinámica).

## 3. Objetivo

Crear, compilar y ejecutar un módulo del kernel Linux que:

* Se compile utilizando un Makefile.
* Genere un archivo `.ko`.
* Pueda cargarse con `insmod`.
* Pueda descargarse con `rmmod`.
* Registre mensajes en el sistema mediante `printk` y `dmesg`.

## 4. Entorno de Desarrollo

* **Sistema Operativo:** GNU/Linux (Ubuntu basado en Debian)
* **Kernel:** 6.14.0-33-generic
* **Compilador:** GCC 13.3.0
* **Herramientas utilizadas:**
   * `make`
   * `gcc`
   * `insmod`
   * `rmmod`
   * `lsmod`
   * `dmesg`

## 5. Desarrollo del Módulo

El módulo fue desarrollado en lenguaje C utilizando la API del kernel Linux.

### Código Principal

El módulo contiene:

* `module_init()` → función ejecutada al cargar el módulo.
* `module_exit()` → función ejecutada al descargar el módulo.
* `printk()` → para registrar mensajes en el buffer del kernel.

El módulo imprime un mensaje cuando es cargado y otro cuando es descargado.

## 6. Proceso de Compilación

Se utilizó un Makefile para compilar el módulo mediante el sistema de construcción del kernel.

**Comandos utilizados:**
```bash
make clean
make
```

**Resultado:**

![alt text](<Captura desde 2026-02-11 08-03-08.png>)

* Se generó correctamente el archivo:
```
modulo_202201405.ko
```

![alt text](<Captura desde 2026-02-11 08-04-22.png>)

Con eso logramos verificar que se geberó el módulo correctamente.

## 7. Carga del Módulo

Para cargar el módulo se utilizó:
```bash
sudo insmod modulo_202201405.ko
```

**Verificación:**
```bash
lsmod | grep modulo_202201405
```

**Resultado:**

![alt text](<Captura desde 2026-02-11 08-07-48.png>)

El módulo aparece listado como cargado en el kernel.

## 8. Verificación con dmesg

Para visualizar los mensajes del kernel:
```bash
sudo dmesg | tail
```

![alt text](<Captura desde 2026-02-11 08-08-36.png>)

**Se obtuvo:**
```
Modulo 202201405: cargado correctamente en el kernel.
```

**Nota técnica:** El sistema requiere privilegios de superusuario para acceder al buffer del kernel mediante `dmesg`, por razones de seguridad.

## 9. Descarga del Módulo

Para descargar el módulo:
```bash
sudo rmmod modulo_202201405
```


**Verificación:**
```bash
sudo dmesg | tail
```

**Resultado:**

![alt text](<Captura desde 2026-02-11 08-09-50.png>)

```
Modulo 202201405: descargado correctamente del kernel.
```

Esto confirma el correcto ciclo de vida del módulo.



## Conclusiones

- make

- ls -l *.ko

- sudo insmod

- lsmod

- dmesg (carga)

- sudo rmmod

- dmesg (descarga)

-----------



# Investigación: Compilación de un Kernel Linux Personalizado

## Introducción

El kernel Linux puede ser compilado de manera personalizada para:

* Optimizar rendimiento
* Eliminar controladores innecesarios
* Agregar soporte específico de hardware
* Aplicar parches personalizados
* Realizar pruebas de desarrollo

Compilar un kernel personalizado permite modificar el núcleo del sistema operativo en lugar de solo agregar módulos externos.

## Proceso General de Compilación

El proceso se realiza en un entorno GNU/Linux y requiere herramientas de desarrollo.

## Instalación de Dependencias
```bash
sudo apt update
sudo apt install build-essential libncurses-dev bison flex libssl-dev libelf-dev
```

Estas herramientas permiten compilar el kernel y configurar sus opciones.

## Descargar el Código Fuente del Kernel

Se puede descargar desde:

https://www.kernel.org

O mediante:
```bash
wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.x.tar.xz
```

Luego:
```bash
tar -xf linux-6.x.tar.xz
cd linux-6.x
```

## Configuración del Kernel

Antes de compilar, se debe configurar qué componentes incluir.

**Método interactivo:**
```bash
make menuconfig
```

Permite:

* Activar/desactivar drivers
* Habilitar sistemas de archivos
* Configurar seguridad
* Seleccionar arquitectura

La configuración se guarda en el archivo:
```
.config
```

## Compilación del Kernel
```bash
make -j$(nproc)
```

Este comando compila usando todos los núcleos del procesador.

Luego:
```bash
sudo make modules_install
sudo make install
```

Esto:

* Instala los módulos
* Copia el nuevo kernel a `/boot`
* Actualiza el gestor de arranque (GRUB)

## Reinicio del Sistema

Después de la instalación:
```bash
sudo reboot
```

En el menú de GRUB se puede seleccionar el nuevo kernel.

**Verificar versión:**
```bash
uname -r
```


# Conclusiones Generales

1. La práctica permitió comprender de manera práctica la estructura y funcionamiento interno del kernel Linux, diferenciando claramente entre el espacio de usuario y el espacio de kernel.

2. Se logró desarrollar un módulo del kernel utilizando la API oficial, empleando funciones como `module_init`, `module_exit` y `printk`, lo que evidenció el uso correcto de herramientas propias del entorno de desarrollo del kernel.

3. El proceso de compilación mediante `make` y un Makefile específico para módulos demostró la importancia del sistema de construcción del kernel y su integración con los headers correspondientes.

4. La carga dinámica del módulo con `insmod` y su posterior eliminación con `rmmod` permitió observar el ciclo de vida completo de un módulo del kernel, confirmando su correcta ejecución mediante los registros en `dmesg`.

5. Se comprendió que los mensajes como "module verification failed" y "File exists" forman parte del comportamiento normal del sistema bajo ciertas condiciones, lo cual fortaleció el análisis técnico y la interpretación de resultados.

6. La implementación adicional para registrar información como UID, fecha y hora evidenció un uso más avanzado de la API del kernel, demostrando mayor comprensión de las estructuras internas del sistema.