#ifndef NAME_H
#define NAME_H

// Protocole de communication - Constantes
extern const char* LOGIN_OK;
extern const char* LOGIN_FAIL;
extern const char* LOGIN_EXIST;
extern const char* LOGIN_NEW;
extern const char* NOT_FOUND;
extern const char* TOUTES;
extern const char* TOUS;

// Messages de recherche
extern const char* SEARCH;
extern const char* SEARCH_FAIL;
extern const char* SEARCH_OK;

// Messages de spécialités
extern const char* SPECIALTIES_FAIL;
extern const char* SPECIALTIES_OK;
extern const char* GET_SPECIALTIES;

// Messages de médecins
extern const char* DOCTORS_FAIL;
extern const char* DOCTORS_OK;
extern const char* GET_DOCTORS;

// Messages de réservation
extern const char* BOOK_CONSULTATION;
extern const char* BOOK_OK;
extern const char* BOOK_FAIL;

// Messages d'erreur
extern const char* FORMAT;
extern const char* UNKNOWN_CMD;
extern const char* DB;
extern const char* INSERT;
extern const char* ALREADY_BOOKED;
extern const char* UPDATE_FAILED;

// Configuration DB
extern const char* DB_HOST;
extern const char* DB_USER;
extern const char* DB_PASS;
extern const char* DB_NAME;

#endif // NAME_H