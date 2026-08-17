#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <microhttpd.h>
#include <jansson.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <grp.h>
#include <time.h>
#include "syscalls_wrapper.h"
#include "quarantine_persistent.h"

#define PORT 3000
#define FRONTEND_DIR "./frontend"
#define JSON_DIR "/tmp"
#define SCAN_STATE_FILE "/tmp/daemon_scan_state"

struct post_context {
    char data[8192];
    size_t size;
};

static enum MHD_Result send_json_response(struct MHD_Connection* connection, int status, json_t* payload) {
    char* response = json_dumps(payload, JSON_COMPACT);
    struct MHD_Response* resp = MHD_create_response_from_buffer(
        strlen(response), (void*)response, MHD_RESPMEM_MUST_FREE);

    MHD_add_response_header(resp, "Content-Type", "application/json");
    MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
    enum MHD_Result ret = MHD_queue_response(connection, status, resp);
    MHD_destroy_response(resp);
    return ret;
}

static int is_admin_request(struct MHD_Connection* connection) {
    const char* role = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-User-Role");
    return role != NULL && strcmp(role, "admin_user") == 0;
}

static enum MHD_Result unauthorized_admin_required(struct MHD_Connection* connection) {
    json_t* resp = json_object();
    json_object_set_new(resp, "success", json_false());
    json_object_set_new(resp, "error", json_string("Permiso denegado: se requiere rol admin_user"));
    enum MHD_Result ret = send_json_response(connection, MHD_HTTP_FORBIDDEN, resp);
    json_decref(resp);
    return ret;
}

static int set_scan_state(int enabled) {
    FILE* f = fopen(SCAN_STATE_FILE, "w");
    if (!f) {
        return -1;
    }
    fprintf(f, "%d\n", enabled ? 1 : 0);
    fclose(f);
    return 0;
}

static int get_scan_state(void) {
    FILE* f = fopen(SCAN_STATE_FILE, "r");
    int value = 1;

    if (!f) {
        return 1;
    }

    if (fscanf(f, "%d", &value) != 1) {
        value = 1;
    }
    fclose(f);
    return value ? 1 : 0;
}

// ============ AUTENTICACIÓN CON PAM ============

// Estructura para pasar credenciales al callback de PAM
struct pam_credentials {
    const char* username;
    const char* password;
};

// Callback para PAM
static int pam_conversation(int num_msg, const struct pam_message** msg,
                           struct pam_response** resp, void* appdata_ptr) {
    struct pam_credentials* cred = (struct pam_credentials*) appdata_ptr;
    struct pam_response* response = NULL;
    
    if (!cred || !msg || !resp) {
        return PAM_CONV_ERR;
    }
    
    response = malloc(num_msg * sizeof(struct pam_response));
    if (!response) {
        return PAM_CONV_ERR;
    }
    
    for (int i = 0; i < num_msg; i++) {
        response[i].resp_retcode = 0;
        response[i].resp = NULL;
        
        switch (msg[i]->msg_style) {
            case PAM_PROMPT_ECHO_OFF:
            case PAM_PROMPT_ECHO_ON:
                if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
                    // Solicitud de contraseña
                    response[i].resp = malloc(strlen(cred->password) + 1);
                    strcpy(response[i].resp, cred->password);
                } else {
                    // Solicitud de usuario
                    response[i].resp = malloc(strlen(cred->username) + 1);
                    strcpy(response[i].resp, cred->username);
                }
                break;
            case PAM_ERROR_MSG:
            case PAM_TEXT_INFO:
                break;
            default:
                free(response);
                return PAM_CONV_ERR;
        }
    }
    
    *resp = response;
    return PAM_SUCCESS;
}

