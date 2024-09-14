#include <iostream>
#include <fstream>
#include <cstring>
#include <winsock2.h>  // Windows sockets library
#include <ws2tcpip.h>  // For InetPton function
#include "checksum.h" // includes the checksum function for error dection

#pragma comment(lib, "ws2_32.lib")  // Link with Winsock library

#define PORT 8080
#define BUFFER_SIZE 1024

void send_file(SOCKET socket_fd, const char* filename);
void receive_file(SOCKET socket_fd, const char* filename);

int main(int argc, char* argv[]) {
    if (argc < 3) {
        //When running client code have to ./client put/get filename
        std::cerr << "Usage: " << argv[0] << " <put|get> <filename>" << std::endl;
        return 1;
    }

    const char* command = argv[1];
    const char* filename = argv[2];

    //Debugging argument
    std::cout << "Command: " << command << ", Filename: " << filename << std::endl;

    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server_addr;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "Failed to initialize Winsock. Error Code: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        std::cerr << "Could not create socket. Error Code: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);


// Set server address to localhost (127.0.0.1) Will need to change the IP since it is only unique to my computer

    // Debugging code
     
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address / Address not supported" << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Debug output for connection attempt
    std::cout << "Connecting to the server..." << std::endl;


    // Connect to the server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed. Error Code: " << WSAGetLastError() << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to the server!" << std::endl;



    //replaced int bytes code (11:00)
    std::string full_command = std::string(command) + " " + filename + "\n";
    int bytes_sent = send(sock, full_command.c_str(), full_command.length(), 0);
    if (bytes_sent == SOCKET_ERROR) {
        std::cerr << "Failed to send command. Error Code: " << WSAGetLastError() << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    if (strcmp(command, "put") == 0) {
        // Send file to the server
        send_file(sock, filename);
    } else if (strcmp(command, "get") == 0) {
        // Receive file from the server
        receive_file(sock, filename);
    } else {
        std::cerr << "Invalid command. Use 'put' or 'get'." << std::endl;
    }

    // Close socket
    closesocket(sock);
    WSACleanup();
    return 0;
}

// Function to send a file to the server
void send_file(SOCKET socket_fd, const char* filename) {
    
    // Calculate checksum
    unsigned int checksum = compute_checksum(filename,BUFFER_SIZE);
    std::cout << "Checksum calculated for file '" << filename << "': " << checksum << std::endl;

    // Send checksum to the server
    send(socket_fd, (char*)&checksum, sizeof(checksum), 0);

    //Proceed to send the file
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "File not found: " << filename << std::endl;
        return;
    }

    char buffer[BUFFER_SIZE] = {0};
    while (file.read(buffer, BUFFER_SIZE)) {
        send(socket_fd, buffer, file.gcount(), 0);
    }
    // Send any remaining bytes
    if (file.gcount() > 0) {
        send(socket_fd, buffer, file.gcount(), 0);
    }
    
    file.close();
    std::cout << "File '" << filename << "' has been sent successfully." << std::endl;
}

// Function to receive a file from the server
void receive_file(SOCKET socket_fd, const char* filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to create file: " << filename << std::endl;
        return;
    }

    char buffer[BUFFER_SIZE] = {0};
    int bytes_received;
    while ((bytes_received = recv(socket_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        file.write(buffer, bytes_received);
    }

    file.close();
    std::cout << "File '" << filename << "' has been received and saved." << std::endl;
}
