//
// Created by ron.lavi1 on 6/21/18.
//

#ifndef WHATSAPPCLIENT_WHATSAPPCLIENT_H
#define WHATSAPPCLIENT_WHATSAPPCLIENT_H


#include <netinet/in.h>
#include <netdb.h>
#include "whatsappio.h"

class whatsappClient
{
private:
    //Info that will be extracted from the given command:
    command_type commandT;
    std::string name;
    std::string message;
    std::vector<std::string> clients;

    std::string feedback;
    int clientFD;
    bool isValCommand;
    // Structs:
    sockaddr_in serverAddress;
    hostent *hp;
    fd_set readFileDescriptors;
    fd_set writeFileDescriptors;
    char buffer[WA_MAX_INPUT]; //Buffer

    //Input given by user:
    char *ipAddress; //being validated in clientSetServerAddress
    int portNumber ; //storing port number on which the accepts connections
    std::string clientName; //converts char* to string



public:
    whatsappClient(char *clientName, char *ipAddress, char *port);

    void run();

    bool isValidName(std::__cxx11::basic_string<char> name);

    int validatePort(char *portInput);

    void clientSetServerAddress();

    void createSocket();

    void connectClient();

    void setFileDescriptors();

    void writeToServer();

    void readFromServer();

    void sendClientName();

    bool readCommand();

    void readFeedback();
};


#endif //WHATSAPPCLIENT_WHATSAPPCLIENT_H
