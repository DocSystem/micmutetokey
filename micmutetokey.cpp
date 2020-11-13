#include <windows.h>
#include <stdio.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <fstream>
#include "INIReader.h"

int GetMute() {

    HRESULT hr;

    CoInitialize(NULL);
    IMMDeviceEnumerator* deviceEnumerator = NULL;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (LPVOID*)&deviceEnumerator);
    IMMDevice* defaultDevice = NULL;

    hr = deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &defaultDevice);
    deviceEnumerator->Release();
    deviceEnumerator = NULL;

    IAudioEndpointVolume* endpointVolume = NULL;
    hr = defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, NULL, (LPVOID*)&endpointVolume);
    defaultDevice->Release();
    defaultDevice = NULL;

    BOOL mute;
    hr = endpointVolume->GetMute(&mute);

    endpointVolume->Release();
    CoUninitialize();

    return mute ? 1 : 0;
}

int main()
{
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    INIReader reader("config.ini");
    if (reader.ParseError() != 0) {
        printf("Can't load 'config.ini'\n");
        std::ofstream ConfigFile("config.ini");
        ConfigFile << "[General]\n";
        ConfigFile << "refreshRate=100\n\n";
        ConfigFile << "[Keys]\n";
        ConfigFile << "onMute=0x82\n";
        ConfigFile << "onUnmute=0x82\n";
        ConfigFile << "; Here are all key codes: https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes\n";
        ConfigFile << "; By default, both keys are f19\n";
        printf("Succesfully created config file\n");
    }
    printf("Config loaded from 'config.ini'\n");
    int muteKeyCode = reader.GetInteger("Keys", "onMute", 0x82); // virtual key-code to simulate on mute
    int unmuteKeyCode = reader.GetInteger("Keys", "onUnmute", 0x82); // virtual key-code to simulate on unmute
    int refreshRate = 1000 / reader.GetInteger("General", "refreshRate", 100);
    int currentMicMuteStatus = GetMute();
    int oldCurrentMicMuteStatus = currentMicMuteStatus;
    while (true)
    {
        Sleep(refreshRate);
        oldCurrentMicMuteStatus = currentMicMuteStatus;
        currentMicMuteStatus = GetMute();
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