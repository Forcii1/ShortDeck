
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
    uint16_t audioFormat;     // 1 = PCM, 3 = IEEE float
    uint16_t numChannels;
    uint32_t sampleRate;
    uint16_t bitsPerSample;
    uint32_t dataSize;
};
// Funktion: WAV-Datei laden und Header prüfen
bool LoadWavFile(const std::string& filename, WavHeader& outHeader, std::vector<BYTE>& outData) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "WAV-Datei konnte nicht geöffnet werden\n";
        return false;
    }

    char riff[4];
    file.read(riff, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0) {
        std::cerr << "Keine gültige RIFF-Datei\n";
        return false;
    }

    file.ignore(4); // ChunkSize

    char wave[4];
    file.read(wave, 4);
    if (std::strncmp(wave, "WAVE", 4) != 0) {
        std::cerr << "Keine gültige WAVE-Datei\n";
        return false;
    }

    bool foundFmt = false;
    bool foundData = false;
    WavHeader header = {};
    std::vector<BYTE> data;

    while (!file.eof()) {
        char chunkId[4];
        uint32_t chunkSize = 0;
        file.read(chunkId, 4);
        file.read(reinterpret_cast<char*>(&chunkSize), 4);

        if (file.gcount() < 4) break;

        if (std::strncmp(chunkId, "fmt ", 4) == 0) {
            if (chunkSize < 16) {
                std::cerr << "fmt-Chunk zu klein\n";
                return false;
            }

            uint16_t audioFormat;
            uint16_t numChannels;
            uint32_t sampleRate;
            uint32_t byteRate;
            uint16_t blockAlign;
            uint16_t bitsPerSample;

            file.read(reinterpret_cast<char*>(&audioFormat), sizeof(audioFormat));
            file.read(reinterpret_cast<char*>(&numChannels), sizeof(numChannels));
            file.read(reinterpret_cast<char*>(&sampleRate), sizeof(sampleRate));
            file.read(reinterpret_cast<char*>(&byteRate), sizeof(byteRate));
            file.read(reinterpret_cast<char*>(&blockAlign), sizeof(blockAlign));
            file.read(reinterpret_cast<char*>(&bitsPerSample), sizeof(bitsPerSample));

            // Falls Format mehr als 16 Byte lang ist, Rest überspringen
            if (chunkSize > 16)
                file.ignore(chunkSize - 16);

            header.audioFormat = audioFormat;
            header.numChannels = numChannels;
            header.sampleRate = sampleRate;
            header.bitsPerSample = bitsPerSample;

            foundFmt = true;
        }
        else if (std::strncmp(chunkId, "data", 4) == 0) {
            if (!foundFmt) {
                std::cerr << "Daten gefunden vor Formatdefinition\n";
                return false;
            }

            data.resize(chunkSize);
            file.read(reinterpret_cast<char*>(data.data()), chunkSize);
            header.dataSize = chunkSize;
            foundData = true;
        }
        else {
            // Unbekannter Chunk → überspringen
            file.ignore(chunkSize);
        }
    }

    if (!foundFmt || !foundData) {
        std::cerr << "fmt- oder data-Chunk fehlt\n";
        return false;
    }

    outHeader = header;
    outData = std::move(data);
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
        std::cerr << "Gerät nicht gefunden\n";
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

    // WAV laden
    WavHeader header;
    std::vector<BYTE> audioData;
    if (!LoadWavFile(wavFile, header, audioData)) {
        std::cerr << "WAV-Datei konnte nicht geladen werden\n";
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        CoUninitialize();
        return E_FAIL;
    }

    // Audioformat prüfen (nur einfache Prüfung, ggf. erweitern)
    if (pwfx->nChannels != header.numChannels ||
        pwfx->nSamplesPerSec != header.sampleRate ||
        pwfx->wBitsPerSample != header.bitsPerSample) {

        std::wcout << L"Geräteformat:\n";
        std::wcout << L"  Kanäle: " << pwfx->nChannels << L"\n";
        std::wcout << L"  SampleRate: " << pwfx->nSamplesPerSec << L"\n";
        std::wcout << L"  BitsPerSample: " << pwfx->wBitsPerSample << L"\n";

        std::wcout << L"WAV-Datei:\n";
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

    // Pufferdauer: 200 ms
    REFERENCE_TIME bufferDuration = 2000000; // 200 ms in 100-ns-Einheiten

    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        bufferDuration,
        0,
        pwfx,
        NULL
    );
    if (FAILED(hr)) {
        std::cerr << "AudioClient Initialisierung fehlgeschlagen\n";
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        CoUninitialize();
        return hr;
    }

    UINT32 bufferFrameCount;
    hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    if (FAILED(hr)) {
        std::cerr << "GetBufferSize fehlgeschlagen\n";
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        CoUninitialize();
        return hr;
    }

    IAudioRenderClient* pRenderClient = nullptr;
    hr = pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&pRenderClient);
    if (FAILED(hr)) {
        std::cerr << "RenderClient konnte nicht erstellt werden\n";
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        CoUninitialize();
        return hr;
    }

    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        std::cerr << "AudioClient konnte nicht gestartet werden\n";
        pRenderClient->Release();
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        CoUninitialize();
        return hr;
    }

    const UINT32 frameSize = header.numChannels * (header.bitsPerSample / 8);
    const UINT32 totalFrames = header.dataSize / frameSize;

    UINT32 framesPlayed = 0;
    const BYTE* audioPtr = audioData.data();

    while (framesPlayed < totalFrames) {
        UINT32 padding = 0;
        hr = pAudioClient->GetCurrentPadding(&padding);
        if (FAILED(hr)) break;

        UINT32 framesAvailable = bufferFrameCount - padding;
        if (framesAvailable == 0) {
            Sleep(1);
            continue;
        }

        UINT32 framesToWrite = std::min(framesAvailable, totalFrames - framesPlayed);
        BYTE* pData = nullptr;
        hr = pRenderClient->GetBuffer(framesToWrite, &pData);
        if (FAILED(hr)) break;

        memcpy(pData, audioPtr + framesPlayed * frameSize, framesToWrite * frameSize);

        hr = pRenderClient->ReleaseBuffer(framesToWrite, 0);
        if (FAILED(hr)) break;

        framesPlayed += framesToWrite;
    }

    // Noch so lange warten, wie Audio läuft
    int durationMs = (totalFrames * 1000) / header.sampleRate;
    Sleep(durationMs + 300); // Reservezeit

    pAudioClient->Stop();
    pRenderClient->Release();
    CoTaskMemFree(pwfx);
    pAudioClient->Release();
    pDevice->Release();
    CoUninitialize();

    std::cout << "Playback beendet.\n";
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