// Autenticar con PAM
int authenticate_pam(const char* username, const char* password) {
    pam_handle_t* handle = NULL;
    int ret;
    
    struct pam_credentials cred = {
        .username = username,
        .password = password
    };
    
    struct pam_conv conv = {
        .conv = pam_conversation,
        .appdata_ptr = &cred
    };
    
    // Iniciar PAM con el servicio de login estándar
    ret = pam_start("login", username, &conv, &handle);
    if (ret != PAM_SUCCESS) {
        fprintf(stderr, "pam_start fallido: %s\n", pam_strerror(handle, ret));
        return 0;
    }
    
    // Intentar autenticación
    ret = pam_authenticate(handle, 0);
    int auth_success = (ret == PAM_SUCCESS);
    if (!auth_success) {
        fprintf(stderr, "pam_authenticate fallido para %s: %s\n", username, pam_strerror(handle, ret));
    }
    
    // Liberar recursos
    pam_end(handle, ret);
    
    return auth_success;
}

// Obtener rol del usuario (basado en grupos)
const char* get_user_role(const char* username) {
    struct passwd* pwd = getpwnam(username);
    if (!pwd) {
        return "common_user";
    }
    
    // Verificar si el usuario está en grupo "sudo" o "admin"
    struct group* grp = getgrnam("sudo");
    if (grp) {
        for (int i = 0; grp->gr_mem[i]; i++) {
            if (strcmp(grp->gr_mem[i], username) == 0) {
                return "admin_user";
            }
        }
    }
    
    grp = getgrnam("admin");
    if (grp) {
        for (int i = 0; grp->gr_mem[i]; i++) {
            if (strcmp(grp->gr_mem[i], username) == 0) {
                return "admin_user";
            }
        }
    }
    
    return "common_user";
}

// ============ UTILIDADES PARA LEER ARCHIVOS ============

char* read_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    
    return buf;
}

// Leer un JSON del disco y retornar el objeto json_t
json_t* read_json_file(const char* filename) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", JSON_DIR, filename);
    
    char* content = read_file(path);
    if (!content) {
        return NULL;
    }
    
    json_error_t error;
    json_t* root = json_loads(content, 0, &error);
    free(content);
    
    return root;
}

// ============ MANEJADORES DE ENDPOINTS ============

