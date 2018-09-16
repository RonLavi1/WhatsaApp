#include <zconf.h>
#include <algorithm>
#include "whatsappClient.h"
#include "whatsappio.h"

int main(int argc, char *argv[])
{
    //Example for an Input: whatsappClient Naama 127.0.0.1 8875
    if (argc != 4)
    {
        print_client_usage();
        exit(1);
    }

    whatsappClient client = whatsappClient(argv[1],argv[2],argv[3]);
    client.run();
}

whatsappClient::whatsappClient(char* clientN, char* ipAdd,char* port)
{
    clientName = std::string(clientN);
    if (!isValidName(clientName))
    {
        print_client_usage();
        exit(1);
    }
    portNumber = validatePort(port);
    ipAddress = ipAdd;
    clientSetServerAddress();
    createSocket();
    connectClient();
}

/**
 * validate the name given has characters and\or digits only
 * @param name
 * @return If the name is valid returns true, false o.w
 */
bool whatsappClient::isValidName(std::string name)
{
    if (name.size() > WA_MAX_INPUT)
    {
        return false;
    }
    for (char c : name)
    {
        if (!isalnum(c))
        {
            return false;
        }
    }
    return true;
}

/**
 * Checks if the given by input port is valid and converts it to integer
 * @param portInput the given by input port
 * @return the port as an integer
 */
int whatsappClient::validatePort(char *portInput)
{
    try
    {
        return std::stoi(portInput);
    }
    catch (const std::exception &e)
    {
        print_client_usage();
        exit(1);
    }
}


void whatsappClient::clientSetServerAddress()
{
    //TAKEN FROM THE TUTORIAL: hostnet initialization
    hp = gethostbyname(ipAddress); /* get our address info */
    if (hp == nullptr)
    {
        print_client_usage();
        exit(1);
    }
    //serverAddress initialization
    memset(&serverAddress, 0, sizeof(struct sockaddr_in));
    /* this is our host address */
    serverAddress.sin_family = static_cast<sa_family_t>(hp->h_addrtype);
    memcpy(&serverAddress.sin_addr, hp->h_addr, static_cast<size_t>(hp->h_length));
    /* this is our port number */
    serverAddress.sin_port = htons(static_cast<uint16_t>(portNumber));
}

void whatsappClient::createSocket()
{
    clientFD = socket(AF_INET, SOCK_STREAM, 0);
    if (clientFD < 0)
    {
        close(clientFD);
        print_fail_connection();
        exit(1);
    }
}


void whatsappClient::setFileDescriptors()
{
    FD_ZERO(&readFileDescriptors);
    FD_ZERO(&writeFileDescriptors);
    FD_SET(STDIN_FILENO, &readFileDescriptors);
    FD_SET(clientFD, &readFileDescriptors);
}


void whatsappClient::sendClientName()
{
    memset(buffer, '\0', WA_MAX_INPUT); //Clears the buffer
    for (unsigned int i = 0; i < clientName.size(); i++)
    {
        buffer[i] = clientName.at(i);
    }
    if (write(clientFD, buffer, WA_MAX_INPUT) < 0)
    {
        print_error("write", errno);
        exit(1);
    }
    //Getting response from the server if client name already exists
    setFileDescriptors();

    if (select(clientFD + 1, &readFileDescriptors, &writeFileDescriptors, nullptr, nullptr) < 0)
    {
        print_error("select", errno);
        exit(1);
    }
    if(FD_ISSET(clientFD,&readFileDescriptors)){

        readFromServer();
        if (std::string(buffer) == "Failed")
        {
            print_dup_connection();
            close(clientFD);
            exit(1);
        }
    }
    if(FD_ISSET(STDIN_FILENO,&readFileDescriptors)){
        readCommand();
    }
    if(FD_ISSET(clientFD, &writeFileDescriptors)){
        writeToServer();
    }
}

