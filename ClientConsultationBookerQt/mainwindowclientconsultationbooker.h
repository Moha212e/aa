#ifndef MAINWINDOWCLIENTCONSULTATIONBOOKER_H
#define MAINWINDOWCLIENTCONSULTATIONBOOKER_H

#include <QMainWindow>
#include <string>
using namespace std;
#include "../socket/socket.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindowClientConsultationBooker; }
QT_END_NAMESPACE

class MainWindowClientConsultationBooker : public QMainWindow
{
    Q_OBJECT

public:
    MainWindowClientConsultationBooker(QWidget *parent = nullptr);
    ~MainWindowClientConsultationBooker();

    void addTupleTableConsultations(int id, string specialty, string doctor, string date, string hour);
    void clearTableConsultations();
    int getSelectionIndexTableConsultations() const;

    void addComboBoxSpecialties(string specialty);
    string getSelectionSpecialty() const;
    void clearComboBoxSpecialties();

    void addComboBoxDoctors(string doctor);
    string getSelectionDoctor() const;
    void clearComboBoxDoctors();

    string getLastName() const;
    string getFirstName() const;
    int getPatientId() const;
    bool isNewPatientSelected() const;
    string getStartDate() const;
    string getEndDate() const;
    void setLastName(string value);
    void setFirstName(string value);
    void setPatientId(int value);
    void setNewPatientChecked(bool state);
    void setStartDate(string date);
    void setEndDate(string date);

    void loginOk();
    void logoutOk();

    void dialogMessage(const string& title,const string& message);
    void dialogError(const string& title,const string& message);
    string dialogInputText(const string& title,const string& question);
    int dialogInputInt(const string& title,const string& question);
    bool C_connectToServer = false; // Indicateur de connexion au serveur
    int C_clientSocket = -1; // Socket client pour la communication avec le serveur
    bool connectToServer();
    
    // Fonctions de communication avec le serveur
    bool sendToServer(const string& message);
    string receiveFromServer();
    bool handleLoginResponse(const string& response);
    
    // Fonctions de recherche
    bool handleSearchResponse(const string& response);
    void loadSpecialties();
    void loadDoctors();
    void loadAllDoctors();
    
    // Fonctions de réservation
    bool handleBookResponse(const string& response);
    void bookConsultation(int consultationId, const string& reason);

private slots:
    void on_pushButtonLogin_clicked();
    void on_pushButtonLogout_clicked();
    void on_pushButtonRechercher_clicked();
    void on_pushButtonReserver_clicked();

private:
    Ui::MainWindowClientConsultationBooker *ui;
};
#endif // MAINWINDOWCLIENTCONSULTATIONBOOKER_H
