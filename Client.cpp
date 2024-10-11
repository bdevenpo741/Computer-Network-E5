#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fstream>
#include <thread>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_IP "127.0.0.1" // Change this to your server's IP address if necessary
#define SERVER_PORT 8080
#define UDP_PORT 8081
#define BUFFER_SIZE 1024

void sendCommand(SOCKET clientSocket, const std::string &command);
void receiveMessages(SOCKET clientSocket);
void sendFile(SOCKET clientSocket, const std::string &filename);
void receiveFile(SOCKET clientSocket);

int main()
{
    WSADATA wsaData;
    SOCKET clientSocket;
    struct sockaddr_in serverAddr;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    // Create TCP socket
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    // Set up the server address structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    // Connect to the server
    if (connect(clientSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Connection to server failed.\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to the server.\n";

    // Start a thread to receive messages from the server
    std::thread receiveThread(receiveMessages, clientSocket);

    char buffer[BUFFER_SIZE];
    while (true)
    {
        std::cout << "Enter command (put <filename> / get <filename> / exit): ";
        std::cin.getline(buffer, BUFFER_SIZE);
        std::string command(buffer);

        if (command.substr(0, 3) == "put")
        {
            // Send the 'put' command to server
            sendCommand(clientSocket, command);
            sendFile(clientSocket, command.substr(4)); // Pass filename after 'put '
        }
        else if (command.substr(0, 3) == "get")
        {
            // Send the 'get' command to server
            sendCommand(clientSocket, command);
        }
        else if (command == "exit")
        {
            sendCommand(clientSocket, command);
            break; // Exit the loop if the user wants to disconnect
        }
        else
        {
            // Send a regular message to the server
            sendCommand(clientSocket, command);
        }
    }

    receiveThread.join();
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}

void sendCommand(SOCKET clientSocket, const std::string &command)
{
    send(clientSocket, command.c_str(), command.size(), 0);
}

void receiveMessages(SOCKET clientSocket)
{
    char buffer[BUFFER_SIZE];
    int bytesReceived;

    while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0)
    {
        buffer[bytesReceived] = '\0'; // Null-terminate the received string
        std::cout << "Server: " << buffer << std::endl;
    }

    if (bytesReceived == 0)
    {
        std::cout << "Server disconnected.\n";
    }
    else
    {
        std::cerr << "Error receiving message from server.\n";
    }
}

void sendFile(SOCKET clientSocket, const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Could not open file: " << filename << "\n";
        return;
    }

    // Read the file and send its content
    char buffer[BUFFER_SIZE];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
    {
        send(clientSocket, buffer, file.gcount(), 0);
    }

    file.close();
    std::cout << "File " << filename << " sent to the server.\n";
}

void receiveFile(SOCKET clientSocket)
{
    char buffer[BUFFER_SIZE];
    int bytesReceived;

    std::ofstream file("received_file", std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Could not create file to save received data.\n";
        return;
    }

    while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0)
    {
        file.write(buffer, bytesReceived);
    }

    file.close();
    std::cout << "File received and saved.\n";
}
