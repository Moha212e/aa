// Implémentation serveur avec lecture de configuration et squelette de pool de threads
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <pthread.h>
#include <string>
#include <vector>
#include <queue>
#include <fstream>
#include <iostream>
#include "../util/name.h"
#include "../socket/socket.h"
#include "database.h"

using namespace std;

// Alias pour simplifier les appels
#define DB DatabaseManager

// Constantes d'amélioration du code
const int BUFFER_SIZE = 1024;
const int LOGIN_NEW_LENGTH = 10;  // "LOGIN_NEW;" = 10 caractères
const int LOGIN_EXIST_LENGTH = 12; // "LOGIN_EXIST;" = 12 caractères
const int SEARCH_LENGTH = 7;       // "SEARCH;" = 7 caractères
const int GET_DOCTORS_LENGTH = 12; // "GET_DOCTORS;" = 12 caractères
const int BOOK_CONSULTATION_LENGTH = 19; // "BOOK_CONSULTATION;" = 19 caractères

// Déclarations de variables globales
struct ServerConfig {
    int portReservation = 0;
    int nbThreads = 4;
};

struct ClientTask { 
    int socket; 
    char ip[INET_ADDRSTRLEN]; // 16 caractères max pour l'ip
};

static ServerConfig config;
static bool stop = false;
static queue<ClientTask> tasks; // file de tâches à traiter
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

// Variables globales du serveur
static bool configLoaded, dbConfigLoaded;
static char buffer[BUFFER_SIZE], ipClient[INET_ADDRSTRLEN];
static string line, key, val;
static int serverSocket, clientSocket;


static void trim(string &s) {
    while(!s.empty() && (s.back()=='\n' || s.back()=='\r' || s.back()==' '|| s.back()=='\t')) {
        s.pop_back();
    }
    int i = 0; 
    while(i < s.size() && (s[i]==' '||s[i]=='\t')) {
        ++i; 
    }
    s.erase(0, i);
}


// Connexion à la base de données
static MYSQL* connectToDatabase() {
    DatabaseConfig dbConfig;
    
    if(DB::loadConfig("../conf/serveur.conf", dbConfig)) {
        return DB::openConnection(dbConfig);
    }
    return nullptr;
}

static int getMessageType(const string& message) {
    if (message.empty()) return 0;
    
    switch (message[0]) {
        case 'L': // LOGIN_NEW ou LOGIN_EXIST
            if (message.find(LOGIN_NEW) == 0) return 1;
            if (message.find(LOGIN_EXIST) == 0) return 2;
            break;
        case 'S': // SEARCH
            if (message.find(SEARCH) == 0) return 3;
            break;
        case 'G': // GET_SPECIALTIES ou GET_DOCTORS
            if (message.find(GET_SPECIALTIES) == 0) return 4;
            if (message.find(GET_DOCTORS) == 0) return 5;
            break;
        case 'B': // BOOK_CONSULTATION
            if (message.find(BOOK_CONSULTATION) == 0) return 6;
            break;
    }
    return 0; // Inconnu
}

// Traitement d'un message client
static void processMessage(MYSQL* con, int socket, const string& message) {
    // Variables locales pour éviter les conditions de course
    size_t pos, pos1, pos2, pos3;
    string lastName, firstName, specialty, doctor, startDate, endDate, reason;
    int id, consultationId, patientId;
    
    switch(getMessageType(message)) {
        case 1: { // LOGIN_NEW
            // Format: LOGIN_NEW;NOM;PRENOM
            pos = message.find(';', LOGIN_NEW_LENGTH);
            if(pos < message.length()) {
                lastName = message.substr(LOGIN_NEW_LENGTH, pos - LOGIN_NEW_LENGTH);
                firstName = message.substr(pos + 1);
                DB::handleLoginNew(con, socket, lastName, firstName);
            } else {
                Send(socket, (string(LOGIN_FAIL) + FORMAT).c_str(), (string(LOGIN_FAIL) + FORMAT).length());
            }
            break;
        }
        
        case 2: { // LOGIN_EXIST
            // Format: LOGIN_EXIST;ID;NOM;PRENOM
            pos1 = message.find(';', LOGIN_EXIST_LENGTH);
            pos2 = message.find(';', pos1 + 1);
            if(pos1 < message.length() && pos2 < message.length()) {
                id = atoi(message.substr(LOGIN_EXIST_LENGTH, pos1 - LOGIN_EXIST_LENGTH).c_str());
                lastName = message.substr(pos1 + 1, pos2 - pos1 - 1);
                firstName = message.substr(pos2 + 1);
                DB::handleLoginExist(con, socket, id, lastName, firstName);
            } else {
                Send(socket, (string(LOGIN_FAIL) + FORMAT).c_str(), (string(LOGIN_FAIL) + FORMAT).length());
            }
            break;
        }
        
        case 3: { // SEARCH
            // Format: SEARCH;SPECIALTY;DOCTOR;START_DATE;END_DATE
            pos1 = message.find(';', SEARCH_LENGTH);
            pos2 = message.find(';', pos1 + 1);
            pos3 = message.find(';', pos2 + 1);
            if(pos1 < message.length() && pos2 < message.length() && pos3 < message.length()) {
                specialty = message.substr(SEARCH_LENGTH, pos1 - SEARCH_LENGTH);
                doctor = message.substr(pos1 + 1, pos2 - pos1 - 1);
                startDate = message.substr(pos2 + 1, pos3 - pos2 - 1);
                endDate = message.substr(pos3 + 1);
                DB::handleSearch(con, socket, specialty, doctor, startDate, endDate);
            } else {
                Send(socket, (string(SEARCH_FAIL) + FORMAT).c_str(), (string(SEARCH_FAIL) + FORMAT).length());
            }
            break;
        }
        
        case 4: // GET_SPECIALTIES
            DB::handleGetSpecialties(con, socket);
            break;
            
        case 5: { // GET_DOCTORS
            // Format: GET_DOCTORS;SPECIALTY
            pos = message.find(';', GET_DOCTORS_LENGTH);
            if(pos < message.length()) {
                specialty = message.substr(GET_DOCTORS_LENGTH, pos - GET_DOCTORS_LENGTH);
                DB::handleGetDoctors(con, socket, specialty);
            } else {
                Send(socket, (string(DOCTORS_FAIL) + FORMAT).c_str(), (string(DOCTORS_FAIL) + FORMAT).length());
            }
            break;
        }
        
        case 6: { // BOOK_CONSULTATION
            // Format: BOOK_CONSULTATION;CONSULTATION_ID;PATIENT_ID;REASON
            pos1 = message.find(';', BOOK_CONSULTATION_LENGTH);
            pos2 = message.find(';', pos1 + 1);
            if(pos1 < message.length() && pos2 < message.length()) {
                consultationId = atoi(message.substr(BOOK_CONSULTATION_LENGTH, pos1 - BOOK_CONSULTATION_LENGTH).c_str());
                patientId = atoi(message.substr(pos1 + 1, pos2 - pos1 - 1).c_str());
                reason = message.substr(pos2 + 1);
                DB::handleBookConsultation(con, socket, consultationId, patientId, reason);
            } else {
                Send(socket, (string(BOOK_FAIL) + FORMAT).c_str(), (string(BOOK_FAIL) + FORMAT).length());
            }
            break;
        }
        
        default: // Message inconnu
            break;
    }
}

