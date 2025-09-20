#include <stdio.h>
#include <stdlib.h>
#include <mysql.h>
#include <time.h>
#include <string.h>

typedef struct {
  int  id;
  char name[50];
} SPECIALTY;

typedef struct {
  int  id;
  int  specialty_id;
  char last_name[50];
  char first_name[50];
} DOCTOR;

typedef struct {
  int  id;
  char last_name[50];
  char first_name[50];
  char birth_date[20];
} PATIENT;

typedef struct {
  int  id;
  int  doctor_id;
  int  patient_id; // -1 => NULL
  char date[20];
  char hour[10]; // stocké en TIME dans la BD
  char reason[255];
} CONSULTATION;

typedef struct {
  int  id;
  int  consultation_id;
  char description[255];
} REPORT;

SPECIALTY specialties[] = {
  {-1, "Cardiologie"},
  {-1, "Dermatologie"},
  {-1, "Neurologie"},
  {-1, "Ophtalmologie"},
  {-1, "Pédiatrie"}
};
int nbSpecialties = 5;

DOCTOR doctors[] = {
  {-1, 1, "Dupont", "Alice"},
  {-1, 2, "Lemoine", "Bernard"},
  {-1, 3, "Martin", "Claire"},
  {-1, 2, "Maboul", "Paul"},
  {-1, 1, "Coptere", "Elie"},
  {-1, 3, "Merad", "Gad"},
  {-1, 4, "Benali", "Mohammed"},
  {-1, 5, "Shala", "Donika"},
  {-1, 5, "Azrou", "Nassim"},
  {-1, 4, "Kova", "Zafina"}
};
int nbDoctors = 10;

PATIENT patients[] = {
  {-1, "Durand", "Jean", "1980-05-12"},
  {-1, "Petit", "Sophie", "1992-11-30"},
  {-1, "Benali", "Mohammed", "1987-01-09"},
  {-1, "Shala", "Donika", "1995-03-21"},
  {-1, "Azrou", "Nassim", "2001-07-14"},
  {-1, "Kova", "Zafina", "1998-09-02"}
};
int nbPatients = 6;

CONSULTATION consultations[] = {
  {-1, 3,  1, "2025-10-01", "09:00", "Check-up"},
  {-1, 1,  2, "2025-10-02", "14:30", "Premier rendez-vous"},
  {-1, 2,  1, "2025-10-03", "11:15", "Douleurs persistantes"},
  {-1, 4, -1, "2025-10-04", "09:00", ""},
  {-1, 4, -1, "2025-10-04", "09:30", ""},
  {-1, 4, -1, "2025-10-04", "10:00", ""},
  {-1, 4, -1, "2025-10-04", "10:30", ""},
  {-1, 6, -1, "2025-10-07", "13:00", ""},
  {-1, 6, -1, "2025-10-07", "13:30", ""},
  {-1, 6, -1, "2025-10-07", "14:00", ""},
  {-1, 6, -1, "2025-10-07", "14:30", ""},
  {-1, 7,  3, "2025-10-05", "10:00", "Suivi tension"},
  {-1, 8,  4, "2025-10-06", "10:15", "Rougeurs peau"},
  {-1, 9,  5, "2025-10-06", "11:45", "Migraine chronique"},
  {-1, 10, 6, "2025-10-06", "09:30", "Vision trouble"}
};
int nbConsultations = 15;

REPORT reports[] = {
  {-1, 1, "RAS"},
  {-1, 2, "Diagnostic initial, à confirmer"},
  {-1, 3, "Prescription anti-douleur"}
};
int nbReports = 3;

void finish_with_error(MYSQL *con) {
  fprintf(stderr, "%s\n", mysql_error(con));
  mysql_close(con);
  exit(1);
}

