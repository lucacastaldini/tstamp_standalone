MODEL ?= Z10
DEBUG ?= 0
CFLAGS  = -Wall -Werror -std=c99 -D_POSIX_C_SOURCE=199309L
CFLAGS += -I/opt/redpitaya/include -Iinclude -D$(MODEL)

################################
# uncomment the following line to disable auto clear flags
# CFLAGS += -DAUTO_CLEAR_FLAGS_DISABLED 
################################

# Abilita trace se DEBUG=1
ifeq ($(DEBUG), 1)
    CFLAGS += -DTRACEON
    $(info Debug mode enabled - traces will be active)
endif

LDFLAGS = -L/opt/redpitaya/lib
LDLIBS = -lrp -lrp-hw-calib -lrp-hw-profiles -lrp-gpio -lrp-i2c -lrp-hw -lm -lpthread -li2c

# Variabili di installazione
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INSTALL = install

# Directory di build e output
BUILD_DIR = build
BIN_DIR = bin
LIB_DIR = lib

# Librerie da creare
STATIC_LIB = $(LIB_DIR)/libtstamp.a
SHARED_LIB = $(LIB_DIR)/libtstamp.so

# Programmi da compilare
PROGRAMS = gps_timestamp_demo
TEST_PROGRAM = tstamp_test

# File sorgenti per la libreria timestamp
TSTAMP_SOURCES = tstamp.c uart.c hk_fpga.c 
TSTAMP_OBJECTS = $(addprefix $(BUILD_DIR)/, $(TSTAMP_SOURCES:.c=.o))

# Target principale - libreria statica e programma di test
all: $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR) $(STATIC_LIB) $(BIN_DIR)/$(TEST_PROGRAM)

# Target per compilazione con debug/trace abilitati
debug:
	$(MAKE) DEBUG=1 all

# Creazione della libreria statica
$(STATIC_LIB): $(TSTAMP_OBJECTS) | $(LIB_DIR)
	ar rcs $@ $^
	@echo "Static library created: $@"

# Demo GPS timestamp (opzionale - solo se richiesto)
$(BIN_DIR)/gps_timestamp_demo: src/gps_demo.c $(STATIC_LIB)
	gcc $(CFLAGS) src/gps_demo.c -o $@ -L$(LIB_DIR) -ltstamp $(LDFLAGS) $(LDLIBS)

# Programma di test
$(BIN_DIR)/$(TEST_PROGRAM): src/gps_demo.c $(STATIC_LIB)
	gcc $(CFLAGS) src/gps_demo.c -o $@ -L$(LIB_DIR) -ltstamp $(LDFLAGS) $(LDLIBS)

# Regole per i file oggetto della libreria timestamp
$(BUILD_DIR)/%.o: src/%.c
	gcc $(CFLAGS) -c $< -o $@

# Target per compilare solo la libreria statica
lib: $(TSTAMP_OBJECTS) | $(LIB_DIR)
	ar rcs $(STATIC_LIB) $^
	@echo "Static library built: $(STATIC_LIB)"

# Creazione delle directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

# Installazione - libreria statica e programma di test
install: all
	@echo "Installing timestamp static library and test program..."
	$(INSTALL) -d $(LIBDIR)
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -m 644 $(STATIC_LIB) $(LIBDIR)
	$(INSTALL) -m 755 $(BIN_DIR)/$(TEST_PROGRAM) $(BINDIR)
	@echo "Static library and test program installation completed successfully."

# Disinstallazione
uninstall:
	@echo "Uninstalling timestamp static library and test program..."
	rm -f $(LIBDIR)/libtstamp.a
	rm -f $(BINDIR)/$(TEST_PROGRAM)
	@echo "Uninstallation completed successfully."

# Test della libreria
test: $(BIN_DIR)/tstamp_test
	@echo "Running timestamp tests..."
	cd $(BIN_DIR) && LD_LIBRARY_PATH=../$(LIB_DIR):$$LD_LIBRARY_PATH ./tstamp_test

# Pulizia
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)

# Mostra informazioni sulla build
info:
	@echo "=== Timestamp Static Library Build Configuration ==="
	@echo "Model: $(MODEL)"
	@echo "Debug: $(DEBUG)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "LDLIBS: $(LDLIBS)"
	@echo "Static Library: $(STATIC_LIB)"
	@echo "Available targets: all, lib, test, clean, install, uninstall"

# Target phony
.PHONY: all debug lib clean install uninstall test info