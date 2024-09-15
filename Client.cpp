#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fstream>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

int64_t SendFile(SOCKET s, const std::string& fileName, int chunkSize);
void sendFile(SOCKET& clientSocket, const std::string& filename);
void receiveFile(SOCKET& clientSocket, const std::string& filename);

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    SOCKET clientSocket;
    struct sockaddr_in serverAddr;
    char buffer[BUFFER_SIZE];

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    // Create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    // Server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    // Connect to server
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection to server failed.\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server.\n";
    
    while (true) {
        std::cout << "Enter command (put <filename> / get <filename> / exit): ";
        std::cin.getline(buffer, BUFFER_SIZE);
        std::string command(buffer);

        // Send command to the server
        send(clientSocket, buffer, command.size(), 0);

        if (command == "exit") {
            break;
        }

        // Parse the command to decide on file transfer action
        std::string action = command.substr(0, command.find(' '));
        std::string filename = command.substr(command.find(' ') + 1);

        if (action == "put") {
            SendFile(clientSocket, filename,BUFFER_SIZE);

            // Wait for confirmation from the server
            int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                std::cout << "Server response: " << buffer << "\n";
            }
        } else if (action == "get") {
            receiveFile(clientSocket, filename);
        } else {
            std::cout << "Invalid command.\n";
        }
    }

    // Cleanup
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}

/*void sendFile(SOCKET& clientSocket, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filename << "\n";
        return;
    }

    char buffer[BUFFER_SIZE];
    bool dataSent = false;

    while (!file.eof()) {
        file.read(buffer, BUFFER_SIZE);
        int bytesRead = file.gcount();
        unsigned int bytesSent = 0;
        int bytesToSend = 0;

        while (bytesSent < BUFFER_SIZE)
        {
            bytesToSend = BUFFER_SIZE - bytesSent;
            send(clientSocket, buffer + bytesSent, bytesToSend, 0);
            bytesSent += bytesToSend;
        }
        dataSent = true;
    }

    file.close();

    if (dataSent) {
        std::cout << "File " << filename << " sent successfully.\n";
    } else {
        std::cerr << "No data was sent for file: " << filename << "\n";
    }
}*/

void receiveFile(SOCKET& clientSocket, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not create file: " << filename << "\n";
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytesReceived;
    while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0) {
        file.write(buffer, bytesReceived);
    }

    file.close();
    std::cout << "File " << filename << " received successfully.\n";
}




int64_t GetFileSize(const std::string& fileName) {
    // no idea how to get filesizes > 2.1 GB in a C++ kind-of way.
    // I will cheat and use Microsoft's C-style file API
    FILE* f;
    if (fopen_s(&f, fileName.c_str(), "rb") != 0) {
        return -1;
    }
    _fseeki64(f, 0, SEEK_END);
    const int64_t len = _ftelli64(f);
    fclose(f);
    return len;
}

///
/// Recieves data in to buffer until bufferSize value is met
///
int RecvBuffer(SOCKET s, char* buffer, int bufferSize, int chunkSize = 4 * 1024) {
    int i = 0;
    while (i < bufferSize) {
        const int l = recv(s, &buffer[i], __min(chunkSize, bufferSize - i), 0);
        if (l < 0) { return l; } // this is an error
        i += l;
    }
    return i;
}

///
/// Sends data in buffer until bufferSize value is met
///
int SendBuffer(SOCKET s, const char* buffer, int bufferSize, int chunkSize = 4 * 1024) {

    int i = 0;
    while (i < bufferSize) {
        const int l = send(s, &buffer[i], __min(chunkSize, bufferSize - i), 0);
        if (l < 0) { return l; } // this is an error
        i += l;
    }
    return i;
}

//
// Sends a file
// returns size of file if success
// returns -1 if file couldn't be opened for input
// returns -2 if couldn't send file length properly
// returns -3 if file couldn't be sent properly
//
int64_t SendFile(SOCKET s, const std::string& fileName, int chunkSize = 64 * 1024) {

    const int64_t fileSize = GetFileSize(fileName);
    if (fileSize < 0) { return -1; }

    std::ifstream file(fileName, std::ifstream::binary);
    if (file.fail()) { return -1; }

    if (SendBuffer(s, reinterpret_cast<const char*>(&fileSize),
        sizeof(fileSize)) != sizeof(fileSize)) {
        return -2;
    }

    char* buffer = new char[chunkSize];
    bool errored = false;
    int64_t i = fileSize;
    while (i != 0) {
        const int64_t ssize = __min(i, (int64_t)chunkSize);
        if (!file.read(buffer, ssize)) { errored = true; break; }
        const int l = SendBuffer(s, buffer, (int)ssize);
        if (l < 0) { errored = true; break; }
        i -= l;
    }
    delete[] buffer;

    file.close();

    return errored ? -3 : fileSize;
}

//
// Receives a file
// returns size of file if success
// returns -1 if file couldn't be opened for output
// returns -2 if couldn't receive file length properly
// returns -3 if couldn't receive file properly
//
int64_t RecvFile(SOCKET s, const std::string& fileName, int chunkSize = 64 * 1024) {
    std::ofstream file(fileName, std::ofstream::binary);
    if (file.fail()) { return -1; }

    int64_t fileSize;
    if (RecvBuffer(s, reinterpret_cast<char*>(&fileSize),
        sizeof(fileSize)) != sizeof(fileSize)) {
        return -2;
    }

    char* buffer = new char[chunkSize];
    bool errored = false;
    int64_t i = fileSize;
    while (i != 0) {
        const int r = RecvBuffer(s, buffer, (int)__min(i, (int64_t)chunkSize));
        if ((r < 0) || !file.write(buffer, r)) { errored = true; break; }
        i -= r;
    }
    delete[] buffer;

    file.close();

    return errored ? -3 : fileSize;
}