int main() {
  MYSQL* connexion = mysql_init(NULL);
  if (!mysql_real_connect(connexion, "localhost", "Student", "PassStudent1_", "PourStudent", 0, NULL, 0)) {
    finish_with_error(connexion);
  }

  mysql_query(connexion, "DROP TABLE IF EXISTS reports;");
  mysql_query(connexion, "DROP TABLE IF EXISTS consultations;");
  mysql_query(connexion, "DROP TABLE IF EXISTS patients;");
  mysql_query(connexion, "DROP TABLE IF EXISTS doctors;");
  mysql_query(connexion, "DROP TABLE IF EXISTS specialties;");

  if (mysql_query(connexion, "CREATE TABLE specialties ("
                             "id INT AUTO_INCREMENT PRIMARY KEY,"
                             "name VARCHAR(50) NOT NULL,"
                             "UNIQUE KEY uk_specialty_name(name)"
                             ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;"))
    finish_with_error(connexion);

  if (mysql_query(connexion, "CREATE TABLE doctors ("
                             "id INT AUTO_INCREMENT PRIMARY KEY, "
                             "specialty_id INT NOT NULL, "
                             "last_name VARCHAR(50) NOT NULL, "
                             "first_name VARCHAR(50) NOT NULL, "
                             "FOREIGN KEY (specialty_id) REFERENCES specialties(id)"
                             ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;"))
    finish_with_error(connexion);

  if (mysql_query(connexion, "CREATE TABLE patients ("
                             "id INT AUTO_INCREMENT PRIMARY KEY, "
                             "last_name VARCHAR(50) NOT NULL, "
                             "first_name VARCHAR(50) NOT NULL, "
                             "birth_date DATE NOT NULL"
                             ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;"))
    finish_with_error(connexion);

  if (mysql_query(connexion, "CREATE TABLE consultations ("
                             "id INT AUTO_INCREMENT PRIMARY KEY, "
                             "doctor_id INT NOT NULL, "
                             "patient_id INT NULL, "
                             "date DATE NOT NULL, "
                             "hour TIME NOT NULL, "
                             "reason VARCHAR(255) NOT NULL DEFAULT '', "
                             "FOREIGN KEY (doctor_id) REFERENCES doctors(id), "
                             "FOREIGN KEY (patient_id) REFERENCES patients(id) ON DELETE SET NULL, "
                             "UNIQUE KEY uk_slot (doctor_id, date, hour)"
                             ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;"))
    finish_with_error(connexion);
  if (mysql_query(connexion, "CREATE TABLE reports ("
                             "id INT AUTO_INCREMENT PRIMARY KEY, "
                             "consultation_id INT NOT NULL, "
                             "description TEXT NOT NULL, "
                             "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                             "FOREIGN KEY (consultation_id) REFERENCES consultations(id) ON DELETE CASCADE"
                             ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;"))
    finish_with_error(connexion);

  char request[512];
  for (int i = 0; i < nbSpecialties; i++) {
    sprintf(request, "INSERT INTO specialties VALUES (NULL, '%s');", specialties[i].name);
    if (mysql_query(connexion, request)) finish_with_error(connexion);
  }

  for (int i = 0; i < nbDoctors; i++) {
    sprintf(request, "INSERT INTO doctors VALUES (NULL, %d, '%s', '%s');",
            doctors[i].specialty_id, doctors[i].last_name, doctors[i].first_name);
    if (mysql_query(connexion, request)) finish_with_error(connexion);
  }

  for (int i = 0; i < nbPatients; i++) {
    sprintf(request, "INSERT INTO patients VALUES (NULL, '%s', '%s', '%s');",
            patients[i].last_name, patients[i].first_name, patients[i].birth_date);
    if (mysql_query(connexion, request)) finish_with_error(connexion);
  }

  for (int i = 0; i < nbConsultations; i++) {
    if (consultations[i].patient_id == -1) {
      sprintf(request, "INSERT INTO consultations (doctor_id, patient_id, date, hour, reason) "
                       "VALUES (%d, NULL, '%s', '%s', '%s');",
              consultations[i].doctor_id, consultations[i].date, consultations[i].hour, consultations[i].reason);
    } else {
      sprintf(request, "INSERT INTO consultations (doctor_id, patient_id, date, hour, reason) "
                       "VALUES (%d, %d, '%s', '%s', '%s');",
              consultations[i].doctor_id, consultations[i].patient_id, consultations[i].date,
              consultations[i].hour, consultations[i].reason);
    }
    if (mysql_query(connexion, request)) finish_with_error(connexion);
  }

  // Insérer quelques reports liés aux premières consultations (en supposant auto-inc commence à 1)
  for (int i = 0; i < nbReports; i++) {
    sprintf(request, "INSERT INTO reports (consultation_id, description) VALUES (%d, '%s');", reports[i].consultation_id, reports[i].description);
    if (mysql_query(connexion, request)) finish_with_error(connexion);
  }

  mysql_close(connexion);
  return 0;
}

