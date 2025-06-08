
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
#include <initguid.h>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <string>
#include <comdef.h> // Für _com_error
#include <vector>
#include <algorithm>
#include "nlohmann/json.hpp"
using json = nlohmann::json;


#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <avrt.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <comdef.h>
#include <codecvt>

#pragma comment(lib, "Ole32.lib")

// WAV-Header Struktur (minimal)
struct WavHeader {
    char riff[4];
    uint32_t chunkSize;
    char wave[4];
    char fmt[4];
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char data[4];
    uint32_t dataSize;
};

// Funktion: WAV-Datei laden und Header prüfen
bool LoadWavFile(const std::string& filename, WavHeader& header, std::vector<BYTE>& audioData) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Kann WAV-Datei nicht öffnen.\n";
        return false;
    }

    file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));
    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        std::cerr << "Keine gültige WAV-Datei.\n";
        return false;
    }
    if (header.audioFormat != 1 && header.audioFormat != 3) { // PCM
        std::cerr << "Nur PCM WAV-Dateien unterstützt.\n";
        return false;
    }

    audioData.resize(header.dataSize);
    file.read(reinterpret_cast<char*>(audioData.data()), header.dataSize);

    return true;
}

// Funktion: Gerät per Name suchen
HRESULT GetDeviceByName(const std::wstring& deviceName, IMMDevice** ppDevice) {
    HRESULT hr;
    IMMDeviceEnumerator* pEnumerator = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) return hr;

    IMMDeviceCollection* pCollection = nullptr;
    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        pEnumerator->Release();
        return hr;
    }

    UINT count;
    pCollection->GetCount(&count);
    std::cout<<count<<std::endl;
    for (UINT i = 0; i < count; i++) {
        IMMDevice* pDevice = nullptr;
        pCollection->Item(i, &pDevice);

        LPWSTR pName = nullptr;
        IPropertyStore* pProps = nullptr;

        pDevice->OpenPropertyStore(STGM_READ, &pProps);
        PROPVARIANT varName;
        PropVariantInit(&varName);

        pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        if (varName.vt == VT_LPWSTR) {
            if (deviceName == varName.pwszVal) {
                *ppDevice = pDevice;
                (*ppDevice)->AddRef();
                PropVariantClear(&varName);
                pProps->Release();
                pDevice->Release();
                pCollection->Release();
                pEnumerator->Release();
                return S_OK;
            }
        }
        PropVariantClear(&varName);
        pProps->Release();
        pDevice->Release();
    }

    pCollection->Release();
    pEnumerator->Release();
    
    return E_FAIL;
}

// Funktion: WAV auf bestimmtes Gerät abspielen
HRESULT PlayWavOnDevice(const std::string& wavFile, const std::wstring& deviceName) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) return hr;

    IMMDevice* pDevice = nullptr;
    hr = GetDeviceByName(deviceName, &pDevice);

    if (FAILED(hr)) {
        std::cerr << "Gerät nicht gefunden: "<< "\n";
        CoUninitialize();
        return hr;
    }

    IAudioClient* pAudioClient = nullptr;
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hr)) {
        pDevice->Release();
        CoUninitialize();
        return hr;
    }

    WAVEFORMATEX* pwfx = nullptr;
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        pAudioClient->Release();
        pDevice->Release();
        CoUninitialize();
        return hr;
    }

    // WAV-Datei laden
    WavHeader header;
    std::vector<BYTE> audioData;
    if (!LoadWavFile(wavFile, header, audioData)) {
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        CoUninitialize();
        return E_FAIL;
    }
    
    // Prüfen, ob WAV-Format passt
    if ((pwfx->wFormatTag != WAVE_FORMAT_PCM && pwfx->wFormatTag != WAVE_FORMAT_EXTENSIBLE) ||
        pwfx->nChannels != header.numChannels ||
        pwfx->nSamplesPerSec != header.sampleRate ||
        pwfx->wBitsPerSample != header.bitsPerSample) {

            std::wcout << L"Geräte-Format:\n";
            std::wcout << L"  FormatTag: " << pwfx->wFormatTag << L"\n";
            std::wcout << L"  Kanäle: " << pwfx->nChannels << L"\n";
            std::wcout << L"  SampleRate: " << pwfx->nSamplesPerSec << L"\n";
            std::wcout << L"  BitsPerSample: " << pwfx->wBitsPerSample << L"\n";

            std::wcout << L"Geräte-Format benötigt:\n";
            std::wcout << L"  FormatTag: " << WAVE_FORMAT_PCM << L"\n";
            std::wcout << L"  Kanäle: " << header.numChannels << L"\n";
            std::wcout << L"  SampleRate: " << header.sampleRate << L"\n";
            std::wcout << L"  BitsPerSample: " << header.bitsPerSample << L"\n";


        std::cerr << "WAV-Format passt nicht zum Gerät.\n";
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        CoUninitialize();
        return E_FAIL;
    }

    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 0, 0, pwfx, NULL);
    if (FAILED(hr)) {
        std::cerr << "IAudioClient Init fehlgeschlagen\n";
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        CoUninitialize();
        return hr;
    }

    IAudioRenderClient* pRenderClient = nullptr;
    hr = pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&pRenderClient);
    if (FAILED(hr)) {
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        CoUninitialize();
        return hr;
    }

    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        pRenderClient->Release();
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        CoUninitialize();
        return hr;
    }

    UINT32 bufferFrameCount;
    hr = pAudioClient->GetBufferSize(&bufferFrameCount);

    UINT32 framesPlayed = 0;
    const BYTE* audioPtr = audioData.data();
    UINT32 totalFrames = header.dataSize / (header.numChannels * (header.bitsPerSample / 8));

    while (framesPlayed < totalFrames) {
        UINT32 padding = 0;
        pAudioClient->GetCurrentPadding(&padding);
        UINT32 framesAvailable = bufferFrameCount - padding;

        UINT32 framesToWrite = std::min(framesAvailable, totalFrames - framesPlayed);

        BYTE* pData = nullptr;
        hr = pRenderClient->GetBuffer(framesToWrite, &pData);
        if (FAILED(hr)) break;

        memcpy(pData, audioPtr + framesPlayed * header.numChannels * (header.bitsPerSample / 8),
               framesToWrite * header.numChannels * (header.bitsPerSample / 8));

        hr = pRenderClient->ReleaseBuffer(framesToWrite, 0);
        if (FAILED(hr)) break;

        framesPlayed += framesToWrite;
        Sleep(10);
    }
    Sleep(0 + 1000);
    pAudioClient->Stop();
    pRenderClient->Release();
    CoTaskMemFree(pwfx);
    pAudioClient->Release();
    pDevice->Release();
    CoUninitialize();

    std::cout << "Playback fertig.\n";
    return S_OK;
}   

std::wstring utf8_to_wstring(const char* utf8str)
{
    if (!utf8str) return L"";

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8str, -1, nullptr, 0);
    if (size_needed == 0) return L"";

    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8str, -1, &wstr[0], size_needed);
    // string enthält jetzt ein Nullterminator am Ende, den man ggf. entfernen kann
    wstr.resize(size_needed - 1); // Nullterminator entfernen
    return wstr;
}


int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: wavplay.exe <WAV-Datei> <Zielgerät-Name>\n";
        return -1;
    }

    std::string wavFile = argv[1];
    std::wstring deviceName = utf8_to_wstring(argv[2]);
    HRESULT hr = PlayWavOnDevice(wavFile, deviceName);
    if (FAILED(hr)) {
        std::cerr << "Fehler bei der Wiedergabe\n";
        return -1;
    }

    return 0;
}