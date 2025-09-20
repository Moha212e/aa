
#include "socket.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
int Send(int sSocket, const char *data, int taille)
{
    // Ajouter le caractère de fin de transmission
    char message[TAILLE_MAX + 1];
    strncpy(message, data, taille);
    message[taille] = '\n'; // Caractère de fin de transmission
    
    int totalEnvoye = 0;
    int tailleComplete = taille + 1; // +1 pour le \n
    while (totalEnvoye < tailleComplete) {
        int envoye = send(sSocket, message + totalEnvoye, tailleComplete - totalEnvoye, 0);
        if (envoye <= 0) return -1;
        totalEnvoye += envoye;
    }
    return totalEnvoye;
}

int Receive(int sSocket, char *data)
{
    char buffer[TAILLE_MAX];
    int totalRecu = 0;
    
    while (totalRecu < TAILLE_MAX) {
        int recu = recv(sSocket, buffer + totalRecu, TAILLE_MAX - totalRecu, 0);
        if (recu <= 0) return -1;
        
        totalRecu += recu;
        
        // Chercher le caractère de fin de transmission dans le buffer
        for (int i = 0; i < totalRecu; i++) {
            if (buffer[i] == '\n') {
                // Trouvé ! Copier le message sans le \n
                buffer[i] = '\0';
                strcpy(data, buffer);
                return i; // Retourner la taille du message sans le \n
            }
        }
    }
    
    // Si on arrive ici, le message est trop long
    return -1;
}
int ServerSocket(int port)
{
    int sSocket; // socket de la connexion serveur descriptor (file descriptor)
    struct sockaddr_in adresseServeur;

    sSocket = socket(AF_INET, SOCK_STREAM, 0); // création du socket de la connexion serveur TCP (SOCK_STREAM)
    if (sSocket < 0) return -1;

    adresseServeur.sin_family = AF_INET;
    adresseServeur.sin_addr.s_addr = INADDR_ANY;
    adresseServeur.sin_port = htons(port);

    if (bind(sSocket, (struct sockaddr *)&adresseServeur, sizeof(adresseServeur)) < 0) {
        closeSocket(sSocket);
        return -1;
    }

    if (listen(sSocket, 5) < 0) {
        closeSocket(sSocket);
        return -1;
    }

    return sSocket;
}
int AcceptConnection(int socketEcoute , char* ipClient)
{
    struct sockaddr_in adresseClient;
    socklen_t tailleAdresse = sizeof(adresseClient);
    int sSocket;

    sSocket = accept(socketEcoute, (struct sockaddr *)&adresseClient, &tailleAdresse);
    if (sSocket < 0) return -1;

    if (ipClient != NULL) {
        snprintf(ipClient, INET_ADDRSTRLEN, "%s", inet_ntoa(adresseClient.sin_addr));
    }

    return sSocket;
}
int ClientSocket(const char* ipServeur, int port){
    int sSocket;
    struct sockaddr_in adresseServeur;

    sSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (sSocket < 0) return -1;

    adresseServeur.sin_family = AF_INET;
    adresseServeur.sin_addr.s_addr = inet_addr(ipServeur);
    adresseServeur.sin_port = htons(port);

    if (connect(sSocket, (struct sockaddr *)&adresseServeur, sizeof(adresseServeur)) < 0) {
        closeSocket(sSocket);
        return -1;
    }

    return sSocket;
}
int closeSocket(int sSocket){
    if (close(sSocket) < 0) {
        return -1;
    }
    return 0;
}