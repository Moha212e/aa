#include "name.h"

// Protocole de communication - Constantes
const char* LOGIN_OK = "LOGIN_OK;";
const char* LOGIN_FAIL = "LOGIN_FAIL;";
const char* LOGIN_EXIST = "LOGIN_EXIST;";
const char* LOGIN_NEW = "LOGIN_NEW;";
const char* NOT_FOUND = "NOT_FOUND";
const char* TOUTES = "--- TOUTES ---";
const char* TOUS = "--- TOUS ---";

// Messages de recherche
const char* SEARCH = "SEARCH;";
const char* SEARCH_FAIL = "SEARCH_FAIL;";
const char* SEARCH_OK = "SEARCH_OK;";

// Messages de spécialités
const char* SPECIALTIES_FAIL = "SPECIALTIES_FAIL;";
const char* SPECIALTIES_OK = "SPECIALTIES_OK;";
const char* GET_SPECIALTIES = "GET_SPECIALTIES;";

// Messages de médecins
const char* DOCTORS_FAIL = "DOCTORS_FAIL;";
const char* DOCTORS_OK = "DOCTORS_OK;";
const char* GET_DOCTORS = "GET_DOCTORS;";

// Messages de réservation
const char* BOOK_CONSULTATION = "BOOK_CONSULTATION;";
const char* BOOK_OK = "BOOK_OK";
const char* BOOK_FAIL = "BOOK_FAIL;";

// Messages d'erreur
const char* FORMAT = "FORMAT";
const char* UNKNOWN_CMD = "UNKNOWN_CMD";
const char* DB = "DB";
const char* INSERT = "INSERT";
const char* ALREADY_BOOKED = "ALREADY_BOOKED";
const char* UPDATE_FAILED = "UPDATE_FAILED";

// Configuration DB
const char* DB_HOST = "DB_HOST";
const char* DB_USER = "DB_USER";
const char* DB_PASS = "DB_PASS";
const char* DB_NAME = "DB_NAME";