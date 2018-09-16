#include <fcgios.h>
#include <algorithm>
#include <zconf.h>
#include "whatsappServer.h"


int main(int argc, char *argv[])
{
    //Example for an Input: whatsappServer 8875
    if (argc != 2)
    {
        print_server_usage();
        exit(1);
    }
    whatsappServer server = whatsappServer(argv[1]);
    server.run();
}

whatsappServer::whatsappServer(char *port)
{
    portNumber = validatePort(port);
    setServerAddress();
    setMainSocket();
}

int whatsappServer::validatePort(char *portInput)
{
    try
    {
        int portNum = std::stoi(portInput);
        return portNum;
    }
    catch (const std::exception &e)
    {
        print_server_usage();
        exit(1);
    }
}

void whatsappServer::setServerAddress()
{
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htons(INADDR_ANY); //Address to accept any incoming messages
    serverAddress.sin_port = htons(static_cast<uint16_t>(portNumber));
}

void whatsappServer::setMainSocket()
{
    //create the main socket
    mainSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (mainSocket < 0)
    {
        print_fail_connection();
        exit(1);
    }

    //bind the socket to localhost by port given
    if (bind(mainSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0)
    {
        print_fail_connection();
        exit(1);
    }
    // The listen system call allows the process to listen on the socket for connections.
    if (listen(mainSocket, MAX_PENDING_CONNECTIONS)< 0)
    {
        print_fail_connection();
        exit(1);
    }
}

void whatsappServer::setFileDescriptors()
{
    //clear the socket set
    FD_ZERO(&readFDSet);
    //add main socket to set
    FD_SET(mainSocket, &readFDSet);
    FD_SET(STDIN_FILENO, &readFDSet);
}

int whatsappServer::fdSetClients()
{
    int maxFD = mainSocket;
    //add active sockets to set
    for (const auto &client:clientSockets)
    {
        if (client.second > 0)
        {
            FD_SET(client.second, &readFDSet);
        }
        if(maxFD < client.second)
        {
            maxFD = client.second;
        }
    }
    return maxFD;
}

void whatsappServer::serverInput()
{
    memset(serverInputBuffer, '\0', WA_MAX_INPUT);
    fgets(serverInputBuffer, WA_MAX_INPUT, stdin);
    if (std::string(serverInputBuffer) == "EXIT\n")
    {
        print_exit();
        exit(0);
    }
    print_invalid_input();
}

void whatsappServer::readFromClient(int clientFd)
{
    int bytesRead = 0;
    int totalBytes = 0;
    memset(buffer, '\0', WA_MAX_INPUT); //Clears the buffer

    // read from the socket into the buffer: similarly to whats been in the tirgul
    while (totalBytes < WA_MAX_INPUT)
    {
        bytesRead = static_cast<int>(read(clientFd, (buffer + totalBytes),
                                          static_cast<size_t>(WA_MAX_INPUT - totalBytes)));
        if (bytesRead < 1)
        {
            print_error("read()", errno);
            exit(1);
        }
        totalBytes += bytesRead;
    }
}

void whatsappServer::writeToClient(int clientFD, std::string messageToClient)
{
    memset(buffer, '\0', WA_MAX_INPUT);
    for (unsigned long i = 0; (i < messageToClient.size() && i < WA_MAX_INPUT); i++)
    {
        buffer[i] = messageToClient.at(i);
    }
    if (write(clientFD, buffer, WA_MAX_INPUT) < 0)
    {
        print_error("writeToClient", errno);
        exit(1);
    }
}

void whatsappServer::newIncomingClient()
{
    int newClient = accept(mainSocket, (struct sockaddr *) &serverAddress,
                           (socklen_t *) &addressLength);
    if (newClient < 0)
    {
        print_error("accept", errno);
        exit(1);
    }
    //Gets the client name.
    readFromClient(newClient);
    bool existingUser = static_cast<bool>(clientSockets.count(std::string(buffer)));
    if (existingUser)
    {
        //THERE IS ALREADY USER IN THIS NAME CONNECTED AND INFORM THE CLIENT
        feedback = "Failed";
        writeToClient(newClient,feedback);
    }
    else
    {
        clientSockets[std::string(buffer)] = newClient;
        feedback = "Succeed";
        FD_SET(newClient, &readFDSet);
        print_connection_server(std::string(buffer));
        writeToClient(newClient,feedback);
    }
}

std::string whatsappServer::connectedClients(std::map <std::string, int> clientSockets)
{
    std::vector<std::string> clientNames;
    for (const auto &client : clientSockets)
    {
        clientNames.push_back(client.first);
    }
    std::sort(clientNames.begin(), clientNames.end(), std::less<std::string>());
    std::string connectedClientNames; //Will hold the names to display
    connectedClientNames = "";
    for (const std::string &clientName : clientNames)
        connectedClientNames += clientName + ",";
    connectedClientNames.pop_back(); //message to display
    return connectedClientNames;
}


void whatsappServer::removeDuplicateNames(std::string currentClient)
{
    clients.push_back(currentClient);
    std::sort(clients.begin(), clients.end());
    clients.erase(unique(clients.begin(), clients.end()), clients.end());
}


void whatsappServer::clientNewInput()
{
    std::map<std::string, int> tempClientSockets;
    tempClientSockets.insert(clientSockets.begin(), clientSockets.end());
    for (const auto &client :tempClientSockets)
    {
        if (client.second == 0) continue;
        clientFD = client.second; //current client FD
        std::string tempClientName = client.first;
        if (FD_ISSET(clientFD, &readFDSet))
        {
            //reads the incoming message
            //0 means eof we assume this is not the case and the input is valid
            if (read(clientFD, buffer, WA_MAX_INPUT) > 0)
            {
                parse_command(std::string(buffer), commandT, name, message, clients);
                std::string messageToSend;
                feedback = "Failed";
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

                        if (groups.size() < WA_MAX_GROUP) //we can add another group
                        {
                            for (const auto &clientName :clientSockets)
                            {
                                if (clientName.first == name) //there is a client with this name
                                {
                                    print_create_group(true, false, tempClientName, name);
                                    writeToClient(clientFD,feedback);
                                    return;
                                }
                            }
                            for (const auto &tempGroup : groups)
                            {
                                if (tempGroup.first == name) //there  is a group with this name
                                {
                                    print_create_group(true, false, tempClientName, name);
                                    writeToClient(clientFD,feedback);
                                    return;
                                }
                            }
                            removeDuplicateNames(tempClientName);
                            for (const auto &tempClient : clients)
                            {
                                if (!clientSockets.count(tempClient)) //non existing client
                                {
                                    print_create_group(true, false, tempClientName, name);
                                    writeToClient(clientFD,feedback);
                                    return;                                }
                            }
                            //Every thing is legit if we got here Adds clients to groups
                            //feedback defined above has success
                            groups[name] = clients;
                            feedback = "Succeed";
                            print_create_group(true, true, tempClientName, name);
                            writeToClient(clientFD,feedback);
                            return;
                        }
                        else
                        {
                            print_create_group(true, false, tempClientName, name);
                            writeToClient(clientFD,feedback);
                        }
                        return;
                    case SEND:
                        /*
                            If name is a client name it sends <sender_client_name>: <message> only to the
                            specified client. If name is a group name it sends <sender_client_name>:
                            <message> to all group members (except the sender client).
                        */
                        messageToSend.append(tempClientName).append(": ").append(message);
                        if (clientSockets.count(name)) //the target user exists
                        {
                            feedback = "Succeed";
                            print_send(true, true, tempClientName, name, message);
                            writeToClient(clientFD,feedback);
                            writeToClient(clientSockets[name],messageToSend);
                            return;
                        }
                        else //Group Case
                        {
                            if (groups.count(name)) //Such group exists
                            {
                                for (const auto &member : groups[name])
                                {
                                    if (tempClientName == member) //Looking if indeed
                                        // sender is part of the group
                                    {
                                        feedback = "Succeed";
                                        break;
                                    }
                                }
                                if (feedback == "Succeed")
                                {
                                    for (const auto &member : groups[name])
                                    {
                                        if (member != tempClientName) //Member other than
                                            // the sender
                                        {
                                            writeToClient(clientSockets[member],messageToSend);
                                        }
                                    }
                                    writeToClient(clientFD,feedback); //inform the sender success
                                    print_send(true, true, tempClientName, name, message);
                                    return;
                                }
                                else //sender is not part of the group
                                {
                                    print_send(true, false, tempClientName, name, message);
                                    writeToClient(clientFD,feedback);
                                    return;
                                }
                            }
                            else //No group or client with this name
                            {
                                print_send(true, false, tempClientName, name, message);
                                writeToClient(clientFD,feedback);
                                return;
                            }
                        }
                    case WHO:
                        /*
                            Sends a request (to the server) to receive a list (might be empty) of
                            currently connected client names (alphabetically order), separated by comma
                            without spaces.
                        */
                        print_who_server(tempClientName);
                        feedback = connectedClients(clientSockets);
                        writeToClient(clientFD,feedback);
                        return;
                    case EXIT:
                        /*
                            Unregisters the client from the server and removes it from all
                            groups. After the server unregistered the client, the client
                            should print “Unregistered successfully” and then exit(0).
                        */
                        clientSockets.erase(tempClientName); //removes from client list
                        for(auto &group : groups)
                        {
                            for(unsigned  int index = 0;index <group.second.size(); index++)
                            {
                                if(group.second.at(index) == tempClientName)
                                {
                                    group.second.erase(group.second.begin() + index);
                                    break;
                                }
                            }
                        }
                        feedback = "Succeed";
                        print_exit(true, tempClientName);
                        writeToClient(clientFD,feedback);
                        return;
                    case INVALID:
                        return;
                }
            }
            else
            {
                print_error("read", errno);
                exit(1);
            }
        }
    }
}

void whatsappServer::run()
{
    while (true)
    {
        setFileDescriptors();
        int maxFD = fdSetClients();

        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        if (select(maxFD +1, &readFDSet, nullptr, nullptr, nullptr) < 0)
        {
            print_error("select", errno);
            exit(1);
        }
        //Reads input from the server
        if (FD_ISSET(STDIN_FILENO, &readFDSet))
        {
            serverInput();
        }
        //If something happened on the mainSocket, it means an incoming connection(new client)
        if (FD_ISSET(mainSocket, &readFDSet))
        {
            newIncomingClient();
        }
        //else its some IO operation from the client side :
        else
        {
            clientNewInput();
        }

    }
}







