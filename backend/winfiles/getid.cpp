
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

#define DLLEXPORT extern "C" __declspec(dllexport)

const PROPERTYKEY PKEY_Device_FriendlyName = {
    {0xA45C254E, 0xDF1C, 0x4EFD, {0x80,0x20,0x67,0xD1,0x46,0xA8,0x50,0xE0}},
    14
};


std::string getid(std::string device) {
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

    std::wstring targetName(device.begin(), device.end()); // z.B. "CABLE Output (VB-Audio Virtual Cable)"
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
            //std::wcout << L"Device found: " << varName.pwszVal << std::endl;
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
       //std::wcout << L"Gefundene Geräte-ID: " << foundDeviceId << std::endl;
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

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: getid.exe \"Device Friendly Name\"" << std::endl;
        return 1;
    }
    std::string deviceName = argv[1];
    std::string id = getid(deviceName);

    if (id.empty()) {
        std::cerr << "Device not found or error occurred." << std::endl;
        return 1;
    }

    std::cout << id << std::endl; // ID als Ausgabe
    return 0;
}