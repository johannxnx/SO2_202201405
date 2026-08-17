/* Librerías principales del kernel */
#include <linux/module.h>      // Permite crear módulos del kernel
#include <linux/kernel.h>      // Contiene funciones básicas como printk
#include <linux/init.h>        // Macros para funciones de inicio y salida
#include <linux/cred.h>        // Permite acceder a credenciales (UID)
#include <linux/timekeeping.h> // Funciones para obtener tiempo del kernel
#include <linux/time.h>        // Estructuras para manejo de fecha y hora

/* Información del módulo */
MODULE_LICENSE("GPL"); // Licencia requerida para evitar advertencias del kernel
MODULE_AUTHOR("Johan Moises Cardona Rosales");
MODULE_DESCRIPTION("Modulo avanzado del kernel - Practica 2 SO2"); // Descripción
MODULE_VERSION("1.1");                                             // Versión del módulo

/*
 * Función que se ejecuta cuando el módulo es cargado con insmod.
 * La macro __init indica que esta función se usa solo durante la carga
 * y puede liberarse de memoria después.
 */
static int __init inicio_modulo(void)
{
    /* Estructuras para manejo del tiempo */
    struct timespec64 ts; // Guarda tiempo en segundos y nanosegundos
    struct tm tm;         // Estructura para fecha legible (día, mes, año, hora)
    kuid_t uid;           // Tipo especial del kernel para representar UID

    /* Obtener tiempo actual del sistema */
    ktime_get_real_ts64(&ts); // Obtiene tiempo real del kernel
    time64_to_tm(ts.tv_sec, 0, &tm);
    // Convierte segundos en formato calendario (día, mes, año, etc.)

    /* Obtener usuario actual que ejecuta la carga */
    uid = current_uid(); // Obtiene el UID del proceso actual

    /* Mensajes enviados al buffer del kernel */
    printk(KERN_INFO "=============================================\n");
    printk(KERN_INFO "Modulo 202201405 cargado correctamente.\n");

    /* Mostrar UID convirtiéndolo a entero */
    printk(KERN_INFO "Usuario UID: %u\n", __kuid_val(uid));

    /* Mostrar fecha actual */
    printk(KERN_INFO "Fecha: %02d-%02d-%04ld\n",
           tm.tm_mday,       // Día
           tm.tm_mon + 1,    // Mes (se suma 1 porque empieza en 0)
           tm.tm_year + 1900 // Año (se suma 1900 por convención UNIX)
    );

    /* Mostrar hora actual */
    printk(KERN_INFO "Hora: %02d:%02d:%02d\n",
           tm.tm_hour,
           tm.tm_min,
           tm.tm_sec);

    printk(KERN_INFO "=============================================\n");

    return 0; // Retornar 0 indica carga exitosa
}

/*
 * Función que se ejecuta cuando el módulo es descargado con rmmod.
 * La macro __exit indica que esta función solo se usa al descargar.
 */
static void __exit salida_modulo(void)
{
    printk(KERN_INFO "Modulo 202201405 descargado correctamente.\n");
}

/* Registrar funciones de inicio y salida */
module_init(inicio_modulo); // Se ejecuta al hacer insmod
module_exit(salida_modulo); // Se ejecuta al hacer rmmod