// POST /api/login
enum MHD_Result handle_login(struct MHD_Connection* connection,
                           const char* upload_data,
                           size_t* upload_data_size,
                           void** con_cls) {

    struct post_context* ctx = (struct post_context*)*con_cls;
    
    // Primera llamada
    if (ctx == NULL) {
        ctx = malloc(sizeof(struct post_context));
        ctx->size = 0;
        *con_cls = (void*)ctx;
        return MHD_YES;
    }
    
    // Acumular datos POST
    if (*upload_data_size > 0) {
        if (ctx->size + *upload_data_size > 8191) {
            *upload_data_size = 8191 - ctx->size;
        }
        memcpy(ctx->data + ctx->size, upload_data, *upload_data_size);
        ctx->size += *upload_data_size;
        *upload_data_size = 0;
        return MHD_YES;
    }
    
    // Procesar datos cuando termina el upload
    ctx->data[ctx->size] = '\0';
    
    json_error_t error;
    json_t* root = json_loads(ctx->data, 0, &error);
    
    if (!root) {
        json_t* resp = json_object();
        json_object_set_new(resp, "success", json_false());
        json_object_set_new(resp, "error", json_string("JSON inválido"));
        
        char* response = json_dumps(resp, JSON_COMPACT);
        struct MHD_Response* mhd_resp = MHD_create_response_from_buffer(
            strlen(response), (void*)response, MHD_RESPMEM_MUST_FREE);
        
        MHD_add_response_header(mhd_resp, "Content-Type", "application/json");
        MHD_add_response_header(mhd_resp, "Access-Control-Allow-Origin", "*");
        int ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, mhd_resp);
        MHD_destroy_response(mhd_resp);
        json_decref(resp);
        
        free(ctx);
        *con_cls = NULL;
        return ret;
    }
    
    const char* username = json_string_value(json_object_get(root, "username"));
    const char* password = json_string_value(json_object_get(root, "password"));
    
    if (!username || !password) {
        json_t* resp = json_object();
        json_object_set_new(resp, "success", json_false());
        json_object_set_new(resp, "error", json_string("Usuario y contraseña requeridos"));
        
        char* response = json_dumps(resp, JSON_COMPACT);
        struct MHD_Response* mhd_resp = MHD_create_response_from_buffer(
            strlen(response), (void*)response, MHD_RESPMEM_MUST_FREE);
        
        MHD_add_response_header(mhd_resp, "Content-Type", "application/json");
        MHD_add_response_header(mhd_resp, "Access-Control-Allow-Origin", "*");
        int ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, mhd_resp);
        MHD_destroy_response(mhd_resp);
        json_decref(resp);
        json_decref(root);
        
        free(ctx);
        *con_cls = NULL;
        return ret;
    }
    
    // Autenticar con PAM
    int auth_ok = authenticate_pam(username, password);
    
    if (!auth_ok) {
        json_t* resp = json_object();
        json_object_set_new(resp, "success", json_false());
        json_object_set_new(resp, "error", json_string("Usuario o contraseña incorrectos"));
        
        char* response = json_dumps(resp, JSON_COMPACT);
        struct MHD_Response* mhd_resp = MHD_create_response_from_buffer(
            strlen(response), (void*)response, MHD_RESPMEM_MUST_FREE);
        
        MHD_add_response_header(mhd_resp, "Content-Type", "application/json");
        MHD_add_response_header(mhd_resp, "Access-Control-Allow-Origin", "*");
        int ret = MHD_queue_response(connection, MHD_HTTP_UNAUTHORIZED, mhd_resp);
        MHD_destroy_response(mhd_resp);
        json_decref(resp);
        json_decref(root);
        
        free(ctx);
        *con_cls = NULL;
        return ret;
    }
    
    // Autenticación exitosa
    const char* role = get_user_role(username);
    
    json_t* resp = json_object();
    json_object_set_new(resp, "success", json_true());
    json_object_set_new(resp, "username", json_string(username));
    json_object_set_new(resp, "role", json_string(role));
    json_object_set_new(resp, "token", json_string(""));
    
    char* response = json_dumps(resp, JSON_COMPACT);
    struct MHD_Response* mhd_resp = MHD_create_response_from_buffer(
        strlen(response), (void*)response, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(mhd_resp, "Content-Type", "application/json");
    MHD_add_response_header(mhd_resp, "Access-Control-Allow-Origin", "*");
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, mhd_resp);
    MHD_destroy_response(mhd_resp);
    json_decref(resp);
    json_decref(root);
    
    free(ctx);
    *con_cls = NULL;
    
    return ret;
}
enum MHD_Result handle_monitor(struct MHD_Connection* connection, void* cls) {
    json_t* data = read_json_file("daemon_monitor.json");
    
    if (!data) {
        // Respuesta por defecto mientras el daemon genera el JSON inicial.
        json_t* fallback = json_object();
        json_t* memoria = json_object();
        json_t* fallos = json_object();
        json_t* paginas = json_object();

        json_object_set_new(memoria, "usada", json_integer(0));
        json_object_set_new(memoria, "libre", json_integer(0));
        json_object_set_new(memoria, "cache", json_integer(0));
        json_object_set_new(memoria, "swap", json_integer(0));

        json_object_set_new(fallos, "menores", json_integer(0));
        json_object_set_new(fallos, "mayores", json_integer(0));

        json_object_set_new(paginas, "activas", json_integer(0));
        json_object_set_new(paginas, "inactivas", json_integer(0));

        json_object_set_new(fallback, "memoria", memoria);
        json_object_set_new(fallback, "fallos", fallos);
        json_object_set_new(fallback, "paginas", paginas);
        json_object_set_new(fallback, "timestamp", json_integer(time(NULL)));

        char* response = json_dumps(fallback, JSON_COMPACT);
        struct MHD_Response* resp = MHD_create_response_from_buffer(
            strlen(response), (void*)response, MHD_RESPMEM_MUST_FREE);
        
        MHD_add_response_header(resp, "Content-Type", "application/json");
        MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
        MHD_destroy_response(resp);
        json_decref(fallback);
        
        return ret;
    }
    
