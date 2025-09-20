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
#include <mysql.h>
#include "../util/name.h"

#include "../socket/socket.h"

// Constantes d'amélioration du code
const int BUFFER_SIZE = 1024;
const int QUERY_SIZE = 512;
const int LOGIN_NEW_LENGTH = 10;  // "LOGIN_NEW;" = 10 caractères
const int LOGIN_EXIST_LENGTH = 12; // "LOGIN_EXIST;" = 12 caractères
const int SEARCH_LENGTH = 7;       // "SEARCH;" = 7 caractères
const int GET_DOCTORS_LENGTH = 12; // "GET_DOCTORS;" = 12 caractères
const int BOOK_CONSULTATION_LENGTH = 18; // "BOOK_CONSULTATION;" = 18 caractères

struct ServerConfig {
    int portReservation = 0;
    int nbThreads = 4;
    std::string dbHost;
    std::string dbUser;
    std::string dbPass;
    std::string dbName;
};

static ServerConfig config;
static bool stop = false;

struct ClientTask { int socket; char ip[INET_ADDRSTRLEN]; };

static std::queue<ClientTask> tasks; // file de tâches à traiter
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  condition  = PTHREAD_COND_INITIALIZER;

static void trim(std::string &s) {
    while(!s.empty() && (s.back()=='\n' || s.back()=='\r' || s.back()==' '|| s.back()=='\t')) {
        s.pop_back();
    }
    size_t i=0; 
    while(i<s.size() && (s[i]==' '||s[i]=='\t')) {
        ++i; 
    }
    s.erase(0,i);
}

static bool loadConfig(const char* path, ServerConfig &cfg){
    std::ifstream in(path);
    if(!in) {
        return false;
    }
    std::string line;
    while(std::getline(in,line)){
        trim(line);
        if(line.empty()|| line[0]=='#') {
            continue;
        }
        auto pos = line.find('=');
        if(pos==std::string::npos) {
            continue;
        }
        std::string key=line.substr(0,pos); 
        trim(key);
        std::string val=line.substr(pos+1); 
        trim(val);
        if(key=="PORT_RESERVATION") {
            cfg.portReservation = std::atoi(val.c_str());
        }
        else if(key=="NB_THREADS") {
            cfg.nbThreads = std::atoi(val.c_str());
        }
        else if(key==DB_HOST) {
            cfg.dbHost = val;
        }
        else if(key=="DB_USER") {
            cfg.dbUser = val;
        }
        else if(key=="DB_PASS") {
            cfg.dbPass = val;
        }
        else if(key=="DB_NAME") {
            cfg.dbName = val;
        }
    }
    return cfg.portReservation>0;
}

static MYSQL* openDb(){
    MYSQL* con = mysql_init(NULL);
    if(!con) return NULL;
    if(!mysql_real_connect(con, config.dbHost.c_str(), config.dbUser.c_str(), config.dbPass.c_str(), config.dbName.c_str(), 0, NULL, 0)){
        mysql_close(con);
        return NULL;
    }
    return con;
}



static int createNewPatient(MYSQL* con, const std::string &last, const std::string &first){
    // Validation des entrées
    if(last.empty() || first.empty() || last.length() > 50 || first.length() > 50) {
        return -1;
    }
    
    // Échappement des caractères spéciaux
    std::string escapedLast = last;
    std::string escapedFirst = first;
    
    // Remplacer les caractères dangereux
    size_t pos = 0;
    while((pos = escapedLast.find("'", pos)) != std::string::npos) {
        escapedLast.replace(pos, 1, "''");
        pos += 2;
    }
    pos = 0;
    while((pos = escapedFirst.find("'", pos)) != std::string::npos) {
        escapedFirst.replace(pos, 1, "''");
        pos += 2;
    }
    
    char query[QUERY_SIZE];
    snprintf(query, sizeof(query), "INSERT INTO patients (last_name, first_name, birth_date) VALUES ('%s','%s','2000-01-01')", 
             escapedLast.c_str(), escapedFirst.c_str());
    if(mysql_query(con, query)!=0) return -1;
    return (int) mysql_insert_id(con);
}

static bool verifyExistingPatient(MYSQL* con, int id, const std::string &last, const std::string &first){
    char query[512];
    snprintf(query, sizeof(query), "SELECT id FROM patients WHERE id=%d AND last_name='%s' AND first_name='%s'", 
             id, last.c_str(), first.c_str());
    
    if(mysql_query(con, query) != 0) return false;
    
    MYSQL_RES *result = mysql_store_result(con);
    if(!result) return false;
    
    bool found = (mysql_num_rows(result) > 0);
    mysql_free_result(result);
    return found;
}

static void sendResponse(int socket, const std::string &response) {
    Send(socket, response.c_str(), response.length());
}

