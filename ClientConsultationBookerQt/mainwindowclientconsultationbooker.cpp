#include "mainwindowclientconsultationbooker.h"
#include "ui_mainwindowclientconsultationbooker.h"
#include <QInputDialog>
#include <QMessageBox>
#include <iostream>
#include "../util/name.h"
using namespace std;



static const char *SERVER_IP = "127.0.0.1";
static const int SERVER_PORT = 12345;

MainWindowClientConsultationBooker::MainWindowClientConsultationBooker(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowClientConsultationBooker)
{
    ui->setupUi(this);
    logoutOk();

    // Configuration de la table des employes (Personnel Garage)
    ui->tableWidgetConsultations->setColumnCount(5);
    ui->tableWidgetConsultations->setRowCount(0);
    QStringList labelsTableConsultations;
    labelsTableConsultations << "Id" << "Spécialité" << "Médecin" << "Date" << "Heure";
    ui->tableWidgetConsultations->setHorizontalHeaderLabels(labelsTableConsultations);
    ui->tableWidgetConsultations->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableWidgetConsultations->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidgetConsultations->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidgetConsultations->horizontalHeader()->setVisible(true);
    ui->tableWidgetConsultations->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidgetConsultations->verticalHeader()->setVisible(false);
    ui->tableWidgetConsultations->horizontalHeader()->setStyleSheet("background-color: lightyellow");
    int columnWidths[] = {40, 150, 200, 150, 100};
    for (int col = 0; col < 5; ++col)
        ui->tableWidgetConsultations->setColumnWidth(col, columnWidths[col]);
    if (connectToServer()){
        // Charger les spécialités au démarrage
        loadSpecialties();
        // Charger tous les médecins (sans filtre de spécialité)
        loadAllDoctors();
    } else {
        dialogError("Connexion", "Échec de la connexion au serveur");
    }
}