    char* response = json_dumps(data, JSON_COMPACT);
    struct MHD_Response* resp = MHD_create_response_from_buffer(
        strlen(response), (void*)response, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(resp, "Content-Type", "application/json");
    MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
    MHD_destroy_response(resp);
    json_decref(data);
    
    return ret;
}

// GET /api/files
enum MHD_Result handle_files(struct MHD_Connection* connection, void* cls) {
    json_t* data = read_json_file("daemon_files.json");
    
    if (!data) {
        json_t* error = json_object();
        json_object_set_new(error, "archivos", json_array());
        
        char* response = json_dumps(error, JSON_COMPACT);
        struct MHD_Response* resp = MHD_create_response_from_buffer(
            strlen(response), (void*)response, MHD_RESPMEM_MUST_FREE);
        
        MHD_add_response_header(resp, "Content-Type", "application/json");
        MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
        MHD_destroy_response(resp);
        json_decref(error);
        
        return ret;
    }
    
    char* response = json_dumps(data, JSON_COMPACT);
    struct MHD_Response* resp = MHD_create_response_from_buffer(
        strlen(response), (void*)response, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(resp, "Content-Type", "application/json");
    MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
    MHD_destroy_response(resp);
    json_decref(data);
    
    return ret;
}

// GET /api/processes
enum MHD_Result handle_processes(struct MHD_Connection* connection, void* cls) {
    json_t* data = read_json_file("daemon_processes.json");
    
    if (!data) {
        json_t* error = json_object();
        json_object_set_new(error, "procesos", json_array());
        
        char* response = json_dumps(error, JSON_COMPACT);
        struct MHD_Response* resp = MHD_create_response_from_buffer(
            strlen(response), (void*)response, MHD_RESPMEM_MUST_FREE);
        
        MHD_add_response_header(resp, "Content-Type", "application/json");
        MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
        MHD_destroy_response(resp);
        json_decref(error);
        
        return ret;
    }
    
    char* response = json_dumps(data, JSON_COMPACT);
    struct MHD_Response* resp = MHD_create_response_from_buffer(
        strlen(response), (void*)response, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(resp, "Content-Type", "application/json");
    MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
    MHD_destroy_response(resp);
    json_decref(data);
    
    return ret;
}

// GET /api/alerts
enum MHD_Result handle_alerts(struct MHD_Connection* connection, void* cls) {
    json_t* data = read_json_file("daemon_alerts.json");
    
    if (!data) {
        json_t* error = json_object();
        json_object_set_new(error, "alertas", json_array());
        
        char* response = json_dumps(error, JSON_COMPACT);
        struct MHD_Response* resp = MHD_create_response_from_buffer(
            strlen(response), (void*)response, MHD_RESPMEM_MUST_FREE);
        
        MHD_add_response_header(resp, "Content-Type", "application/json");
        MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
        MHD_destroy_response(resp);
        json_decref(error);
        
        return ret;
    }
    
    char* response = json_dumps(data, JSON_COMPACT);
    struct MHD_Response* resp = MHD_create_response_from_buffer(
        strlen(response), (void*)response, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(resp, "Content-Type", "application/json");
    MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
    MHD_destroy_response(resp);
    json_decref(data);
    
    return ret;
}

// GET /api/quarantine/list
enum MHD_Result handle_quarantine(struct MHD_Connection* connection, void* cls) {
    json_t* data = read_json_file("daemon_quarantine.json");
    