static void handleLoginNew(MYSQL* con, int socket, const std::string &last, const std::string &first) {
    printf("Traitement LOGIN_NEW pour %s %s\n", last.c_str(), first.c_str());
    
    int patientId = createNewPatient(con, last, first);
    if(patientId > 0) {
        std::string response = std::string(LOGIN_OK) + std::to_string(patientId);
        sendResponse(socket, response);
        printf("Nouveau patient créé avec ID: %d\n", patientId);
        
        // Vérifier que le patient a bien été créé
        char query[512];
        snprintf(query, sizeof(query), "SELECT id, last_name, first_name FROM patients WHERE id=%d", patientId);
        if(mysql_query(con, query) == 0) {
            MYSQL_RES *result = mysql_store_result(con);
            if(result && mysql_num_rows(result) > 0) {
                MYSQL_ROW row = mysql_fetch_row(result);
                printf("Vérification: Patient ID=%s, Nom=%s, Prénom=%s\n", row[0], row[1], row[2]);
            }
            mysql_free_result(result);
        }
    } else {
        sendResponse(socket, std::string(LOGIN_FAIL) + INSERT);
        printf("Erreur lors de la création du patient\n");
    }
}

static void handleLoginExist(MYSQL* con, int socket, int id, const std::string &last, const std::string &first) {
    printf("Traitement LOGIN_EXIST pour ID=%d, %s %s\n", id, last.c_str(), first.c_str());
    
    if(verifyExistingPatient(con, id, last, first)) {
        std::string response = std::string(LOGIN_OK) + std::to_string(id);
        sendResponse(socket, response);
        printf("Patient existant vérifié avec succès\n");
    } else {
        sendResponse(socket, std::string(LOGIN_FAIL) + NOT_FOUND);
        printf("Patient non trouvé ou données incorrectes\n");
    }
}

static void handleSearch(MYSQL* con, int socket, const std::string &specialty, const std::string &doctor, const std::string &startDate, const std::string &endDate) {
    printf("Traitement SEARCH: specialty=%s, doctor=%s, startDate=%s, endDate=%s\n", 
           specialty.c_str(), doctor.c_str(), startDate.c_str(), endDate.c_str());
    
    // Construire la requête SQL
    std::string query = "SELECT c.id, s.name, CONCAT(d.first_name, ' ', d.last_name), c.date, c.hour ";
    query += "FROM consultations c ";
    query += "JOIN doctors d ON c.doctor_id = d.id ";
    query += "JOIN specialties s ON d.specialty_id = s.id ";
    query += "WHERE c.patient_id IS NULL "; // Seulement les créneaux libres
    
    // Ajouter les filtres selon les critères
    if (specialty != TOUTES) {
        query += "AND s.name = '" + specialty + "' ";
    }
    if (doctor != TOUS) {
        query += "AND CONCAT(d.first_name, ' ', d.last_name) = '" + doctor + "' ";
    }
    query += "AND c.date BETWEEN '" + startDate + "' AND '" + endDate + "' ";
    query += "ORDER BY c.date, c.hour";
    
    printf("Requête SQL: %s\n", query.c_str());
    
    if (mysql_query(con, query.c_str()) != 0) {
        sendResponse(socket, std::string(SEARCH_FAIL) + DB);
        printf("Erreur requête SQL: %s\n", mysql_error(con));
        return;
    }
    
    MYSQL_RES *result = mysql_store_result(con);
    if (!result) {
        sendResponse(socket, std::string(SEARCH_FAIL) + DB);
        printf("Erreur stockage résultat\n");
        return;
    }
    
    std::string response = SEARCH_OK;
    int numRows = mysql_num_rows(result);
    printf("Nombre de consultations trouvées: %d\n", numRows);
    
    if (numRows == 0) {
        sendResponse(socket, response);
        mysql_free_result(result);
        return;
    }
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (!response.empty() && response.back() != ';') {
            response += "|";
        }
        response += std::string(row[0]) + ";" + std::string(row[1]) + ";" + std::string(row[2]) + ";" + std::string(row[3]) + ";" + std::string(row[4]);
    }
    
    mysql_free_result(result);
    sendResponse(socket, response);
    printf("Réponse envoyée: %s\n", response.c_str());
}

static void handleGetSpecialties(MYSQL* con, int socket) {
    printf("Traitement GET_SPECIALTIES\n");
    
    std::string query = "SELECT name FROM specialties ORDER BY name";
    
    if (mysql_query(con, query.c_str()) != 0) {
        sendResponse(socket, "SPECIALTIES_FAIL;DB");
        return;
    }
    
    MYSQL_RES *result = mysql_store_result(con);
    if (!result) {
        sendResponse(socket, "SPECIALTIES_FAIL;DB");
        return;
    }
    
    std::string response = SPECIALTIES_OK;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (!response.empty() && response.back() != ';') {
            response += "|";
        }
        response += std::string(row[0]);
    }
    
    mysql_free_result(result);
    sendResponse(socket, response);
}

