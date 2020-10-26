#include <windows.h>
#include <stdio.h>
#include <mmdeviceapi.h>
#include <devicetopology.h>
#include <WinUser.h>
#include <iostream>
#include <fstream>
#include "INIReader.h"

HRESULT WalkTreeBackwardsFromPart(IPart* pPart);
HRESULT DisplayMute(IAudioMute* pMute);

int MuteStatus{ 0 };

int getMicrophoneMuteStatus() {
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        printf("Failed CoInitializeEx: hr = 0x%08x\n", hr);
        return __LINE__;
    }

    // get default render endpoint
    IMMDeviceEnumerator* pEnum = NULL;
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&pEnum
    );
    if (FAILED(hr)) {
        printf("Couldn't get device enumerator: hr = 0x%08x\n", hr);
        CoUninitialize();
        return __LINE__;
    }
    IMMDevice* pDevice = NULL;
    hr = pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) {
        printf("Couldn't get default render device: hr = 0x%08x\n", hr);
        pEnum->Release();
        CoUninitialize();
        return __LINE__;
    }
    pEnum->Release();

    // get device topology object for that endpoint
    IDeviceTopology* pDT = NULL;
    hr = pDevice->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, NULL, (void**)&pDT);
    if (FAILED(hr)) {
        printf("Couldn't get device topology object: hr = 0x%08x\n", hr);
        pDevice->Release();
        CoUninitialize();
        return __LINE__;
    }
    pDevice->Release();

    // get the single connector for that endpoint
    IConnector* pConnEndpoint = NULL;
    hr = pDT->GetConnector(0, &pConnEndpoint);
    if (FAILED(hr)) {
        printf("Couldn't get the connector on the endpoint: hr = 0x%08x\n", hr);
        pDT->Release();
        CoUninitialize();
        return __LINE__;
    }
    pDT->Release();

    // get the connector on the device that is
    // connected to
    // the connector on the endpoint
    IConnector* pConnDevice = NULL;
    hr = pConnEndpoint->GetConnectedTo(&pConnDevice);
    if (FAILED(hr)) {
        printf("Couldn't get the connector on the device: hr = 0x%08x\n", hr);
        pConnEndpoint->Release();
        CoUninitialize();
        return __LINE__;
    }
    pConnEndpoint->Release();

    // QI on the device's connector for IPart
    IPart* pPart = NULL;
    hr = pConnDevice->QueryInterface(__uuidof(IPart), (void**)&pPart);
    if (FAILED(hr)) {
        printf("Couldn't get the part: hr = 0x%08x\n", hr);
        pConnDevice->Release();
        CoUninitialize();
        return __LINE__;
    }
    pConnDevice->Release();

    // all the real work is done in this function
    MuteStatus = 0;
    hr = WalkTreeBackwardsFromPart(pPart);
    if (FAILED(hr)) {
        printf("Couldn't walk the tree: hr = 0x%08x\n", hr);
        pPart->Release();
        CoUninitialize();
        return __LINE__;
    }
    pPart->Release();

    CoUninitialize();

    return hr;
}

