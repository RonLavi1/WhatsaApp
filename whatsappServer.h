//
// Created by ron.lavi1 on 6/21/18.
//

#ifndef WHATSAPPSERVER_WHATSAPPSERVER_H
#define WHATSAPPSERVER_WHATSAPPSERVER_H

#define MAX_PENDING_CONNECTIONS 10
#include <netinet/in.h>
#include <map>
#include "whatsappio.h"


class whatsappServer
{
private:

/* ---------- INITIALIZING VARIABLES ---------- */
    char buffer[WA_MAX_INPUT]; //MAYBE DO THIS FIELD AS CHAR?
    char serverInputBuffer[WA_MAX_INPUT]; //MAYBE DO THIS FIELD AS CHAR?
    int mainSocket;
    int portNumber;
    int clientFD;
    int addressLength = sizeof(serverAddress);

    fd_set readFDSet;
    sockaddr_in serverAddress;

    std::map <std::string, int> clientSockets; // key:client-name , value: fd_num
    //will hold all the users connected

    std::map<std::string, std::vector<std::string>> groups;
    //key is group name & value is users/clients in the group

    //Returns values from the parser
    command_type commandT;
    std::string name;
    std::string message;
    std::vector<std::string> clients;
    std::string feedback;
public:
    explicit whatsappServer(char* port);

    int validatePort(char *portInput);

    void run();

    std::string connectedClients(std::map<std::string, int> clientSockets);

    void readFromClient(int clientFd);

    void serverInput();

    void setServerAddress();

    void removeDuplicateNames(std::string currentClient);

    void setMainSocket();

    void writeToClient(int clientFD, std::string messageToClient);

    void setFileDescriptors();

    void newIncomingClient();

    void clientNewInput();

    int fdSetClients();
};

#endif //WHATSAPPSERVER_WHATSAPPSERVER_H
