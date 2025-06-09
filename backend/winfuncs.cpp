#ifdef _WIN32

#define _WIN32_WINNT 0x0600  // Windows Vista oder höher (für QueryFullProcessImageNameW)
#define NOMINMAX
#include <windows.h>
#include <psapi.h>              
#include <iostream>
#include <cstdio>
#include <array>
#include <fstream>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <string>
#include <comdef.h> // Für _com_error
#include <vector>
#include <algorithm>
#include "nlohmann/json.hpp"
using json = nlohmann::json;



const PROPERTYKEY PKEY_Device_FriendlyName = {
    {0xA45C254E, 0xDF1C, 0x4EFD, {0x80,0x20,0x67,0xD1,0x46,0xA8,0x50,0xE0}},
    14
};


#pragma comment(lib, "Ole32.lib")



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
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hSerial == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening serial port " << portName << "\n";
        return INVALID_HANDLE_VALUE;
    }

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error getting state\n";
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    // Flow Control aus
    dcbSerialParams.fOutxCtsFlow = FALSE;
    dcbSerialParams.fRtsControl = RTS_CONTROL_DISABLE;
    dcbSerialParams.fOutX = FALSE;
    dcbSerialParams.fInX = FALSE;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error setting serial port state\n";
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hSerial, &timeouts)) {
        std::cerr << "Error setting timeouts\n";
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    // Steuerleitungen setzen
    EscapeCommFunction(hSerial, SETDTR);
    EscapeCommFunction(hSerial, SETRTS);

    // Optional: initialen "Handshake"-Byte senden
    // char initByte = 0x00;
    // DWORD bytesWritten;
    // WriteFile(hSerial, &initByte, 1, &bytesWritten, NULL);

    Sleep(100); // kurz warten

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


bool changevolume(std::string nappName, std::string  value, const std::string& mode = "") {
    std::transform(nappName.begin(), nappName.end(),nappName.begin(),::towlower);
    const std::wstring appName=stringToWString(nappName);
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

// Hilfsfunktion für char* -> wchar_t*

// Konvertierung von std::string (UTF-8) zu std::wstring (UTF-16)
std::wstring StringToWString(const std::string& s) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

//--- Soundboard
bool soundboard(const std::string& path, const std::string& vol, const std::string& id) {
    std::string cmd = "playsound.bat \"" + path + "\" \"" + vol + "\" \"" + id+"\"";
    std::cout<<"IDU: "<<cmd<<std::endl;

    // Umwandlung std::string -> std::wstring
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), (int)cmd.size(), NULL, 0);
    std::wstring wcmd(size_needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), (int)cmd.size(), &wcmd[0], size_needed);

    // CreateProcessW braucht einen nicht-konstanten wchar_t-Puffer
    // Deshalb std::vector<wchar_t> mit null-terminator
    std::vector<wchar_t> cmdBuffer(wcmd.begin(), wcmd.end());
    cmdBuffer.push_back(L'\0'); // Nullterminator anhängen

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (!CreateProcessW(NULL, cmdBuffer.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        DWORD err = GetLastError();
        wprintf(L"CreateProcessW fehlgeschlagen mit Fehler %lu\n", err);
        return false;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}

std::string getid() {
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        std::wcerr << L"CoInitialize failed: " << std::hex << hr << std::endl;
        return "1";
    }

    IMMDeviceEnumerator* pEnumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                          IID_PPV_ARGS(&pEnumerator));
    if (FAILED(hr)) {
        std::wcerr << L"Failed to create device enumerator: " << std::hex << hr << std::endl;
        CoUninitialize();
        return "1";
    }

    IMMDeviceCollection* pCollection = nullptr;
    hr = pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        std::wcerr << L"Failed to enumerate capture devices: " << std::hex << hr << std::endl;
        pEnumerator->Release();
        CoUninitialize();
        return "1";
    }

    UINT count = 0;
    hr = pCollection->GetCount(&count);
    if (FAILED(hr)) {
        std::wcerr << L"Failed to get device count: " << std::hex << hr << std::endl;
        pCollection->Release();
        pEnumerator->Release();
        CoUninitialize();
        return "1";
    }

    std::wstring targetName = L"CABLE Output (VB-Audio Virtual Cable)"; // z.B. "CABLE Output (VB-Audio Virtual Cable)"
    std::wstring foundDeviceId;

    for (UINT i = 0; i < count; i++) {
        IMMDevice* pDevice = nullptr;
        hr = pCollection->Item(i, &pDevice);
        if (FAILED(hr)) {
            continue;
        }

        IPropertyStore* pProps = nullptr;
        hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
        if (FAILED(hr)) {
            pDevice->Release();
            continue;
        }

        PROPVARIANT varName;
        PropVariantInit(&varName);

        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        if (SUCCEEDED(hr) && varName.vt == VT_LPWSTR) {
            std::wcout << L"Device found: " << varName.pwszVal << std::endl;
            if (targetName == varName.pwszVal) {
                // ID holen
                LPWSTR id = nullptr;
                hr = pDevice->GetId(&id);
                if (SUCCEEDED(hr)) {
                    foundDeviceId = id;
                    CoTaskMemFree(id);
                }
            }
        }

        PropVariantClear(&varName);
        pProps->Release();
        pDevice->Release();

        if (!foundDeviceId.empty()) break;
    }

    pCollection->Release();
    pEnumerator->Release();
    CoUninitialize();

    if (!foundDeviceId.empty()) {
        std::wcout << L"Gefundene Geräte-ID: " << foundDeviceId << std::endl;
        size_t len = wcstombs(nullptr, foundDeviceId.c_str(), 0) + 1;

        // Creating a buffer to hold the multibyte string
        char* buffer = new char[len];

        // Converting wstring to string
        wcstombs(buffer, foundDeviceId.c_str(), len);

        // Creating std::string from char buffer
        std::string str(buffer);

        // Cleaning up the buffer
        delete[] buffer;
        return str;
    } else {
        std::wcerr << L"Gerät nicht gefunden." << std::endl;
    }

    return 0;
}
//---
int executefunction(int page, int butt){
    std::ifstream file("../frontend/config.json");
    json jsn;
    file >> jsn;
    json button = jsn["pages"][page][butt-1];

    if(button.value("type", "").compare("changevolume")==0){
        changevolume(((button["data"].value("target", "")+".exe")), button["data"].value("step", ""), button["data"].value("direction", ""));
        return 0;
    }
    soundboard("sounds/"+(button["data"].value("file", "")),(button["data"].value("volume", "")),getid());
    return 1;
}
#endif