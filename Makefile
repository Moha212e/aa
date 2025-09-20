
# Compiler
CXX = g++

# Directories
BD_DIR = BD_Hospital
CLIENT_DIR = ClientConsultationBookerQt
SOCKET_DIR = socket
SERVEUR_DIR = serveur
UTIL_DIR = util

# Source files
BD_SRC = $(BD_DIR)/CreationBD.cpp
CLIENT_SRC = $(CLIENT_DIR)/main.cpp $(CLIENT_DIR)/mainwindowclientconsultationbooker.cpp $(CLIENT_DIR)/moc_mainwindowclientconsultationbooker.cpp socket/socket.cpp
SOCKET_SRC = $(SOCKET_DIR)/socket.cpp
SERVEUR_SRC = $(SERVEUR_DIR)/serveur.cpp
UTIL_HEADERS = $(UTIL_DIR)/name.h

# Output binaries
BD_BIN = $(BD_DIR)/CreationBD
CLIENT_BIN = $(CLIENT_DIR)/ClientConsultationBooker
SERVEUR_BIN = $(SERVEUR_DIR)/serveur

# MySQL flags (headers + lib)
MYSQL_CFLAGS = -I/usr/include/mysql
MYSQL_LIBS = -lmysqlclient -lpthread -lz -lm -lrt -lssl -lcrypto -ldl

# Qt flags (adjust if needed)
QT_FLAGS = `pkg-config --cflags --libs Qt5Widgets`

# Default target
all: $(BD_BIN) $(CLIENT_BIN) $(SERVEUR_BIN)

$(BD_BIN): $(BD_SRC)
	$(CXX) -o $@ $< $(MYSQL_CFLAGS) -m64 -L/usr/lib64/mysql $(MYSQL_LIBS)

$(CLIENT_BIN): $(CLIENT_SRC) $(UTIL_HEADERS)
	$(CXX) -fPIC -o $@ $^ $(QT_FLAGS)

$(SERVEUR_BIN): $(SERVEUR_SRC) $(SOCKET_SRC) $(UTIL_HEADERS)
	$(CXX) -o $@ $^ -lpthread $(MYSQL_CFLAGS) -m64 -L/usr/lib64/mysql $(MYSQL_LIBS)

clean:
	rm -f $(BD_BIN) $(CLIENT_BIN) $(SERVEUR_BIN)

.PHONY: all clean
