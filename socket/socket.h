#ifndef SOCKET_H
#define SOCKET_H
#define TAILLE_MAX 1024
#include <sys/socket.h>
#include <netinet/in.h>
int ServerSocket(int port);
int AcceptConnection(int socketEcoute , char* ipClient);
int ClientSocket(const char* ipServeur, int port);
int Send(int sSocket, const char *data, int taille);
int Receive(int sSocket, char *data);
int closeSocket(int sSocket);
#endif