    if (!data) {
        json_t* error = json_object();
        json_object_set_new(error, "archivos", json_array());
        json_object_set_new(error, "total", json_integer(0));
        
        char* response = json_dumps(error, JSON_COMPACT);
        struct MHD_Response* resp = MHD_create_response_from_buffer(
            strlen(response), (void*)response, MHD_RESPMEM_MUST_FREE);
        
        MHD_add_response_header(resp, "Content-Type", "application/json");
        MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
        MHD_destroy_response(resp);
        json_decref(error);
        
        return ret;
    }
    
    char* response = json_dumps(data, JSON_COMPACT);
    struct MHD_Response* resp = MHD_create_response_from_buffer(
        strlen(response), (void*)response, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(resp, "Content-Type", "application/json");
    MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
    MHD_destroy_response(resp);
    json_decref(data);
    
    return ret;
}

// GET /api/scan/status
enum MHD_Result handle_scan_status(struct MHD_Connection* connection, void* cls) {
    (void)cls;
    json_t* resp = json_object();
    json_object_set_new(resp, "success", json_true());
    json_object_set_new(resp, "enabled", get_scan_state() ? json_true() : json_false());
    enum MHD_Result ret = send_json_response(connection, MHD_HTTP_OK, resp);
    json_decref(resp);
    return ret;
}

// POST /api/restore
enum MHD_Result handle_restore(struct MHD_Connection* connection,
                             const char* upload_data,
                             size_t* upload_data_size,
                             void** con_cls) {
    if (!is_admin_request(connection)) {
        return unauthorized_admin_required(connection);
    }

    struct post_context* ctx = (struct post_context*)*con_cls;
    if (ctx == NULL) {
        ctx = malloc(sizeof(struct post_context));
        if (!ctx) {
            return MHD_NO;
        }
        ctx->size = 0;
        *con_cls = (void*)ctx;
        return MHD_YES;
    }

    if (*upload_data_size > 0) {
        if (ctx->size + *upload_data_size > sizeof(ctx->data) - 1) {
            *upload_data_size = sizeof(ctx->data) - 1 - ctx->size;
        }
        memcpy(ctx->data + ctx->size, upload_data, *upload_data_size);
        ctx->size += *upload_data_size;
        *upload_data_size = 0;
        return MHD_YES;
    }

    ctx->data[ctx->size] = '\0';

    json_error_t error;
    json_t* root = json_loads(ctx->data, 0, &error);
    if (!root) {
        json_t* resp = json_object();
        json_object_set_new(resp, "success", json_false());
        json_object_set_new(resp, "error", json_string("JSON inválido"));
        enum MHD_Result ret = send_json_response(connection, MHD_HTTP_BAD_REQUEST, resp);
        json_decref(resp);
        free(ctx);
        *con_cls = NULL;
        return ret;
    }

    const char* path = json_string_value(json_object_get(root, "path"));
    if (!path || strlen(path) == 0) {
        json_t* resp = json_object();
        json_object_set_new(resp, "success", json_false());
        json_object_set_new(resp, "error", json_string("Campo 'path' requerido"));
        enum MHD_Result ret = send_json_response(connection, MHD_HTTP_BAD_REQUEST, resp);
        json_decref(resp);
        json_decref(root);
        free(ctx);
        *con_cls = NULL;
        return ret;
    }

    int rc = restaurar_archivo(path);
    json_t* resp = json_object();
    if (rc == 0) {
        eliminar_de_cuarentena_persistente(path);
        generar_json_cuarentena();
        json_object_set_new(resp, "success", json_true());
        json_object_set_new(resp, "mensaje", json_string("Archivo restaurado correctamente"));
        json_object_set_new(resp, "path", json_string(path));
    } else {
        json_object_set_new(resp, "success", json_false());
        json_object_set_new(resp, "error", json_string("No se pudo restaurar el archivo"));
        json_object_set_new(resp, "codigo", json_integer(rc));
    }

    enum MHD_Result ret = send_json_response(connection, rc == 0 ? MHD_HTTP_OK : MHD_HTTP_INTERNAL_SERVER_ERROR, resp);
    json_decref(resp);
    json_decref(root);
    free(ctx);
    *con_cls = NULL;
    return ret;
}

// POST /api/scan/start y /api/scan/stop
enum MHD_Result handle_scan_control(struct MHD_Connection* connection,
                                  const char* url,
                                  const char* upload_data,
                                  size_t* upload_data_size,
                                  void** con_cls) {
    (void)upload_data;

    if (!is_admin_request(connection)) {
        return unauthorized_admin_required(connection);
    }

    struct post_context* ctx = (struct post_context*)*con_cls;
    if (ctx == NULL) {
        ctx = malloc(sizeof(struct post_context));
        if (!ctx) {
            return MHD_NO;
        }
        ctx->size = 0;
        *con_cls = (void*)ctx;
        return MHD_YES;
    }

    if (*upload_data_size > 0) {
        if (ctx->size + *upload_data_size > sizeof(ctx->data) - 1) {
            *upload_data_size = sizeof(ctx->data) - 1 - ctx->size;
        }
        memcpy(ctx->data + ctx->size, upload_data, *upload_data_size);
        ctx->size += *upload_data_size;
        *upload_data_size = 0;
        return MHD_YES;
    }

    int enable = strcmp(url, "/api/scan/start") == 0 ? 1 : 0;
    int rc = set_scan_state(enable);

    json_t* resp = json_object();
    if (rc == 0) {
        json_object_set_new(resp, "success", json_true());
        json_object_set_new(resp, "enabled", enable ? json_true() : json_false());
        json_object_set_new(resp, "mensaje", json_string(enable ? "Escaneo activado" : "Escaneo desactivado"));
    } else {
        json_object_set_new(resp, "success", json_false());
        json_object_set_new(resp, "error", json_string("No se pudo actualizar el estado del escaneo"));
    }

    enum MHD_Result ret = send_json_response(connection, rc == 0 ? MHD_HTTP_OK : MHD_HTTP_INTERNAL_SERVER_ERROR, resp);
    json_decref(resp);
    free(ctx);
    *con_cls = NULL;
    return ret;
}

// POST /api/process/info
enum MHD_Result handle_process_info(struct MHD_Connection* connection,
                                  const char* upload_data,
                                  size_t* upload_data_size,
                                  void** con_cls) {
    if (!is_admin_request(connection)) {
        return unauthorized_admin_required(connection);
    }

    struct post_context* ctx = (struct post_context*)*con_cls;
    if (ctx == NULL) {
        ctx = malloc(sizeof(struct post_context));
        if (!ctx) {
            return MHD_NO;
        }
        ctx->size = 0;
        *con_cls = (void*)ctx;
        return MHD_YES;
    }

    if (*upload_data_size > 0) {
        if (ctx->size + *upload_data_size > sizeof(ctx->data) - 1) {
            *upload_data_size = sizeof(ctx->data) - 1 - ctx->size;
        }
        memcpy(ctx->data + ctx->size, upload_data, *upload_data_size);
        ctx->size += *upload_data_size;
        *upload_data_size = 0;
        return MHD_YES;
    }

    ctx->data[ctx->size] = '\0';
    json_error_t error;
    json_t* root = json_loads(ctx->data, 0, &error);
    if (!root) {
        json_t* resp = json_object();
        json_object_set_new(resp, "success", json_false());
        json_object_set_new(resp, "error", json_string("JSON inválido"));
        enum MHD_Result ret = send_json_response(connection, MHD_HTTP_BAD_REQUEST, resp);
        json_decref(resp);
        free(ctx);
        *con_cls = NULL;
        return ret;
    }

    json_t* pid_json = json_object_get(root, "pid");
    if (!json_is_integer(pid_json)) {
        json_t* resp = json_object();
        json_object_set_new(resp, "success", json_false());
        json_object_set_new(resp, "error", json_string("Campo 'pid' requerido"));
        enum MHD_Result ret = send_json_response(connection, MHD_HTTP_BAD_REQUEST, resp);
        json_decref(resp);
        json_decref(root);
        free(ctx);
        *con_cls = NULL;
        return ret;
    }

    int pid = (int)json_integer_value(pid_json);
    struct process_info info;
    memset(&info, 0, sizeof(info));

    int rc = obtener_info_proceso(pid, &info);

    json_t* resp = json_object();
    if (rc == 0) {
        json_object_set_new(resp, "success", json_true());
        json_object_set_new(resp, "pid", json_integer(info.pid));
        json_object_set_new(resp, "name", json_string(info.name));
        json_object_set_new(resp, "exec_time_ms", json_integer((json_int_t)info.exec_time_ms));
        json_object_set_new(resp, "mem_kb", json_integer((json_int_t)info.mem_kb));
    } else {
        json_object_set_new(resp, "success", json_false());
        json_object_set_new(resp, "error", json_string("No se pudo obtener información del proceso"));
        json_object_set_new(resp, "codigo", json_integer(rc));
    }

    enum MHD_Result ret = send_json_response(connection, rc == 0 ? MHD_HTTP_OK : MHD_HTTP_NOT_FOUND, resp);
    json_decref(resp);
    json_decref(root);
    free(ctx);
    *con_cls = NULL;
    return ret;
}

// Servir archivo estático
enum MHD_Result serve_static_file(struct MHD_Connection* connection, const char* path) {
    char filepath[512];
    