MainWindowClientConsultationBooker::~MainWindowClientConsultationBooker()
{
    if (C_connectToServer && C_clientSocket >= 0){
        printf("Fermeture de la connexion au serveur dans le destructeur\n");
        closeSocket(C_clientSocket);
        C_clientSocket = -1;
        C_connectToServer = false;
    }
    delete ui;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions utiles Table des livres encodés (ne pas modifier) ////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindowClientConsultationBooker::addTupleTableConsultations(int id,
                                                                    string specialty,
                                                                    string doctor,
                                                                    string date,
                                                                    string hour)
{
    int nb = ui->tableWidgetConsultations->rowCount();
    nb++;
    ui->tableWidgetConsultations->setRowCount(nb);
    ui->tableWidgetConsultations->setRowHeight(nb-1,10);

    // id
    QTableWidgetItem *item = new QTableWidgetItem;
    item->setTextAlignment(Qt::AlignCenter);
    item->setText(QString::number(id));
    ui->tableWidgetConsultations->setItem(nb-1,0,item);

    // specialty
    item = new QTableWidgetItem;
    item->setTextAlignment(Qt::AlignCenter);
    item->setText(QString::fromStdString(specialty));
    ui->tableWidgetConsultations->setItem(nb-1,1,item);

    // doctor
    item = new QTableWidgetItem;
    item->setTextAlignment(Qt::AlignCenter);
    item->setText(QString::fromStdString(doctor));
    ui->tableWidgetConsultations->setItem(nb-1,2,item);

    // date
    item = new QTableWidgetItem;
    item->setTextAlignment(Qt::AlignCenter);
    item->setText(QString::fromStdString(date));
    ui->tableWidgetConsultations->setItem(nb-1,3,item);

    // hour
    item = new QTableWidgetItem;
    item->setTextAlignment(Qt::AlignCenter);
    item->setText(QString::fromStdString(hour));
    ui->tableWidgetConsultations->setItem(nb-1,4,item);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindowClientConsultationBooker::clearTableConsultations() {
    ui->tableWidgetConsultations->setRowCount(0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
int MainWindowClientConsultationBooker::getSelectionIndexTableConsultations() const
{
    QModelIndexList list = ui->tableWidgetConsultations->selectionModel()->selectedRows();
    if (list.size() == 0) return -1;
    QModelIndex index = list.at(0);
    int ind = index.row();
    return ind;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions utiles des comboboxes (ne pas modifier) //////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindowClientConsultationBooker::addComboBoxSpecialties(string specialty) {
    ui->comboBoxSpecialties->addItem(QString::fromStdString(specialty));
}

string MainWindowClientConsultationBooker::getSelectionSpecialty() const {
    return ui->comboBoxSpecialties->currentText().toStdString();
}

void MainWindowClientConsultationBooker::clearComboBoxSpecialties() {
    ui->comboBoxSpecialties->clear();
    this->addComboBoxSpecialties("--- TOUTES ---");
}

void MainWindowClientConsultationBooker::addComboBoxDoctors(string doctor) {
    ui->comboBoxDoctors->addItem(QString::fromStdString(doctor));
}

string MainWindowClientConsultationBooker::getSelectionDoctor() const {
    return ui->comboBoxDoctors->currentText().toStdString();
}

void MainWindowClientConsultationBooker::clearComboBoxDoctors() {
    ui->comboBoxDoctors->clear();
    this->addComboBoxDoctors("--- TOUS ---");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonction utiles de la fenêtre (ne pas modifier) ////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
string MainWindowClientConsultationBooker::getLastName() const {
    return ui->lineEditLastName->text().toStdString();
}

string MainWindowClientConsultationBooker::getFirstName() const {
    return ui->lineEditFirstName->text().toStdString();
}

int MainWindowClientConsultationBooker::getPatientId() const {
    return ui->spinBoxId->value();
}

void MainWindowClientConsultationBooker::setLastName(string value) {
    ui->lineEditLastName->setText(QString::fromStdString(value));
}

string MainWindowClientConsultationBooker::getStartDate() const {
    return ui->dateEditStartDate->date().toString("yyyy-MM-dd").toStdString();
}

string MainWindowClientConsultationBooker::getEndDate() const {
    return ui->dateEditEndDate->date().toString("yyyy-MM-dd").toStdString();
}

void MainWindowClientConsultationBooker::setFirstName(string value) {
    ui->lineEditFirstName->setText(QString::fromStdString(value));
}

void MainWindowClientConsultationBooker::setPatientId(int value) {
    if (value > 0) ui->spinBoxId->setValue(value);
}

bool MainWindowClientConsultationBooker::isNewPatientSelected() const {
    return ui->checkBoxNewPatient->isChecked();
}

void MainWindowClientConsultationBooker::setNewPatientChecked(bool state) {
    ui->checkBoxNewPatient->setChecked(state);
}

void MainWindowClientConsultationBooker::setStartDate(string date) {
    QDate qdate = QDate::fromString(QString::fromStdString(date), "yyyy-MM-dd");
    if (qdate.isValid()) ui->dateEditStartDate->setDate(qdate);
}

void MainWindowClientConsultationBooker::setEndDate(string date) {
    QDate qdate = QDate::fromString(QString::fromStdString(date), "yyyy-MM-dd");
    if (qdate.isValid()) ui->dateEditEndDate->setDate(qdate);
}

void MainWindowClientConsultationBooker::loginOk() {
    ui->lineEditLastName->setReadOnly(true);
    ui->lineEditFirstName->setReadOnly(true);
    ui->spinBoxId->setReadOnly(true);
    ui->checkBoxNewPatient->setEnabled(false);
    ui->pushButtonLogout->setEnabled(true);
    ui->pushButtonLogin->setEnabled(false);
    ui->pushButtonRechercher->setEnabled(true);
    ui->pushButtonReserver->setEnabled(true);
}

void MainWindowClientConsultationBooker::logoutOk() {
    ui->lineEditLastName->setReadOnly(false);
    setLastName("");
    ui->lineEditFirstName->setReadOnly(false);
    setFirstName("");
    ui->spinBoxId->setReadOnly(false);
    setPatientId(1);
    ui->checkBoxNewPatient->setEnabled(true);
    setNewPatientChecked(false);
    ui->pushButtonLogout->setEnabled(false);
    ui->pushButtonLogin->setEnabled(true);
    ui->pushButtonRechercher->setEnabled(false);
    ui->pushButtonReserver->setEnabled(false);
    setStartDate("2025-09-15");
    setEndDate("2025-12-31");
    clearComboBoxDoctors();
    clearComboBoxSpecialties();
    clearTableConsultations();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions permettant d'afficher des boites de dialogue (ne pas modifier) ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindowClientConsultationBooker::dialogMessage(const string& title,const string& message) {
   QMessageBox::information(this,QString::fromStdString(title),QString::fromStdString(message));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindowClientConsultationBooker::dialogError(const string& title,const string& message) {
   QMessageBox::critical(this,QString::fromStdString(title),QString::fromStdString(message));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
string MainWindowClientConsultationBooker::dialogInputText(const string& title,const string& question) {
    return QInputDialog::getText(this,QString::fromStdString(title),QString::fromStdString(question)).toStdString();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
int MainWindowClientConsultationBooker::dialogInputInt(const string& title,const string& question) {
    return QInputDialog::getInt(this,QString::fromStdString(title),QString::fromStdString(question));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions gestion des boutons (TO DO) //////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindowClientConsultationBooker::on_pushButtonLogin_clicked()
{
    string lastName = this->getLastName();
    string firstName = this->getFirstName();
    int patientId = this->getPatientId();
    bool newPatient = this->isNewPatientSelected();

    cout << "lastName = " << lastName << endl;
    cout << "FirstName = " << firstName << endl;
    cout << "patientId = " << patientId << endl;
    cout << "newPatient = " << newPatient << endl;

    if ((lastName == "") || (firstName == "")){
        dialogError("Login", "Le nom et le prénom doivent être renseignés");
        return;
    }
    
    // Vérifier la connexion au serveur
    if (!connectToServer()) {
        return;
    }
    
    string message;
    if (newPatient) {
        // Nouveau patient
        message = string(LOGIN_NEW) + lastName + ";" + firstName;
    } else {
        // Patient existant
        message = string(LOGIN_EXIST) + to_string(patientId) + ";" + lastName + ";" + firstName;
    }
    
    // Envoyer la requête
    if (sendToServer(message)) {
        // Recevoir la réponse
        string response = receiveFromServer();
        if (!response.empty()) {
            handleLoginResponse(response);
        }
    }
}

void MainWindowClientConsultationBooker::on_pushButtonLogout_clicked()
{
    // Fermer la connexion au serveur
    if (C_connectToServer && C_clientSocket >= 0) {
        closeSocket(C_clientSocket);
        C_clientSocket = -1;
        C_connectToServer = false;
        printf("Connexion au serveur fermée\n");
    }
    logoutOk();
}

void MainWindowClientConsultationBooker::on_pushButtonRechercher_clicked()
{
    string specialty = this->getSelectionSpecialty();
    string doctor = this->getSelectionDoctor();
    string startDate = this->getStartDate();
    string endDate = this->getEndDate();

    cout << "specialty = " << specialty << endl;
    cout << "doctor = " << doctor << endl;
    cout << "startDate = " << startDate << endl;
    cout << "endDate = " << endDate << endl;
    
    // Vérifier la connexion au serveur
    if (!connectToServer()) {
        return;
    }
    
    // Construire le message de recherche
    string message = string(SEARCH) + specialty + ";" + doctor + ";" + startDate + ";" + endDate;
    
    // Envoyer la requête
    if (sendToServer(message)) {
        // Recevoir la réponse
        string response = receiveFromServer();
        if (!response.empty()) {
            handleSearchResponse(response);
        }
    }
}

void MainWindowClientConsultationBooker::on_pushButtonReserver_clicked()
{
    int selectedRow = this->getSelectionIndexTableConsultations();

    cout << "selectedRow = " << selectedRow << endl;
    
    if (selectedRow < 0) {
        dialogError("Réservation", "Veuillez sélectionner une consultation à réserver");
        return;
    }
    
    // Récupérer l'ID de la consultation sélectionnée
    QTableWidgetItem *item = ui->tableWidgetConsultations->item(selectedRow, 0);
    if (!item) {
        dialogError("Réservation", "Erreur lors de la récupération de la consultation");
        return;
    }
    
    int consultationId = item->text().toInt();
    cout << "Consultation ID sélectionnée: " << consultationId << endl;
    
    // Demander la raison de la consultation
    string reason = dialogInputText("Réservation", "Veuillez indiquer la raison de votre consultation:");
    if (reason.empty()) {
        dialogError("Réservation", "La raison de la consultation est obligatoire");
        return;
    }
    
    // Effectuer la réservation
    bookConsultation(consultationId, reason);
}
bool MainWindowClientConsultationBooker::connectToServer()
{
    if (C_connectToServer){
        return true;
    }
    C_clientSocket = ClientSocket(SERVER_IP, SERVER_PORT);
    if (C_clientSocket < 0)
    {
        dialogError("Connexion", "Impossible de se connecter au serveur");
        return false;
    }
    C_connectToServer = true;
    return true;
}

bool MainWindowClientConsultationBooker::sendToServer(const string& message)
{
    if (!C_connectToServer || C_clientSocket < 0) {
        dialogError("Erreur", "Pas de connexion au serveur");
        return false;
    }
    
    int result = Send(C_clientSocket, message.c_str(), message.length());
    if (result < 0) {
        dialogError("Erreur", "Impossible d'envoyer le message au serveur");
        return false;
    }
    printf("Message envoyé: %s\n", message.c_str());
    return true;
}

string MainWindowClientConsultationBooker::receiveFromServer()
{
    if (!C_connectToServer || C_clientSocket < 0) {
        return "";
    }
    
    char buffer[1024];
    int result = Receive(C_clientSocket, buffer);
    if (result <= 0) {
        dialogError("Erreur", "Impossible de recevoir la réponse du serveur");
        return "";
    }
    
    buffer[result] = '\0';
    string response(buffer);
    printf("Message reçu: %s\n", response.c_str());
    return response;
}

bool MainWindowClientConsultationBooker::handleLoginResponse(const string& response)
{
    if (response.find(LOGIN_OK) == 0) {
        // Extraire l'ID du patient
        size_t pos = response.find(';');
        if (pos != string::npos) {
            string idStr = response.substr(pos + 1);
            int patientId = stoi(idStr);
            setPatientId(patientId);
            loginOk();
            dialogMessage("Login", "Connexion réussie ! ID patient: " + idStr);
            return true;
        }
    } else if (response.find(LOGIN_FAIL) == 0) {
        // Extraire la raison de l'échec
        size_t pos = response.find(';');
        if (pos != string::npos) {
            string reason = response.substr(pos + 1);
            dialogError("Erreur Login", "Échec de la connexion: " + reason);
        } else {
            dialogError("Erreur Login", "Échec de la connexion");
        }
    } else {
        dialogError("Erreur", "Réponse inattendue du serveur: " + response);
    }
    return false;
}

bool MainWindowClientConsultationBooker::handleSearchResponse(const string& response)
{
    if (response.find(SEARCH_OK) == 0) {
        // Vider la table actuelle
        clearTableConsultations();
        
        // Parser les résultats
        string data = response.substr(10); // Enlever "SEARCH_OK;"
        
        if (data.empty()) {
            dialogMessage("Recherche", "Aucune consultation disponible pour ces critères");
            return true;
        }
        
        // Diviser par "|" pour obtenir chaque consultation
        size_t pos = 0;
        while (pos < data.length()) {
            size_t nextPos = data.find('|', pos);
            string consultation;
            
            if (nextPos == string::npos) {
                consultation = data.substr(pos);
                pos = data.length();
            } else {
                consultation = data.substr(pos, nextPos - pos);
                pos = nextPos + 1;
            }
            
            // Parser chaque consultation: ID;SPECIALTY;DOCTOR;DATE;HOUR
            size_t pos1 = consultation.find(';');
            size_t pos2 = consultation.find(';', pos1 + 1);
            size_t pos3 = consultation.find(';', pos2 + 1);
            size_t pos4 = consultation.find(';', pos3 + 1);
            
            if (pos1 != string::npos && pos2 != string::npos && pos3 != string::npos && pos4 != string::npos) {
                int id = stoi(consultation.substr(0, pos1));
                string specialty = consultation.substr(pos1 + 1, pos2 - pos1 - 1);
                string doctor = consultation.substr(pos2 + 1, pos3 - pos2 - 1);
                string date = consultation.substr(pos3 + 1, pos4 - pos3 - 1);
                string hour = consultation.substr(pos4 + 1);
                
                addTupleTableConsultations(id, specialty, doctor, date, hour);
            }
        }
        
        dialogMessage("Recherche", "Recherche terminée - " + to_string(ui->tableWidgetConsultations->rowCount()) + " consultation(s) trouvée(s)");
        return true;
    } else if (response.find(SEARCH_FAIL) == 0) {
        size_t pos = response.find(';');
        if (pos != string::npos) {
            string reason = response.substr(pos + 1);
            dialogError("Erreur Recherche", "Échec de la recherche: " + reason);
        } else {
            dialogError("Erreur Recherche", "Échec de la recherche");
        }
    } else {
        dialogError("Erreur", "Réponse inattendue du serveur: " + response);
    }
    return false;
}

void MainWindowClientConsultationBooker::loadSpecialties()
{
    if (!connectToServer()) return;
    
    if (sendToServer(GET_SPECIALTIES)) {
        string response = receiveFromServer();
        if (response.find(SPECIALTIES_OK) == 0) {
            string data = response.substr(15); // Enlever "SPECIALTIES_OK;"
            clearComboBoxSpecialties();
            
            // Ajouter l'option "--- TOUTES ---" en premier
            addComboBoxSpecialties(TOUTES);
            
            size_t pos = 0;
            while (pos < data.length()) {
                size_t nextPos = data.find('|', pos);
                string specialty;
                
                if (nextPos == string::npos) {
                    specialty = data.substr(pos);
                    pos = data.length();
                } else {
                    specialty = data.substr(pos, nextPos - pos);
                    pos = nextPos + 1;
                }
                
                addComboBoxSpecialties(specialty);
            }
        }
    }
}

void MainWindowClientConsultationBooker::loadAllDoctors()
{
    if (!connectToServer()) return;
    
    if (sendToServer(string(GET_DOCTORS) + TOUS)) {
        string response = receiveFromServer();
        if (response.find(DOCTORS_OK) == 0) {
            string data = response.substr(11); // Enlever "DOCTORS_OK;"
            clearComboBoxDoctors();
            
            // Ajouter l'option "--- TOUS ---" en premier
            addComboBoxDoctors("--- TOUS ---");
            
            size_t pos = 0;
            while (pos < data.length()) {
                size_t nextPos = data.find('|', pos);
                string doctor;
                
                if (nextPos == string::npos) {
                    doctor = data.substr(pos);
                    pos = data.length();
                } else {
                    doctor = data.substr(pos, nextPos - pos);
                    pos = nextPos + 1;
                }
                
                addComboBoxDoctors(doctor);
            }
        }
    }
}

void MainWindowClientConsultationBooker::loadDoctors()
{
    if (!connectToServer()) return;
    
    string specialty = getSelectionSpecialty();
    if (specialty == TOUTES) specialty = TOUS;
    
    if (sendToServer(string(GET_DOCTORS) + specialty)) {
        string response = receiveFromServer();
        if (response.find(DOCTORS_OK) == 0) {
            string data = response.substr(11); // Enlever "DOCTORS_OK;"
            clearComboBoxDoctors();
            
            size_t pos = 0;
            while (pos < data.length()) {
                size_t nextPos = data.find('|', pos);
                string doctor;
                
                if (nextPos == string::npos) {
                    doctor = data.substr(pos);
                    pos = data.length();
                } else {
                    doctor = data.substr(pos, nextPos - pos);
                    pos = nextPos + 1;
                }
                
                addComboBoxDoctors(doctor);
            }
        }
    }
}

void MainWindowClientConsultationBooker::bookConsultation(int consultationId, const string& reason)
{
    if (!connectToServer()) return;
    
    int patientId = getPatientId();
    string message = string(BOOK_CONSULTATION) + to_string(consultationId) + ";" + to_string(patientId) + ";" + reason;
    
    if (sendToServer(message)) {
        string response = receiveFromServer();
        if (!response.empty()) {
            handleBookResponse(response);
        }
    }
}

bool MainWindowClientConsultationBooker::handleBookResponse(const string& response)
{
    if (response.find(BOOK_OK) == 0) {
        dialogMessage("Réservation", "Consultation réservée avec succès !");
        // Rafraîchir la liste des consultations
        on_pushButtonRechercher_clicked();
        return true;
    } else if (response.find(BOOK_FAIL) == 0) {
        size_t pos = response.find(';');
        if (pos != string::npos) {
            string reason = response.substr(pos + 1);
            dialogError("Erreur Réservation", "Échec de la réservation: " + reason);
        } else {
            dialogError("Erreur Réservation", "Échec de la réservation");
        }
    } else {
        dialogError("Erreur", "Réponse inattendue du serveur: " + response);
    }
    return false;
}