HRESULT WalkTreeBackwardsFromPart(IPart* pPart) {
    HRESULT hr = S_OK;

    LPWSTR pwszPartName = NULL;
    hr = pPart->GetName(&pwszPartName);
    //printf("Part name: %ws\n", *pwszPartName ? pwszPartName : L"(Unnamed)");
    CoTaskMemFree(pwszPartName);

    // see if this is a mute node part
    IAudioMute* pMute = NULL;
    hr = pPart->Activate(CLSCTX_ALL, __uuidof(IAudioMute), (void**)&pMute);
    if (E_NOINTERFACE == hr) {
        // not a mute node
    }
    else if (FAILED(hr)) {
        printf("Unexpected failure trying to activate IAudioMute: hr = 0x%08x\n", hr);
        return hr;
    }
    else {
        // it's a mute node...
        hr = DisplayMute(pMute);
        if (FAILED(hr)) {
            printf("DisplayMute failed: hr = 0x%08x", hr);
            pMute->Release();
            return hr;
        }
        if (hr == 1) {
            MuteStatus = 1;
        }
        pMute->Release();
    }

    // get the list of incoming parts
    IPartsList* pIncomingParts = NULL;
    hr = pPart->EnumPartsIncoming(&pIncomingParts);
    if (E_NOTFOUND == hr) {
        // not an error... we've just reached the end of the path
        // printf("No incoming parts at this part\n");
        return S_OK;
    }
    if (FAILED(hr)) {
        printf("Couldn't enum incoming parts: hr = 0x%08x\n", hr);
        return hr;
    }
    UINT nParts = 0;
    hr = pIncomingParts->GetCount(&nParts);
    if (FAILED(hr)) {
        printf("Couldn't get count of incoming parts: hr = 0x%08x\n", hr);
        pIncomingParts->Release();
        return hr;
    }

    // walk the tree on each incoming part recursively
    for (UINT n = 0; n < nParts; n++) {
        IPart* pIncomingPart = NULL;
        hr = pIncomingParts->GetPart(n, &pIncomingPart);

        hr = WalkTreeBackwardsFromPart(pIncomingPart);
        pIncomingPart->Release();
    }

    pIncomingParts->Release();

    return MuteStatus;
}


HRESULT DisplayMute(IAudioMute* pMute) {
    HRESULT hr = S_OK;
    BOOL bMuted = FALSE;
    hr = pMute->GetMute(&bMuted);

    if (FAILED(hr)) {
        printf("GetMute failed: hr = 0x%08x\n", hr);
        return hr;
    }

    return bMuted ? 1 : 0;
}


int main()
{
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    INIReader reader("config.ini");
    if (reader.ParseError() != 0) {
        printf("Can't load 'config.ini'\n");
        std::ofstream ConfigFile("config.ini");
        ConfigFile << "[keys]\n";
        ConfigFile << "onMute=0x82\n";
        ConfigFile << "onUnmute=0x82\n";
        ConfigFile << "; Here are all key codes: https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes\n";
        ConfigFile << "; By default, both keys are f19\n";
        printf("Succesfully created config file\n");
    }
    printf("Config loaded from 'config.ini'");
    int muteKeyCode = reader.GetInteger("keys", "onMute", 0x82); // virtual key-code to simulate on mute
    int unmuteKeyCode = reader.GetInteger("keys", "onUnmute", 0x82); // virtual key-code to simulate on unmute
    int currentMicMuteStatus = getMicrophoneMuteStatus();
    int oldCurrentMicMuteStatus = currentMicMuteStatus;
    while (true)
    {
        Sleep(10);
        oldCurrentMicMuteStatus = currentMicMuteStatus;
        currentMicMuteStatus = getMicrophoneMuteStatus();
        if (oldCurrentMicMuteStatus != currentMicMuteStatus)
        {
            if (currentMicMuteStatus == 0)
            {
                INPUT inp;
                inp.type = INPUT_KEYBOARD;
                inp.ki.wScan = 0;
                inp.ki.time = 0;
                inp.ki.dwExtraInfo = 0;
                inp.ki.wVk = muteKeyCode; // virtual-key code
                inp.ki.dwFlags = 0; // 0 for key press
                SendInput(1, &inp, sizeof(INPUT));
                inp.ki.dwFlags = KEYEVENTF_KEYUP; // for key release
                SendInput(1, &inp, sizeof(INPUT));
            }
            else
            {
                INPUT inp;
                inp.type = INPUT_KEYBOARD;
                inp.ki.wScan = 0;
                inp.ki.time = 0;
                inp.ki.dwExtraInfo = 0;
                inp.ki.wVk = unmuteKeyCode; // virtual-key code
                inp.ki.dwFlags = 0; // 0 for key press
                SendInput(1, &inp, sizeof(INPUT));
                inp.ki.dwFlags = KEYEVENTF_KEYUP; // for key release
                SendInput(1, &inp, sizeof(INPUT));
            }
        }
    }
    return 0;
}