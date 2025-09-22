#ifndef DATABASE_H
#define DATABASE_H

#include <mysql.h>
#include <string>

using namespace std;

// Constantes pour les requêtes
const int QUERY_SIZE = 512;

// Structure de configuration de la base de données
struct DatabaseConfig {
    string dbHost;
    string dbUser;
    string dbPass;
    string dbName;
};

// Fonctions de gestion de la base de données
class DatabaseManager {
public:
    // Configuration
    static bool loadConfig(const char* path, DatabaseConfig& config);
    
    // Initialisation et connexion
    static MYSQL* openConnection(const DatabaseConfig& config);
    static void closeConnection(MYSQL* con);
    
    // Gestion des patients
    static int createNewPatient(MYSQL* con, const string& last, const string& first);
    static bool verifyExistingPatient(MYSQL* con, int id, const string& last, const string& first);
    
    // Gestion des connexions (login)
    static void handleLoginNew(MYSQL* con, int socket, const string& last, const string& first);
    static void handleLoginExist(MYSQL* con, int socket, int id, const string& last, const string& first);
    
    // Recherche et consultation
    static void handleSearch(MYSQL* con, int socket, const string& specialty, const string& doctor, 
                           const string& startDate, const string& endDate);
    static void handleGetSpecialties(MYSQL* con, int socket);
    static void handleGetDoctors(MYSQL* con, int socket, const string& specialty);
    static void handleBookConsultation(MYSQL* con, int socket, int consultationId, int patientId, 
                                     const string& reason);
    
    
private:
    static void trim(string& s);
};

#endif // DATABASE_H