void whatsappClient::connectClient()
{
    if (connect(clientFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == 0) //Success
    {
        //SENDS THE CLIENT NAME TO THE SERVER:
        sendClientName();
        print_connection();
    }
    else
    {
        print_fail_connection();
        close(clientFD);
        exit(1);
    }
}

void whatsappClient::readFromServer()
{
    int bytesRead = 0;
    int totalBytes = 0;
    memset(buffer, '\0', WA_MAX_INPUT); //Clears the buffer

    // read from the socket into the buffer: similarly to whats been in the tirgul
    while (totalBytes < WA_MAX_INPUT)
    {
        bytesRead = static_cast<int>(read(clientFD, ((char*)buffer + totalBytes),
                                          static_cast<size_t>(WA_MAX_INPUT - totalBytes)));
        if (bytesRead < 1)
        {
            print_exit();
            shutdown(clientFD, SHUT_RDWR);
            close(clientFD);
            exit(1);
        }
        totalBytes += bytesRead;
    }
}

/***
 * Writes the buffer content to the server.
 * @param clientFD the file descriptor of the client
 * @param buffer the content to write
 */
void whatsappClient::writeToServer()
{
    if (write(clientFD, buffer, WA_MAX_INPUT) < 0)
    {
        print_error("write", errno);
        exit(1);
    }
}

bool whatsappClient::readCommand()
{
    bool isValidCommand;

    fgets(buffer, WA_MAX_INPUT, stdin);
    //Case of empty string
    *std::remove(buffer, buffer+strlen(buffer), '\n') = '\0';
    if(std::string(buffer).empty())
    {
        print_invalid_input();
        return false;
    }
    // parse the command and verify if invalid
    try
    {
        parse_command(buffer, commandT, name, message, clients);
    }
    catch (const std::exception &e)
    {
        print_invalid_input();
        return false;
    }
    switch (commandT)
    {
        case CREATE_GROUP:
            /*
                Sends request to create a new group named “group_name” with "
                <list_of_client_names>" as group members. “group_name” is unique (i.e. no
                other group or client​ with this name is allowed) and includes only
                letters and digits. <list_of_client_names> is separated by comma without
                any spaces.For example: create_group osStaff david,eshed,eytan,yair"
            */
            isValidCommand = isValidName(name); //valid name is a must
            if (!isValidCommand)
            {
                print_create_group(false, false, clientName, name);
                return false;
            }
            //VERIFY VALID CLIENTS LIST
            for (auto &item1:clients)
            {
                if (!isValidName(item1))
                {
                    print_create_group(false, false, clientName, name);
                    return false;
                }
            }
            for (const auto &item : clients) //at least one who is not the sender
            {
                if (item != clientName)
                {
                    return true;
                }
            }
            print_create_group(false, false, clientName, name);
            return false;
        case SEND:
            /*
                If name is a client name it sends <sender_client_name>: <message> only to the
                specified client. If name is a group name it sends <sender_client_name>:
                <message> to all group members (except the sender client).
            */
            isValidCommand = isValidName(name);
            if (!isValidCommand || (name == clientName))
            {
                print_send(false, false, clientName, name, message);
                return false;
            }
            return true;
        case WHO:
            /*
                Sends a request (to the server) to receive a list (might be empty) of
                currently connected client names (alphabetically order), separated by comma
                without spaces.
            */
            return true;
        case EXIT:
            /*
                Unregisters the client from the server and removes it from all groups. After
                the server unregistered the client, the client should print “Unregistered
                successfully” and then exit(0).
            */
            return true;
        case INVALID:
            print_invalid_input();
            return false;
    }
    return false;
}

void whatsappClient::readFeedback()
{
    bool commandMadeSuccessfully;
    feedback = std::string(buffer);
    const char* s;
    char c[WA_MAX_INPUT];
    char* bufferP = buffer;
    std::vector<std::string> clientsList;
    //RECEIVE FEEDBACK FROM SERVER...
    switch (commandT)
    {
        case CREATE_GROUP:
            /*
                Sends request to create a new group named “group_name” with "
                <list_of_client_names>" as group members. “group_name” is unique (i.e. no
                other group or client​ with this name is allowed) and includes only
                letters and digits. <list_of_client_names> is separated by comma without
                any spaces.For example: create_group osStaff david,eshed,eytan,yair"
            */
            commandMadeSuccessfully = feedback == "Succeed";
            print_create_group(false, commandMadeSuccessfully, clientName, name);
            return;
        case SEND:
            /*
                If name is a client name it sends <sender_client_name>: <message> only to the
                specified client. If name is a group name it sends <sender_client_name>:
                <message> to all group members (except the sender client).
            */
            commandMadeSuccessfully = feedback == "Succeed";
            print_send(false, commandMadeSuccessfully, clientName, name, message);
            return;
        case WHO:

            /*
                Sends a request (to the server) to receive a list (might be empty) of
                currently connected client names (alphabetically order), separated by comma
                without spaces.
            */

            if (feedback == "Failed")
            {
                print_who_client(false, clients);
                return;
            }
            //Algorithm taken from the parser
            strcpy(c, buffer);
            s = strtok_r(c,",",&bufferP);
            clientsList.emplace_back(s);
            while((s = strtok_r(nullptr, ",", &bufferP)) != nullptr) {
                clientsList.emplace_back(s);
            }
            print_who_client(true, clientsList);
            return;
        case EXIT:
            /*
                Unregisters the client from the server and removes it from all groups. After
                the server unregistered the client, the client should print “Unregistered
                successfully” and then exit(0).
            */
            commandMadeSuccessfully = feedback == "Succeed";
            if (!commandMadeSuccessfully)
            {
                print_error("readFeedback", errno);
                exit(1);
            }
            if (shutdown(clientFD, SHUT_RDWR) < 0)
            {
                print_error("shutdown", errno);
                exit(1);
            }
            if (close(clientFD) < 0) //Close the file descriptor FD.
            {
                print_error("close", errno);
                exit(1);
            }
            print_exit(false, "");
            exit(0);
        case INVALID:
            return;
    }
}

void whatsappClient::run()
{
    while (true)
    {
        setFileDescriptors();
        // wait for new data to arrive from any source
        if (select(clientFD + 1, &readFileDescriptors, &writeFileDescriptors, nullptr,
                   nullptr) < 1)
        {
            print_error("select", errno);
            exit(1);
        }
        //READS INPUT FROM THE CLIENT
        if (FD_ISSET(STDIN_FILENO, &readFileDescriptors))
        {
            isValCommand = readCommand();
            if(isValCommand)
            {
                FD_SET(clientFD, &writeFileDescriptors);
            }
            else
            {
                //Should not write to the server
                FD_ZERO(&writeFileDescriptors);
            }
        }
        //WRITE COMMAND FROM THE CLIENT TO THE SERVER
        if (FD_ISSET(clientFD, &writeFileDescriptors))
        {
            writeToServer();
        }
        //GET FEEDBACK FROM THE SERVER AND RESPONSE ACCORDINGLY
        if (FD_ISSET(clientFD, &readFileDescriptors))
        {
            readFromServer();
            if(isValCommand)
            {
                readFeedback();
                isValCommand = false;
            }
            else //The server invoked the file descriptor and sent a message
            {
                printf("%s\n", buffer);
            }
        }
    }

}