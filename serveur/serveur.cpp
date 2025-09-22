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
const int BOOK_CONSULTATION_LENGTH = 18; // "BOOK_CONSULTATION;" = 18 caractères

// Déclarations de variables globales
struct ServerConfig {
    int portReservation = 0;
    int nbThreads = 4;
};

struct ClientTask { 
    int socket; 
    char ip[INET_ADDRSTRLEN]; 
};

static ServerConfig config;
static bool stop = false;
static queue<ClientTask> tasks; // file de tâches à traiter
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

// Variables de parsing (réutilisables)
static size_t pos, pos1, pos2, pos3;
static string lastName, firstName, specialty, doctor, startDate, endDate, reason, message;
static int id, consultationId, patientId, bytesReceived;
static bool clientConnected, configLoaded, dbConfigLoaded;
static char buffer[BUFFER_SIZE], ipClient[INET_ADDRSTRLEN];
static string line, key, val;
static int serverSocket, clientSocket, i;


static void trim(string &s) {
    while(!s.empty() && (s.back()=='\n' || s.back()=='\r' || s.back()==' '|| s.back()=='\t')) {
        s.pop_back();
    }
    i=0; 
    while(i<s.size() && (s[i]==' '||s[i]=='\t')) {
        ++i; 
    }
    s.erase(0,i);
}

// Fonction simple pour vérifier si on a trouvé quelque chose
static bool found(size_t pos, size_t maxLen) {
    return pos < maxLen;
}

// Connexion à la base de données
static MYSQL* connectToDatabase() {
    DatabaseConfig dbConfig;
    const char* configPaths[] = {
        "conf/serveur.conf",
        "../conf/serveur.conf", 
        "serveur.conf"
    };
    
    for(i = 0; i < 3; i++) {
        if(DB::loadConfig(configPaths[i], dbConfig)) {
            return DB::openConnection(dbConfig);
        }
    }
    return nullptr;
}

// Fonction pour déterminer le type de message
static int getMessageType(const string& message) {
    if(message.find(LOGIN_NEW) == 0) return 1;
    if(message.find(LOGIN_EXIST) == 0) return 2;
    if(message.find(SEARCH) == 0) return 3;
    if(message.find(GET_SPECIALTIES) == 0) return 4;
    if(message.find("GET_DOCTORS;") == 0) return 5;
    if(message.find("BOOK_CONSULTATION;") == 0) return 6;
    return 0; // Inconnu
}

// Traitement d'un message client
static void processMessage(MYSQL* con, int socket, const string& message) {
    switch(getMessageType(message)) {
        case 1: { // LOGIN_NEW
            // Format: LOGIN_NEW;NOM;PRENOM
            pos = message.find(';', LOGIN_NEW_LENGTH);
            if(found(pos, message.length())) {
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
            if(found(pos1, message.length()) && found(pos2, message.length())) {
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
            if(found(pos1, message.length()) && found(pos2, message.length()) && found(pos3, message.length())) {
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
            specialty = message.substr(GET_DOCTORS_LENGTH);
            DB::handleGetDoctors(con, socket, specialty);
            break;
        }
        
        case 6: { // BOOK_CONSULTATION
            // Format: BOOK_CONSULTATION;CONSULTATION_ID;PATIENT_ID;REASON
            pos1 = message.find(';', BOOK_CONSULTATION_LENGTH);
            pos2 = message.find(';', pos1 + 1);
            if(found(pos1, message.length()) && found(pos2, message.length())) {
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
            Send(socket, (string(LOGIN_FAIL) + UNKNOWN_CMD).c_str(), (string(LOGIN_FAIL) + UNKNOWN_CMD).length());
            break;
    }
}

// Traitement d'un client
static void handleClient(ClientTask task) {
    printf("Thread traite la connexion de %s (socket %d)\n", task.ip, task.socket);
    
    // Connexion à la base de données
    MYSQL* con = connectToDatabase();
    if(!con) {
        printf("Erreur: Impossible de se connecter à la base de données\n");
        closeSocket(task.socket);
        return;
    }
    
    // Boucle de traitement des messages
    clientConnected = true;
    
    while(clientConnected) {
        bytesReceived = Receive(task.socket, buffer);
        
        if(bytesReceived <= 0) {
            printf("Client %s déconnecté (socket %d)\n", task.ip, task.socket);
            clientConnected = false;
        } else {
            message = string(buffer);
            printf("Message reçu de %s: %s\n", task.ip, buffer);
            fflush(stdout);
            processMessage(con, task.socket, message);
        }
    }
    
    // Nettoyage
    DB::closeConnection(con);
    closeSocket(task.socket);
    printf("Socket %d fermé\n", task.socket);
}


static bool loadConfig(const char* path, ServerConfig &cfg){
    ifstream in(path);
    if(!in) {
        return false;
    }
    while(getline(in,line)){
        trim(line);
        if(line.empty()) {
            continue;
        }
        pos = line.find('=');
        if(!found(pos, line.length())) {
            continue;
        }
        key=line.substr(0,pos); 
        trim(key);
        val=line.substr(pos+1); 
        trim(val);
        if(key=="PORT_RESERVATION") {
            cfg.portReservation = atoi(val.c_str());
        }
        else if(key=="NB_THREADS") {
            cfg.nbThreads = atoi(val.c_str());
        }
    }
    return cfg.portReservation>0;
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
    // Essayer plusieurs chemins possibles pour le fichier de configuration
    const char* configPaths[] = {
        "conf/serveur.conf",           // Depuis la racine du projet
        "../conf/serveur.conf",        // Depuis le répertoire serveur/
        "serveur.conf"                 // Dans le répertoire courant
    };
    
    configLoaded = false;
    for(i = 0; i < 3; i++) {
        if(loadConfig(configPaths[i], config)) {
            configLoaded = true;
            break;
        }
    }
    
    if(!configLoaded){
        fprintf(stderr, "ERREUR: Impossible de charger la configuration serveur.conf\n");
        return 1;
    }
    if(config.nbThreads<=0) {
        config.nbThreads=4;
    }
    printf("Configuration serveur chargée: port=%d threads=%d\n", config.portReservation, config.nbThreads);

    serverSocket = ServerSocket(config.portReservation);
    if(serverSocket<0){ 
        perror("ServerSocket"); 
        return 1; 
    }
    printf("Serveur en écoute sur le port %d\n", config.portReservation);

    // Création du pool
    vector<pthread_t> threads(config.nbThreads);
    for(i=0;i<config.nbThreads;++i){
        pthread_create(&threads[i], nullptr, workerThread, nullptr);
    }

    while(!stop){
        memset(ipClient, 0, INET_ADDRSTRLEN);
        clientSocket = AcceptConnection(serverSocket, ipClient);
        if(clientSocket<0){ 
            perror("AcceptConnection"); 
            continue; 
        }
        printf("Connexion acceptée de %s\n", ipClient);
        pthread_mutex_lock(&mutex);
        tasks.push({clientSocket,{0}});
        strncpy(tasks.back().ip, ipClient, INET_ADDRSTRLEN-1);
        pthread_cond_signal(&condition);
        pthread_mutex_unlock(&mutex);
    }

    // Arrêt (non déclenché dans ce squelette)
    stop = true;
    pthread_cond_broadcast(&condition);
    for(auto &t: threads) {
        pthread_join(t,nullptr);
    }
    closeSocket(serverSocket);
    return 0;
}