static void handleGetDoctors(MYSQL* con, int socket, const std::string &specialty) {
    printf("Traitement GET_DOCTORS pour spécialité: %s\n", specialty.c_str());
    
    std::string query = "SELECT CONCAT(d.first_name, ' ', d.last_name) ";
    query += "FROM doctors d ";
    query += "JOIN specialties s ON d.specialty_id = s.id ";
    if (specialty != TOUS) {
        query += "WHERE s.name = '" + specialty + "' ";
    }
    query += "ORDER BY d.last_name, d.first_name";
    
    printf("Requête SQL GET_DOCTORS: %s\n", query.c_str());
    
    if (mysql_query(con, query.c_str()) != 0) {
        sendResponse(socket, "DOCTORS_FAIL;DB");
        return;
    }
    
    MYSQL_RES *result = mysql_store_result(con);
    if (!result) {
        sendResponse(socket, "DOCTORS_FAIL;DB");
        return;
    }
    
    std::string response = DOCTORS_OK;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (!response.empty() && response.back() != ';') {
            response += "|";
        }
        response += std::string(row[0]);
    }
    
    mysql_free_result(result);
    sendResponse(socket, response);
}

static void handleBookConsultation(MYSQL* con, int socket, int consultationId, int patientId, const std::string &reason) {
    printf("Traitement BOOK_CONSULTATION pour consultation ID=%d, patient ID=%d\n", consultationId, patientId);
    
    // Vérifier d'abord si la consultation existe et est libre
    char query[512];
    snprintf(query, sizeof(query), "SELECT id, patient_id FROM consultations WHERE id=%d", consultationId);
    
    if (mysql_query(con, query) != 0) {
        sendResponse(socket, std::string(BOOK_FAIL) + DB);
        return;
    }
    
    MYSQL_RES *result = mysql_store_result(con);
    if (!result) {
        sendResponse(socket, std::string(BOOK_FAIL) + DB);
        return;
    }
    
    if (mysql_num_rows(result) == 0) {
        mysql_free_result(result);
        sendResponse(socket, std::string(BOOK_FAIL) + NOT_FOUND);
        return;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row[1] != NULL) { // patient_id n'est pas NULL, consultation déjà réservée
        mysql_free_result(result);
        sendResponse(socket, std::string(BOOK_FAIL) + ALREADY_BOOKED);
        return;
    }
    
    mysql_free_result(result);
    
    // Réserver la consultation
    snprintf(query, sizeof(query), "UPDATE consultations SET patient_id=%d, reason='%s' WHERE id=%d", 
             patientId, reason.c_str(), consultationId);
    
    if (mysql_query(con, query) != 0) {
        sendResponse(socket, std::string(BOOK_FAIL) + DB);
        return;
    }
    
    if (mysql_affected_rows(con) > 0) {
        sendResponse(socket, BOOK_OK);
        printf("Consultation %d réservée avec succès pour le patient %d\n", consultationId, patientId);
    } else {
        sendResponse(socket, std::string(BOOK_FAIL) + UPDATE_FAILED);
    }
}

