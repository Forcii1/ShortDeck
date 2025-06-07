#define _WIN32_WINNT 0x0600  // Windows Vista oder höher (für QueryFullProcessImageNameW)
#define NOMINMAX
#include <windows.h>
#include <psapi.h>              
#include <iostream>
#include <cstdio>
#include <array>

#include <fstream>

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <string>
#include <comdef.h> // Für _com_error
#include <vector>

#include <algorithm>
#include "nlohmann/json.hpp"
using json = nlohmann::json;



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
           //std::cout<<"shit"<<buffer;
        // Timeout oder keine Daten — einfach 0 zurückgeben
        return 0;
    }

    buffer[bytesRead] = '\0';
    // Optional: Ausgabe zur Kontrolle
    // std::cout << "Received: " << buffer << std::endl;
    return atoi(buffer);
}

std::wstring stringToWString(const std::string& str) {
    if (str.empty()) return L"";

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);

    return wstrTo;
}


bool changevolume(const std::wstring& appName, std::string  value, const std::string& mode = "") {
    std::transform(appName.begin(), appName.end(),appName.begin(),::towlower);
    bool comInitialized = false;
    if (SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
    comInitialized = true;
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) || !comInitialized) {
        std::cerr << "COM initialization failed\n";
        return false;
    }

    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&deviceEnumerator));
    if (FAILED(hr)) {
        CoUninitialize();
        return false;
    }

    IMMDevice* defaultDevice = nullptr;
    hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultDevice);
    if (FAILED(hr)) {
        deviceEnumerator->Release();
        CoUninitialize();
        return false;
    }

    IAudioSessionManager2* sessionManager = nullptr;
    hr = defaultDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&sessionManager);
    if (FAILED(hr)) {
        defaultDevice->Release();
        deviceEnumerator->Release();
        CoUninitialize();
        return false;
    }

    IAudioSessionEnumerator* sessionEnumerator = nullptr;
    hr = sessionManager->GetSessionEnumerator(&sessionEnumerator);
    if (FAILED(hr)) {
        sessionManager->Release();
        defaultDevice->Release();
        deviceEnumerator->Release();
        CoUninitialize();
        return false;
    }

    int count = 0;
    sessionEnumerator->GetCount(&count);

    for (int i = 0; i < count; i++) {
        IAudioSessionControl* sessionControl = nullptr;
        hr = sessionEnumerator->GetSession(i, &sessionControl);
        if (FAILED(hr) || !sessionControl) continue;

        IAudioSessionControl2* sessionControl2 = nullptr;
        hr = sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sessionControl2);
        sessionControl->Release();
        if (FAILED(hr) || !sessionControl2) continue;

        DWORD pid = 0;
        sessionControl2->GetProcessId(&pid);
        if (pid == 0) { // System Sounds etc.
            sessionControl2->Release();
            continue;
        }

        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (hProc) {
            wchar_t exeName[MAX_PATH] = {0};
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, exeName, &size)) {
                std::wstring fullExeName(exeName);
                size_t pos = fullExeName.find_last_of(L"\\/");
                std::wstring shortName = (pos == std::wstring::npos) ? fullExeName : fullExeName.substr(pos + 1);

                // Vergleiche Case-insensitive
                std::wstring wAppNameLower = appName;
                std::transform(wAppNameLower.begin(), wAppNameLower.end(), wAppNameLower.begin(), ::towlower);
                std::wstring shortNameLower = shortName;
                std::transform(shortNameLower.begin(), shortNameLower.end(), shortNameLower.begin(), ::towlower);

                if (shortNameLower.find(wAppNameLower) != std::wstring::npos) {
                    ISimpleAudioVolume* audioVolume = nullptr;
                    hr = sessionControl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&audioVolume);
                    if (SUCCEEDED(hr) && audioVolume) {
                        float currentVolume = 0.0f;
                        audioVolume->GetMasterVolume(&currentVolume);
                        if (mode == "+")
                            currentVolume += stof(value) / 100.0f;
                        else if (mode == "-")
                            currentVolume -= stof(value) / 100.0f;
                        else
                            currentVolume = stof(value) / 100.0f;

                        currentVolume = std::max(0.0f, std::min(1.0f, currentVolume));
                        audioVolume->SetMasterVolume(currentVolume, NULL);
                        audioVolume->Release();
                    }
                }
            }
            CloseHandle(hProc);
        }

        sessionControl2->Release();
    }

    sessionEnumerator->Release();
    sessionManager->Release();
    defaultDevice->Release();
    deviceEnumerator->Release();
    CoUninitialize();

    return true;
}

int executefunction(int page, int butt){
    std::ifstream file("../frontend/config.json");
    json jsn;
    file >> jsn;
    json button = jsn["pages"][page][butt-1];

    if(button.value("type", "").compare("changevolume")==0){
        changevolume(stringToWString((button["data"].value("target", "")+".exe")), button["data"].value("step", ""), button["data"].value("direction", ""));
        return 0;
    }

}
