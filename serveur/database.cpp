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
    if (!in) {
        return false;
    }
    
    string line, key, val;
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
        
        if (key == DB_HOST) {
            config.dbHost = val;
        } else if (key == DB_USER) {
            config.dbUser = val;
        } else if (key == DB_PASS) {
            config.dbPass = val;
        } else if (key == DB_NAME) {
            config.dbName = val;
        }
    }
    return !config.dbHost.empty() && !config.dbUser.empty() && !config.dbName.empty();
}


// ==================== CONNEXION ====================

// Connexion à la base de données
MYSQL* DatabaseManager::openConnection(const DatabaseConfig& config) {
    MYSQL* con = mysql_init(NULL);
    if (!con) {
        return NULL;
    }
    if (!mysql_real_connect(con, config.dbHost.c_str(), config.dbUser.c_str(), 
                          config.dbPass.c_str(), config.dbName.c_str(), 0, NULL, 0)) {
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
    if (last.empty() || first.empty()) {
        return -1;
    }
    
    char query[QUERY_SIZE];
    snprintf(query, sizeof(query), "INSERT INTO patients (last_name, first_name, birth_date) VALUES ('%s','%s','2000-01-01')", 
             last.c_str(), first.c_str());
    if (mysql_query(con, query) != 0) {
        return -1;
    }
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
    int patientId = createNewPatient(con, last, first);
    if (patientId > 0) {
        string response = string(LOGIN_OK) + to_string(patientId);
        Send(socket, response.c_str(), response.length());
    } else {
        Send(socket, (string(LOGIN_FAIL) + INSERT).c_str(), (string(LOGIN_FAIL) + INSERT).length());
    }
}

// Gestion de la connexion patient existant
void DatabaseManager::handleLoginExist(MYSQL* con, int socket, int id, const string& last, const string& first) {
    if (verifyExistingPatient(con, id, last, first)) {
        string response = string(LOGIN_OK) + to_string(id);
        Send(socket, response.c_str(), response.length());
    } else {
        Send(socket, (string(LOGIN_FAIL) + NOT_FOUND).c_str(), (string(LOGIN_FAIL) + NOT_FOUND).length());
    }
}

// ==================== RECHERCHE ET RÉSERVATION ====================

// Recherche de consultations
void DatabaseManager::handleSearch(MYSQL* con, int socket, const string& specialty, const string& doctor, 
                                 const string& startDate, const string& endDate) {
    string query = "SELECT c.id, s.name, CONCAT(d.first_name, ' ', d.last_name), c.date, c.hour ";
    query += "FROM consultations c ";
    query += "JOIN doctors d ON c.doctor_id = d.id ";
    query += "JOIN specialties s ON d.specialty_id = s.id ";
    query += "WHERE c.patient_id IS NULL ";
    
    if (specialty != TOUTES) {
        query += "AND s.name = '" + specialty + "' ";
    }
    if (doctor != TOUS) {
        query += "AND CONCAT(d.first_name, ' ', d.last_name) = '" + doctor + "' ";
    }
    query += "AND c.date BETWEEN '" + startDate + "' AND '" + endDate + "' ";
    query += "ORDER BY c.date, c.hour";
    
    if (mysql_query(con, query.c_str()) != 0) {
        Send(socket, (string(SEARCH_FAIL) + DB).c_str(), (string(SEARCH_FAIL) + DB).length());
        return;
    }
    
    MYSQL_RES *result = mysql_store_result(con);
    if (!result) {
        Send(socket, (string(SEARCH_FAIL) + DB).c_str(), (string(SEARCH_FAIL) + DB).length());
        return;
    }
    
    string response = SEARCH_OK;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (!response.empty() && response.back() != ';') {
            response += "|";
        }
        response += string(row[0]) + ";" + string(row[1]) + ";" + string(row[2]) + ";" + string(row[3]) + ";" + string(row[4]);
    }
    
    mysql_free_result(result);
    Send(socket, response.c_str(), response.length());
}

// ==================== RÉCUPÉRATION DE DONNÉES ====================

// Récupération des spécialités
void DatabaseManager::handleGetSpecialties(MYSQL* con, int socket) {
    string query = "SELECT name FROM specialties ORDER BY name";
    
    if (mysql_query(con, query.c_str()) != 0) {
        Send(socket, (string(SPECIALTIES_FAIL) + DB).c_str(), (string(SPECIALTIES_FAIL) + DB).length());
        return;
    }
    
    MYSQL_RES *result = mysql_store_result(con);
    if (!result) {
        Send(socket, (string(SPECIALTIES_FAIL) + DB).c_str(), (string(SPECIALTIES_FAIL) + DB).length());
        return;
    }
    
    string response = SPECIALTIES_OK;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (!response.empty() && response.back() != ';') {
            response += "|";
        }
        response += string(row[0]);
    }
    
    Send(socket, response.c_str(), response.length());
    mysql_free_result(result);
}

// Récupération des médecins
void DatabaseManager::handleGetDoctors(MYSQL* con, int socket, const string& specialty) {
    string query = "SELECT CONCAT(d.first_name, ' ', d.last_name) ";
    query += "FROM doctors d ";
    query += "JOIN specialties s ON d.specialty_id = s.id ";
    if (specialty != TOUS) {
        query += "WHERE s.name = '" + specialty + "' ";
    }
    query += "ORDER BY d.last_name, d.first_name";
    
    if (mysql_query(con, query.c_str()) != 0) {
        Send(socket, (string(DOCTORS_FAIL) + DB).c_str(), (string(DOCTORS_FAIL) + DB).length());
        return;
    }
    
    MYSQL_RES *result = mysql_store_result(con);
    if (!result) {
        Send(socket, (string(DOCTORS_FAIL) + DB).c_str(), (string(DOCTORS_FAIL) + DB).length());
        return;
    }
    
    string response = DOCTORS_OK;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (!response.empty() && response.back() != ';') {
            response += "|";
        }
        response += string(row[0]);
    }
    
    Send(socket, response.c_str(), response.length());
    mysql_free_result(result);
}

// Réservation de consultation
void DatabaseManager::handleBookConsultation(MYSQL* con, int socket, int consultationId, int patientId, 
                                           const string& reason) {
    char query[512];
    snprintf(query, sizeof(query), "SELECT patient_id FROM consultations WHERE id=%d", consultationId);
    
    if (mysql_query(con, query) != 0) {
        Send(socket, (string(BOOK_FAIL) + DB).c_str(), (string(BOOK_FAIL) + DB).length());
        return;
    }
    
    MYSQL_RES *result = mysql_store_result(con);
    if (!result || mysql_num_rows(result) == 0) {
        mysql_free_result(result);
        Send(socket, (string(BOOK_FAIL) + NOT_FOUND).c_str(), (string(BOOK_FAIL) + NOT_FOUND).length());
        return;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row[0] != NULL) {
        mysql_free_result(result);
        Send(socket, (string(BOOK_FAIL) + ALREADY_BOOKED).c_str(), (string(BOOK_FAIL) + ALREADY_BOOKED).length());
        return;
    }
    
    mysql_free_result(result);
    
    snprintf(query, sizeof(query), "UPDATE consultations SET patient_id=%d, reason='%s' WHERE id=%d", 
             patientId, reason.c_str(), consultationId);
    
    if (mysql_query(con, query) != 0 || mysql_affected_rows(con) == 0) {
        Send(socket, (string(BOOK_FAIL) + UPDATE_FAILED).c_str(), (string(BOOK_FAIL) + UPDATE_FAILED).length());
    } else {
        Send(socket, BOOK_OK, strlen(BOOK_OK));
    }
}
