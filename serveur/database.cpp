#include "database.h"
#include "../socket/socket.h"
#include "../util/name.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>

// ==================== FONCTIONS UTILITAIRES ====================

// Fonction utilitaire pour nettoyer les chaînes
void DatabaseManager::trim(string& s) {
    while(!s.empty() && (s.back()=='\n' || s.back()=='\r' || s.back()==' '|| s.back()=='\t')) {
        s.pop_back();
    }
    size_t i=0; 
    while(i<s.size() && (s[i]==' '||s[i]=='\t')) {
        ++i; 
    }
    s.erase(0,i);
}

// Fonction utilitaire pour exécuter une requête SQL et gérer les erreurs
static bool executeQuery(MYSQL* con, const string& query, const string& errorMsg) {
    if (mysql_query(con, query.c_str()) != 0) {
        printf("Erreur SQL: %s - %s\n", errorMsg.c_str(), mysql_error(con));
        return false;
    }
    return true;
}

// Fonction utilitaire pour envoyer une réponse d'erreur
static void sendError(int socket, const string& errorType, const string& errorCode) {
    string response = errorType + errorCode;
    Send(socket, response.c_str(), response.length());
}

// Fonction utilitaire pour construire une réponse avec des données
static void buildDataResponse(int socket, const string& okPrefix, MYSQL_RES* result, 
                            function<string(MYSQL_ROW)> formatRow) {
    string response = okPrefix;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (!response.empty() && response.back() != ';') {
            response += "|";
        }
        response += formatRow(row);
    }
    Send(socket, response.c_str(), response.length());
}

// ==================== CONFIGURATION ====================

// Chargement de la configuration de la base de données
bool DatabaseManager::loadConfig(const char* path, DatabaseConfig& config) {
    ifstream in(path);
    if(!in) {
        return false;
    }
    string line;
    while(getline(in, line)) {
        trim(line);
        if(line.empty() || line[0]=='#') {
            continue;
        }
        auto pos = line.find('=');
        if(pos == string::npos) {
            continue;
        }
        string key = line.substr(0, pos); 
        trim(key);
        string val = line.substr(pos + 1); 
        trim(val);
        
        if(key == DB_HOST) {
            config.dbHost = val;
        }
        else if(key == DB_USER) {
            config.dbUser = val;
        }
        else if(key == DB_PASS) {
            config.dbPass = val;
        }
        else if(key == DB_NAME) {
            config.dbName = val;
        }
    }
    return !config.dbHost.empty() && !config.dbUser.empty() && !config.dbName.empty();
}


// ==================== CONNEXION ====================

// Connexion à la base de données
MYSQL* DatabaseManager::openConnection(const DatabaseConfig& config) {
    MYSQL* con = mysql_init(NULL);
    if(!con) return NULL;
    if(!mysql_real_connect(con, config.dbHost.c_str(), config.dbUser.c_str(), 
                          config.dbPass.c_str(), config.dbName.c_str(), 0, NULL, 0)){
        mysql_close(con);
        return NULL;
    }
    return con;
}

// Fermeture de la connexion
void DatabaseManager::closeConnection(MYSQL* con) {
    if(con) {
        mysql_close(con);
    }
}


// ==================== GESTION DES PATIENTS ====================

// Création d'un nouveau patient
int DatabaseManager::createNewPatient(MYSQL* con, const string& last, const string& first) {
    // Validation des entrées
    if(last.empty() || first.empty() || last.length() > 50 || first.length() > 50) {
        return -1;
    }
    
    // Échappement des caractères spéciaux
    string escapedLast = last;
    string escapedFirst = first;
    
    // Remplacer les caractères dangereux
    size_t pos = 0;
    while((pos = escapedLast.find("'", pos)) != string::npos) {
        escapedLast.replace(pos, 1, "''");
        pos += 2;
    }
    pos = 0;
    while((pos = escapedFirst.find("'", pos)) != string::npos) {
        escapedFirst.replace(pos, 1, "''");
        pos += 2;
    }
    
    char query[QUERY_SIZE];
    snprintf(query, sizeof(query), "INSERT INTO patients (last_name, first_name, birth_date) VALUES ('%s','%s','2000-01-01')", 
             escapedLast.c_str(), escapedFirst.c_str());
    if(mysql_query(con, query)!=0) return -1;
    return (int) mysql_insert_id(con);
}

