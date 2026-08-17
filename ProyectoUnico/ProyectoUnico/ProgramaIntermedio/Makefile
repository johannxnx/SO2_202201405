CC = gcc
CFLAGS = -Wall -I./include
LDFLAGS = -lpthread

# ===== DAEMON MONITOR =====
DAEMON_SRC = src/main.c src/daemon.c src/monitor_thread.c src/scan_thread.c src/syscalls_wrapper.c src/blacklist.c src/file_state.c src/alerts.c src/quarantine_persistent.c
DAEMON_OUT = daemon_monitor

# ===== SERVIDOR HTTP =====
SERVER_SRC = src/server.c src/syscalls_wrapper.c src/quarantine_persistent.c
SERVER_OUT = server_http
SERVER_CFLAGS = $(CFLAGS) `pkg-config --cflags libmicrohttpd jansson`
SERVER_LDFLAGS = `pkg-config --libs libmicrohttpd jansson` -lpam -lpam_misc

# Compilar todo
all: $(DAEMON_OUT) $(SERVER_OUT)
	@echo "✅ Compilación completada"

# Compilar daemon
$(DAEMON_OUT):
	$(CC) $(CFLAGS) $(DAEMON_SRC) -o $(DAEMON_OUT) $(LDFLAGS)
	@echo "✅ Daemon compilado: $(DAEMON_OUT)"

# Compilar servidor HTTP
$(SERVER_OUT):
	$(CC) $(SERVER_CFLAGS) $(SERVER_SRC) -o $(SERVER_OUT) $(SERVER_LDFLAGS)
	@echo "✅ Servidor HTTP compilado: $(SERVER_OUT)"

clean:
	rm -f $(DAEMON_OUT) $(SERVER_OUT)
	@echo "🧹 Limpeza completada"

.PHONY: all clean