    // Prevenir directory traversal
    if (strstr(path, "..")) {
        return MHD_NO;
    }
    
    // Si es raíz, servir index.html
    if (strcmp(path, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", FRONTEND_DIR);
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s", FRONTEND_DIR, path);
    }
    
    char* content = read_file(filepath);
    if (!content) {
        const char* not_found = "<h1>404 - Archivo no encontrado</h1>";
        struct MHD_Response* resp = MHD_create_response_from_buffer(
            strlen(not_found), (void*)not_found, MHD_RESPMEM_PERSISTENT);
        
        int ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, resp);
        MHD_destroy_response(resp);
        
        return ret;
    }
    
    // Detectar content-type
    const char* content_type = "text/plain";
    if (strstr(filepath, ".html")) {
        content_type = "text/html; charset=utf-8";
    } else if (strstr(filepath, ".css")) {
        content_type = "text/css; charset=utf-8";
    } else if (strstr(filepath, ".js")) {
        content_type = "application/javascript; charset=utf-8";
    } else if (strstr(filepath, ".json")) {
        content_type = "application/json; charset=utf-8";
    } else if (strstr(filepath, ".png")) {
        content_type = "image/png";
    } else if (strstr(filepath, ".jpg") || strstr(filepath, ".jpeg")) {
        content_type = "image/jpeg";
    } else if (strstr(filepath, ".svg")) {
        content_type = "image/svg+xml";
    }
    