// Vérification d'un patient existant
bool DatabaseManager::verifyExistingPatient(MYSQL* con, int id, const string& last, const string& first) {
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

// ==================== GESTION DES CONNEXIONS ====================

// Gestion de la connexion nouveau patient
void DatabaseManager::handleLoginNew(MYSQL* con, int socket, const string& last, const string& first) {
    printf("Traitement LOGIN_NEW pour %s %s\n", last.c_str(), first.c_str());
    
    int patientId = createNewPatient(con, last, first);
    if(patientId > 0) {
        string response = string(LOGIN_OK) + to_string(patientId);
        Send(socket, response.c_str(), response.length());
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
        Send(socket, (string(LOGIN_FAIL) + INSERT).c_str(), (string(LOGIN_FAIL) + INSERT).length());
        printf("Erreur lors de la création du patient\n");
    }
}

// Gestion de la connexion patient existant
void DatabaseManager::handleLoginExist(MYSQL* con, int socket, int id, const string& last, const string& first) {
    printf("Traitement LOGIN_EXIST pour ID=%d, %s %s\n", id, last.c_str(), first.c_str());
    
    if(verifyExistingPatient(con, id, last, first)) {
        string response = string(LOGIN_OK) + to_string(id);
        Send(socket, response.c_str(), response.length());
        printf("Patient existant vérifié avec succès\n");
    } else {
        Send(socket, (string(LOGIN_FAIL) + NOT_FOUND).c_str(), (string(LOGIN_FAIL) + NOT_FOUND).length());
        printf("Patient non trouvé ou données incorrectes\n");
    }
}

// ==================== RECHERCHE ET RÉSERVATION ====================

// Recherche de consultations
void DatabaseManager::handleSearch(MYSQL* con, int socket, const string& specialty, const string& doctor, 
                                 const string& startDate, const string& endDate) {
    printf("Traitement SEARCH: specialty=%s, doctor=%s, startDate=%s, endDate=%s\n", 
           specialty.c_str(), doctor.c_str(), startDate.c_str(), endDate.c_str());
    
    // Construire la requête SQL
    string query = "SELECT c.id, s.name, CONCAT(d.first_name, ' ', d.last_name), c.date, c.hour ";
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
        Send(socket, (string(SEARCH_FAIL) + DB).c_str(), (string(SEARCH_FAIL) + DB).length());
        printf("Erreur requête SQL: %s\n", mysql_error(con));
        return;
    }
    
    MYSQL_RES *result = mysql_store_result(con);
    if (!result) {
        Send(socket, (string(SEARCH_FAIL) + DB).c_str(), (string(SEARCH_FAIL) + DB).length());
        printf("Erreur stockage résultat\n");
        return;
    }
    
    string response = SEARCH_OK;
    int numRows = mysql_num_rows(result);
    printf("Nombre de consultations trouvées: %d\n", numRows);
    
    if (numRows == 0) {
        Send(socket, response.c_str(), response.length());
        mysql_free_result(result);
        return;
    }
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (!response.empty() && response.back() != ';') {
            response += "|";
        }
        response += string(row[0]) + ";" + string(row[1]) + ";" + string(row[2]) + ";" + string(row[3]) + ";" + string(row[4]);
        printf("Consultation trouvée: ID=%s, Spécialité=%s, Médecin=%s, Date=%s, Heure=%s\n", 
               row[0], row[1], row[2], row[3], row[4]);
        
    }
    
    mysql_free_result(result);
    Send(socket, response.c_str(), response.length());
}

// ==================== RÉCUPÉRATION DE DONNÉES ====================

