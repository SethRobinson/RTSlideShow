#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

void SetWindowsSystemVolume(int volume)
{
    // Clamp volume to 0-100 range
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    // Convert to 0.0-1.0 range
    float volumeLevel = volume / 100.0f;
    
    HRESULT hr;
    CoInitialize(NULL);
    
    IMMDeviceEnumerator *deviceEnumerator = NULL;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, 
                          __uuidof(IMMDeviceEnumerator), (LPVOID *)&deviceEnumerator);
    
    if (SUCCEEDED(hr))
    {
        IMMDevice *defaultDevice = NULL;
        hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
        
        if (SUCCEEDED(hr))
        {
            IAudioEndpointVolume *endpointVolume = NULL;
            hr = defaultDevice->Activate(__uuidof(IAudioEndpointVolume), 
                                        CLSCTX_INPROC_SERVER, NULL, (LPVOID *)&endpointVolume);
            
            if (SUCCEEDED(hr))
            {
                hr = endpointVolume->SetMasterVolumeLevelScalar(volumeLevel, NULL);
                endpointVolume->Release();
            }
            
            defaultDevice->Release();
        }
        
        deviceEnumerator->Release();
    }
    
    CoUninitialize();
}

#else

void SetWindowsSystemVolume(int volume)
{
    // Stub for non-Windows platforms
}

#endif