    struct MHD_Response* resp = MHD_create_response_from_buffer(
        strlen(content), (void*)content, MHD_RESPMEM_MUST_FREE);
    
    MHD_add_response_header(resp, "Content-Type", content_type);
    MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
    
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
    MHD_destroy_response(resp);
    
    return ret;
}

// ============ MANEJADOR PRINCIPAL ============

enum MHD_Result request_handler(void* cls,
                    struct MHD_Connection* connection,
                    const char* url,
                    const char* method,
                    const char* version,
                    const char* upload_data,
                    size_t* upload_data_size,
                    void** con_cls) {
    
    // Manejar CORS preflight
    if (strcmp(method, "OPTIONS") == 0) {
        struct MHD_Response* resp = MHD_create_response_from_buffer(
            0, NULL, MHD_RESPMEM_PERSISTENT);
        
        MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(resp, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        MHD_add_response_header(resp, "Access-Control-Allow-Headers", "Content-Type, X-User-Role, X-Username");
        
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
        MHD_destroy_response(resp);
        
        return ret;
    }
    
    // Manejar login (POST)
    if (strcmp(method, "POST") == 0 && strcmp(url, "/api/login") == 0) {
        return handle_login(connection, upload_data, upload_data_size, con_cls);
    }

    if (strcmp(method, "POST") == 0 && strcmp(url, "/api/restore") == 0) {
        return handle_restore(connection, upload_data, upload_data_size, con_cls);
    }

    if (strcmp(method, "POST") == 0 &&
        (strcmp(url, "/api/scan/start") == 0 || strcmp(url, "/api/scan/stop") == 0)) {
        return handle_scan_control(connection, url, upload_data, upload_data_size, con_cls);
    }

    if (strcmp(method, "POST") == 0 && strcmp(url, "/api/process/info") == 0) {
        return handle_process_info(connection, upload_data, upload_data_size, con_cls);
    }
    
    // Solo GET para lo demás
    if (strcmp(method, "GET") != 0) {
        return MHD_NO;
    }
    
    // Rutas API
    if (strcmp(url, "/api/monitor") == 0) {
        return handle_monitor(connection, cls);
    }
    if (strcmp(url, "/api/files") == 0) {
        return handle_files(connection, cls);
    }
    if (strcmp(url, "/api/processes") == 0) {
        return handle_processes(connection, cls);
    }
    if (strcmp(url, "/api/alerts") == 0) {
        return handle_alerts(connection, cls);
    }
    if (strcmp(url, "/api/quarantine/list") == 0) {
        return handle_quarantine(connection, cls);
    }
    if (strcmp(url, "/api/scan/status") == 0) {
        return handle_scan_status(connection, cls);
    }
    
    // Archivos estáticos
    return serve_static_file(connection, url);
}

// ============ MAIN ============

int main(int argc, char* argv[]) {
    struct MHD_Daemon* daemon;
    
    printf("════════════════════════════════════════\n");
    printf("🔒 Servidor HTTP de Seguridad - C\n");
    printf("════════════════════════════════════════\n\n");
    printf("Iniciando servidor en puerto %d...\n", PORT);
    printf("Frontend: %s\n", FRONTEND_DIR);
    printf("JSON dir: %s\n\n", JSON_DIR);
    
    daemon = MHD_start_daemon(
        MHD_USE_SELECT_INTERNALLY,
        PORT,
        NULL, NULL,
        &request_handler, NULL,
        MHD_OPTION_END);
    
    if (!daemon) {
        fprintf(stderr, "❌ Error: No se pudo iniciar el servidor\n");
        return 1;
    }
    
    printf("✅ Servidor activo en http://localhost:%d\n", PORT);
    printf("   GET /                   → Frontend\n");
    printf("   POST /api/login         → Autenticación con PAM\n");
    printf("   POST /api/restore       → Restaurar archivo de cuarentena (admin)\n");
    printf("   POST /api/scan/start    → Activar escaneo (admin)\n");
    printf("   POST /api/scan/stop     → Desactivar escaneo (admin)\n");
    printf("   POST /api/process/info  → Info detallada por PID (admin)\n");
    printf("   GET /api/monitor        → Métricas del sistema\n");
    printf("   GET /api/files          → Archivos monitoreados\n");
    printf("   GET /api/processes      → Procesos sospechosos\n");
    printf("   GET /api/alerts         → Alertas de seguridad\n");
    printf("   GET /api/quarantine/list → Archivos en cuarentena\n");
    printf("   GET /api/scan/status    → Estado del escaneo\n");
    printf("\n📄 Presiona Ctrl+C para detener...\n\n");
    
    // Mantener el servidor corriendo
    getchar();
    
    printf("\n🛑 Deteniendo servidor...\n");
    MHD_stop_daemon(daemon);
    printf("✅ Servidor detenido\n");
    
    return 0;
}
