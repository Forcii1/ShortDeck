#include <windows.h>
#include <iostream>
#include <cstdio>
#include <array>


std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

HANDLE init_read(const char* portName = "COM5") {
    HANDLE hSerial = CreateFileA(
        portName,
        GENERIC_READ | GENERIC_WRITE,
        0,                // Kein Sharing
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hSerial == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening serial port " << portName << "\n";
        return INVALID_HANDLE_VALUE;
    }

    // Einstellungen für die serielle Schnittstelle
    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error getting state\n";
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    dcbSerialParams.BaudRate = CBR_115200;  // Baudrate 115200
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error setting serial port state\n";
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    // Zeitüberschreitungen setzen
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hSerial, &timeouts)) {
        std::cerr << "Error setting timeouts\n";
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    return hSerial;
}

int readport(HANDLE hSerial) {
    if (hSerial == INVALID_HANDLE_VALUE) return 0;

    char buffer[256] = {0};
    DWORD bytesRead = 0;

    BOOL success = ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
    if (!success) {
        std::cerr << "Error reading from serial port\n";
        return 0;
    }

    if (bytesRead == 0) {
           std::cout<<"shit"<<buffer;
        // Timeout oder keine Daten — einfach 0 zurückgeben
        return 0;
    }

    buffer[bytesRead] = '\0';
    // Optional: Ausgabe zur Kontrolle
    // std::cout << "Received: " << buffer << std::endl;
    return atoi(buffer);
}