// Traitement d'un client
static void handleClient(ClientTask task) {
    printf("Client connecté: %s (socket %d)\n", task.ip, task.socket);
    
    MYSQL* con = connectToDatabase();
    if (!con) {
        printf("Erreur DB pour client %s\n", task.ip);
        closeSocket(task.socket);
        return;
    }
    
    int bytesReceived;
    while ((bytesReceived = Receive(task.socket, buffer)) > 0) {
        printf("Requête reçue de %s: %s\n", task.ip, buffer);
        processMessage(con, task.socket, string(buffer));
    }
    
    printf("Client déconnecté: %s (socket %d)\n", task.ip, task.socket);
    DB::closeConnection(con);
    closeSocket(task.socket);
}


static bool loadConfig(ServerConfig &cfg) {
    const char* configPaths = "../conf/serveur.conf";
    
    ifstream in(configPaths);
    if (!in) {
        return false;
    }
    
    while (getline(in, line)) {
        trim(line);
        if (line.empty()) {
            continue;
        }
        
        size_t pos = line.find('=');
        if (pos == string::npos) {
            continue;
        }
        
        key = line.substr(0, pos);
        val = line.substr(pos + 1);
        trim(key);
        trim(val);
        
        if (key == "PORT_RESERVATION") {
            cfg.portReservation = atoi(val.c_str());
        } else if (key == "NB_THREADS") {
            cfg.nbThreads = atoi(val.c_str());
        }
    }
    
    return cfg.portReservation > 0;
}





static void* workerThread(void*){
    while(true){
        // Attendre une tâche
        pthread_mutex_lock(&mutex);
        while(tasks.empty() && !stop){
            pthread_cond_wait(&condition,&mutex);
        }
        if(stop && tasks.empty()){
            pthread_mutex_unlock(&mutex); 
            break; 
        }
        ClientTask task = tasks.front(); 
        tasks.pop();
        pthread_mutex_unlock(&mutex);
        
        // Traiter le client
        handleClient(task);
    }
    return nullptr;
}

int main(){
    if (!loadConfig(config)) {
        return 1;
    }

    serverSocket = ServerSocket(config.portReservation);
    if (serverSocket < 0) { 
        return 1; 
    }

    printf("Serveur démarré sur le port %d avec %d threads\n", config.portReservation, config.nbThreads);

    vector<pthread_t> threads(config.nbThreads);
    for (int i = 0; i < config.nbThreads; ++i) {
        pthread_create(&threads[i], nullptr, workerThread, nullptr);
    }

    while (!stop) {
        clientSocket = AcceptConnection(serverSocket, ipClient);
        if (clientSocket < 0) { 
            continue; 
        }
        
        printf("Nouvelle connexion acceptée de %s (socket %d)\n", ipClient, clientSocket);
        
        pthread_mutex_lock(&mutex);
        ClientTask newTask;
        newTask.socket = clientSocket;
        strncpy(newTask.ip, ipClient, INET_ADDRSTRLEN-1);
        newTask.ip[INET_ADDRSTRLEN-1] = '\0';
        tasks.push(newTask);
        pthread_cond_signal(&condition);
        pthread_mutex_unlock(&mutex);
    }

    stop = true;
    pthread_cond_broadcast(&condition);
    for (auto &t: threads) {
        pthread_join(t, nullptr);
    }
    closeSocket(serverSocket);
    return 0;
}