// Récupération des spécialités
void DatabaseManager::handleGetSpecialties(MYSQL* con, int socket) {
    printf("Traitement GET_SPECIALTIES\n");
    
    string query = "SELECT name FROM specialties ORDER BY name";
    
    if (!executeQuery(con, query, "GET_SPECIALTIES")) {
        sendError(socket, "SPECIALTIES_FAIL", ";DB");
        return;
    }
    
    MYSQL_RES *result = mysql_store_result(con);
    if (!result) {
        sendError(socket, "SPECIALTIES_FAIL", ";DB");
        return;
    }
    
    string response = SPECIALTIES_OK;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (!response.empty() && response.back() != ';') {
            response += "|";
        }
        response += string(row[0]);
        printf("Spécialité trouvée: %s\n", row[0]);
    }
    
    Send(socket, response.c_str(), response.length());
    mysql_free_result(result);
}

// Récupération des médecins
void DatabaseManager::handleGetDoctors(MYSQL* con, int socket, const string& specialty) {
    printf("Traitement GET_DOCTORS pour spécialité: %s\n", specialty.c_str());
    
    string query = "SELECT CONCAT(d.first_name, ' ', d.last_name) ";
    query += "FROM doctors d ";
    query += "JOIN specialties s ON d.specialty_id = s.id ";
    if (specialty != TOUS) {
        query += "WHERE s.name = '" + specialty + "' ";
    }
    query += "ORDER BY d.last_name, d.first_name";
    
    printf("Requête SQL GET_DOCTORS: %s\n", query.c_str());
    
    if (!executeQuery(con, query, "GET_DOCTORS")) {
        sendError(socket, "DOCTORS_FAIL", ";DB");
        return;
    }
    
    MYSQL_RES *result = mysql_store_result(con);
    if (!result) {
        sendError(socket, "DOCTORS_FAIL", ";DB");
        return;
    }
    
    string response = DOCTORS_OK;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (!response.empty() && response.back() != ';') {
            response += "|";
        }
        response += string(row[0]);
        printf("Médecin trouvé: %s\n", row[0]);
    }
    
    Send(socket, response.c_str(), response.length());
    mysql_free_result(result);
}

// Réservation de consultation
void DatabaseManager::handleBookConsultation(MYSQL* con, int socket, int consultationId, int patientId, 
                                           const string& reason) {
    printf("Traitement BOOK_CONSULTATION pour consultation ID=%d, patient ID=%d\n", consultationId, patientId);
    
    // Vérifier d'abord si la consultation existe et est libre
    char query[512];
    snprintf(query, sizeof(query), "SELECT id, patient_id FROM consultations WHERE id=%d", consultationId);
    
    if (!executeQuery(con, query, "BOOK_CONSULTATION_CHECK")) {
        sendError(socket, BOOK_FAIL, DB);
        return;
    }
    
    MYSQL_RES *result = mysql_store_result(con);
    if (!result) {
        sendError(socket, BOOK_FAIL, DB);
        return;
    }
    
    if (mysql_num_rows(result) == 0) {
        mysql_free_result(result);
        sendError(socket, BOOK_FAIL, NOT_FOUND);
        return;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row[1] != NULL) { // patient_id n'est pas NULL, consultation déjà réservée
        mysql_free_result(result);
        sendError(socket, BOOK_FAIL, ALREADY_BOOKED);
        return;
    }
    
    mysql_free_result(result);
    
    // Réserver la consultation
    snprintf(query, sizeof(query), "UPDATE consultations SET patient_id=%d, reason='%s' WHERE id=%d", 
             patientId, reason.c_str(), consultationId);
    
    if (!executeQuery(con, query, "BOOK_CONSULTATION_UPDATE")) {
        sendError(socket, BOOK_FAIL, DB);
        return;
    }
    
    if (mysql_affected_rows(con) > 0) {
        Send(socket, BOOK_OK, strlen(BOOK_OK));
        printf("Consultation %d réservée avec succès pour le patient %d\n", consultationId, patientId);
    } else {
        sendError(socket, BOOK_FAIL, UPDATE_FAILED);
    }
}
