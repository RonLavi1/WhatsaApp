��# WhatsaApp

The server receives a port number and listens to this port. It waits for clients to connect to this
port (using TCP connection) and serves their requests (more details below).
When the server receives a request it needs to parse it and executes the request.
At any time the user can enter EXIT in the stdin (keyboard) of the server

Client:
Supports multiple commands such as: create_group,send,check who is currently connected,exit.

All the connections between the client and server are TCP based.
The client should check user input before sending it to the server (i.e. valid commands, expected
and valid arguments, etc.). The client should send only valid requests.
The received serverAddress is an IP address (and not a DNS address)
