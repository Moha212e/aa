#include <stdio.h>
#include <stdlib.h>
#include <mysql.h>

void finish_with_error(MYSQL *con) {
  fprintf(stderr, "Erreur MySQL: %s\n", mysql_error(con));
  mysql_close(con);
  exit(1);
}

int main() {
  printf("=== SUPPRESSION ET RECRÉATION DE LA BASE DE DONNÉES ===\n");
  
  // Connexion au serveur MySQL (sans base de données spécifique)
  MYSQL* connexion = mysql_init(NULL);
  if (!mysql_real_connect(connexion, "localhost", "Student", "PassStudent1_", NULL, 0, NULL, 0)) {
    finish_with_error(connexion);
  }
  
  printf("✓ Connexion au serveur MySQL réussie\n");
  
  // Supprimer la base de données si elle existe
  printf("🗑️  Suppression de la base de données 'PourStudent'...\n");
  if (mysql_query(connexion, "DROP DATABASE IF EXISTS PourStudent;")) {
    finish_with_error(connexion);
  }
  printf("✓ Base de données supprimée\n");
  
  // Créer la base de données
  printf("📝 Création de la base de données 'PourStudent'...\n");
  if (mysql_query(connexion, "CREATE DATABASE PourStudent;")) {
    finish_with_error(connexion);
  }
  printf("✓ Base de données créée\n");
  
  // Sélectionner la base de données
  if (mysql_query(connexion, "USE PourStudent;")) {
    finish_with_error(connexion);
  }
  printf("✓ Base de données sélectionnée\n");
  
  // Créer les tables
  printf("📋 Création des tables...\n");
  
  if (mysql_query(connexion, "CREATE TABLE specialties (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(30));"))
    finish_with_error(connexion);
  printf("  ✓ Table 'specialties' créée\n");
  
  if (mysql_query(connexion, "CREATE TABLE doctors ("
                             "id INT AUTO_INCREMENT PRIMARY KEY, "
                             "specialty_id INT, "
                             "last_name VARCHAR(30), "
                             "first_name VARCHAR(30), "
                             "FOREIGN KEY (specialty_id) REFERENCES specialties(id));"))
    finish_with_error(connexion);
  printf("  ✓ Table 'doctors' créée\n");
  
  if (mysql_query(connexion, "CREATE TABLE patients ("
                             "id INT AUTO_INCREMENT PRIMARY KEY, "
                             "last_name VARCHAR(30), "
                             "first_name VARCHAR(30), "
                             "birth_date DATE);"))
    finish_with_error(connexion);
  printf("  ✓ Table 'patients' créée\n");
  
  if (mysql_query(connexion, "CREATE TABLE consultations ("
                             "id INT AUTO_INCREMENT PRIMARY KEY, "
                             "doctor_id INT NOT NULL, "
                             "patient_id INT NULL, "
                             "date DATE, "
                             "hour VARCHAR(10), "
                             "reason VARCHAR(100), "
                             "FOREIGN KEY (doctor_id) REFERENCES doctors(id), "
                             "FOREIGN KEY (patient_id) REFERENCES patients(id));"))
    finish_with_error(connexion);
  printf("  ✓ Table 'consultations' créée\n");
  
  // Insérer les données
  printf("📊 Insertion des données...\n");
  
  // Spécialités
  const char* specialties[] = {
    "Cardiologie", "Dermatologie", "Neurologie", "Ophtalmologie"
  };
  int nbSpecialties = 4;
  
  for (int i = 0; i < nbSpecialties; i++) {
    char request[256];
    sprintf(request, "INSERT INTO specialties VALUES (NULL, '%s');", specialties[i]);
    if (mysql_query(connexion, request)) finish_with_error(connexion);
  }
  printf("  ✓ %d spécialités insérées\n", nbSpecialties);
  
  // Médecins
  struct {
    int specialty_id;
    const char* last_name;
    const char* first_name;
  } doctors[] = {
    {1, "Dupont", "Alice"},
    {2, "Lemoine", "Bernard"},
    {3, "Martin", "Claire"},
    {2, "Maboul", "Paul"},
    {1, "Coptere", "Elie"},
    {3, "Merad", "Gad"}
  };
  int nbDoctors = 6;
  
  for (int i = 0; i < nbDoctors; i++) {
    char request[256];
    sprintf(request, "INSERT INTO doctors VALUES (NULL, %d, '%s', '%s');",
            doctors[i].specialty_id, doctors[i].last_name, doctors[i].first_name);
    if (mysql_query(connexion, request)) finish_with_error(connexion);
  }
  printf("  ✓ %d médecins insérés\n", nbDoctors);
  
  // Patients
  struct {
    const char* last_name;
    const char* first_name;
    const char* birth_date;
  } patients[] = {
    {"Durand", "Jean", "1980-05-12"},
    {"Petit", "Sophie", "1992-11-30"}
  };
  int nbPatients = 2;
  
  for (int i = 0; i < nbPatients; i++) {
    char request[256];
    sprintf(request, "INSERT INTO patients VALUES (NULL, '%s', '%s', '%s');",
            patients[i].last_name, patients[i].first_name, patients[i].birth_date);
    if (mysql_query(connexion, request)) finish_with_error(connexion);
  }
  printf("  ✓ %d patients insérés\n", nbPatients);
  
  // Consultations
  struct {
    int doctor_id;
    int patient_id;
    const char* date;
    const char* hour;
    const char* reason;
  } consultations[] = {
    {3,  1, "2025-10-01", "09:00", "Check-up"},
    {1,  2, "2025-10-02", "14:30", "Premier rendez-vous"},
    {2,  1, "2025-10-03", "11:15", "Douleurs persistantes"},
    {4, -1, "2025-10-04", "9:00", ""},
    {4, -1, "2025-10-04", "9:30", ""},
    {4, -1, "2025-10-04", "10:00", ""},
    {4, -1, "2025-10-04", "10:30", ""},
    {6, -1, "2025-10-07", "13:00", ""},
    {6, -1, "2025-10-07", "13:30", ""},
    {6, -1, "2025-10-07", "14:00", ""},
    {6, -1, "2025-10-07", "14:30", ""}
  };
  int nbConsultations = 11;
  
  for (int i = 0; i < nbConsultations; i++) {
    char request[512];
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
  printf("  ✓ %d consultations insérées\n", nbConsultations);
  
  mysql_close(connexion);
  
  printf("\n🎉 BASE DE DONNÉES RÉINITIALISÉE AVEC SUCCÈS !\n");
  printf("📊 Résumé :\n");
  printf("   - %d spécialités\n", nbSpecialties);
  printf("   - %d médecins\n", nbDoctors);
  printf("   - %d patients\n", nbPatients);
  printf("   - %d consultations\n", nbConsultations);
  
  return 0;
}