static void* workerThread(void*){
    while(true){
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
        
        // Traitement de la connexion client
        printf("Thread traite la connexion de %s (socket %d)\n", task.ip, task.socket);
        
        char buffer[BUFFER_SIZE];
        bool clientConnected = true;
        
        // Connexion à la base de données pour ce thread
        MYSQL* con = openDb();
        if(!con) {
            printf("Erreur: Impossible de se connecter à la base de données\n");
            closeSocket(task.socket);
            continue;
        }
        
        while(clientConnected) {
            int bytesReceived = Receive(task.socket, buffer);
            
            if(bytesReceived <= 0) {
                // Client déconnecté
                printf("Client %s déconnecté (socket %d)\n", task.ip, task.socket);
                clientConnected = false;
            } else {
                // Traitement du message reçu
                std::string message(buffer);
                printf("Message reçu de %s: %s\n", task.ip, buffer);
                
                // Parser et traiter les commandes
                if(message.find(LOGIN_NEW) == 0) {
                    // Format: LOGIN_NEW;NOM;PRENOM
                    size_t pos1 = message.find(';', LOGIN_NEW_LENGTH);
                    if(pos1 != std::string::npos) {
                        std::string lastName = message.substr(LOGIN_NEW_LENGTH, pos1 - LOGIN_NEW_LENGTH);
                        std::string firstName = message.substr(pos1 + 1);
                        handleLoginNew(con, task.socket, lastName, firstName);
                    } else {
                        sendResponse(task.socket, std::string(LOGIN_FAIL) + FORMAT);
                    }
                }
                else if(message.find(LOGIN_EXIST) == 0) {
                    // Format: LOGIN_EXIST;ID;NOM;PRENOM
                    size_t pos1 = message.find(';', LOGIN_EXIST_LENGTH);
                    size_t pos2 = message.find(';', pos1 + 1);
                    if(pos1 != std::string::npos && pos2 != std::string::npos) {
                        int id = std::atoi(message.substr(LOGIN_EXIST_LENGTH, pos1 - LOGIN_EXIST_LENGTH).c_str());
                        std::string lastName = message.substr(pos1 + 1, pos2 - pos1 - 1);
                        std::string firstName = message.substr(pos2 + 1);
                        handleLoginExist(con, task.socket, id, lastName, firstName);
                    } else {
                        sendResponse(task.socket, std::string(LOGIN_FAIL) + FORMAT);
                    }
                }
                else if(message.find(SEARCH) == 0) {
                    // Format: SEARCH;SPECIALTY;DOCTOR;START_DATE;END_DATE
                    size_t pos1 = message.find(';', SEARCH_LENGTH);
                    size_t pos2 = message.find(';', pos1 + 1);
                    size_t pos3 = message.find(';', pos2 + 1);
                    
                    if(pos1 != std::string::npos && pos2 != std::string::npos && pos3 != std::string::npos) {
                        std::string specialty = message.substr(SEARCH_LENGTH, pos1 - SEARCH_LENGTH);
                        std::string doctor = message.substr(pos1 + 1, pos2 - pos1 - 1);
                        std::string startDate = message.substr(pos2 + 1, pos3 - pos2 - 1);
                        std::string endDate = message.substr(pos3 + 1);
                        handleSearch(con, task.socket, specialty, doctor, startDate, endDate);
                    } else {
                        sendResponse(task.socket, std::string(SEARCH_FAIL) + FORMAT);
                    }
                }
                else if(message.find(GET_SPECIALTIES) == 0) {
                    handleGetSpecialties(con, task.socket);
                }
                else if(message.find("GET_DOCTORS;") == 0) {
                    // Format: GET_DOCTORS;SPECIALTY
                    std::string specialty = message.substr(GET_DOCTORS_LENGTH); // "GET_DOCTORS;" = 12 caractères
                    handleGetDoctors(con, task.socket, specialty);
                }
                else if(message.find("BOOK_CONSULTATION;") == 0) {
                    // Format: BOOK_CONSULTATION;CONSULTATION_ID;PATIENT_ID;REASON
                    size_t pos1 = message.find(';', BOOK_CONSULTATION_LENGTH);
                    size_t pos2 = message.find(';', pos1 + 1);
                    
                    if(pos1 != std::string::npos && pos2 != std::string::npos) {
                        int consultationId = std::atoi(message.substr(BOOK_CONSULTATION_LENGTH, pos1 - BOOK_CONSULTATION_LENGTH).c_str());
                        int patientId = std::atoi(message.substr(pos1 + 1, pos2 - pos1 - 1).c_str());
                        std::string reason = message.substr(pos2 + 1);
                        handleBookConsultation(con, task.socket, consultationId, patientId, reason);
                    } else {
                        sendResponse(task.socket, std::string(BOOK_FAIL) + FORMAT);
                    }
                }
                else {
                    sendResponse(task.socket, std::string(LOGIN_FAIL) + UNKNOWN_CMD);
                }
            }
        }
        
        // Fermer la connexion à la base de données
        mysql_close(con);
        
        // Fermeture propre du socket client
        closeSocket(task.socket);
        printf("Socket %d fermé\n", task.socket);
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
    
    bool configLoaded = false;
    for(int i = 0; i < 3; i++) {
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
    printf("Configuration chargée: port=%d threads=%d DB=%s@%s (%s)\n", config.portReservation, config.nbThreads, config.dbUser.c_str(), config.dbHost.c_str(), config.dbName.c_str());

    int serverSocket = ServerSocket(config.portReservation);
    if(serverSocket<0){ 
        perror("ServerSocket"); 
        return 1; 
    }
    printf("Serveur en écoute sur le port %d\n", config.portReservation);

    // Création du pool
    std::vector<pthread_t> threads(config.nbThreads);
    for(int i=0;i<config.nbThreads;++i){
        pthread_create(&threads[i], nullptr, workerThread, nullptr);
    }

    while(!stop){
        char ipClient[INET_ADDRSTRLEN] = {0};
        int clientSocket = AcceptConnection(serverSocket, ipClient);
        if(clientSocket<0){ 
            perror("AcceptConnection"); 
            continue; 
        }
        printf("Connexion acceptée de %s\n", ipClient);
        pthread_mutex_lock(&mutex);
        tasks.push({clientSocket,{0}});
        std::strncpy(tasks.back().ip, ipClient, INET_ADDRSTRLEN-1);
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