#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

#include "console/console.hpp"
#include "ppu/ppu.hpp"

namespace
{

constexpr int ScreenWidth = 256;
constexpr int ScreenHeight = 240;
constexpr double CpuClockHz = 1'789'773.0;
constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;
constexpr std::uint64_t MaximumElapsedNanoseconds = 100'000'000;
constexpr int AudioPrebufferDurationMilliseconds = 50;
constexpr int AudioMaximumQueueDurationMilliseconds = 100;

void PrepareDemoFrame(dendyforge::PPU& ppu)
{
    ppu.CpuWrite(0x2001, 0x08);
    ppu.PpuWrite(0x3F00, 0x0F);
    ppu.PpuWrite(0x3F01, 0x21);
    ppu.PpuWrite(0x3F02, 0x31);
    ppu.PpuWrite(0x3F03, 0x30);

    for (std::uint16_t row = 0; row < 8; ++row)
    {
        const std::uint8_t stripe = (row & 1) ? 0x55 : 0xAA;
        ppu.PpuWrite(row, stripe);
        ppu.PpuWrite(row + 8, 0x00);
    }

    ppu.RenderBackground();
}

void SetControllerButton(dendyforge::Console& console, SDL_Keycode key, bool pressed)
{
    using Button = dendyforge::Controller::Button;

    switch (key)
    {
    case SDLK_W: console.PrimaryController().SetButton(Button::Up, pressed); break;
    case SDLK_A: console.PrimaryController().SetButton(Button::Left, pressed); break;
    case SDLK_S: console.PrimaryController().SetButton(Button::Down, pressed); break;
    case SDLK_D: console.PrimaryController().SetButton(Button::Right, pressed); break;
    case SDLK_BACKSPACE: console.PrimaryController().SetButton(Button::Select, pressed); break;
    case SDLK_RETURN: console.PrimaryController().SetButton(Button::Start, pressed); break;
    case SDLK_K: console.PrimaryController().SetButton(Button::A, pressed); break;
    case SDLK_L: console.PrimaryController().SetButton(Button::B, pressed); break;
    default: break;
    }
}

}

int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Could not initialize SDL3: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "DendyForge - PPU Frame Buffer", ScreenWidth * 3, ScreenHeight * 3,
        SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    SDL_Texture* texture = renderer
        ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                            SDL_TEXTUREACCESS_STREAMING, ScreenWidth, ScreenHeight)
        : nullptr;

    if (!window || !renderer || !texture)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Could not create SDL3 video output: %s", SDL_GetError());
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderLogicalPresentation(
        renderer, ScreenWidth, ScreenHeight, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    if (!SDL_SetRenderVSync(renderer, 1))
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Could not enable renderer VSync: %s", SDL_GetError());
    }

    const SDL_AudioSpec audioSpec{SDL_AUDIO_F32, 1, dendyforge::APU::SampleRate};
    SDL_AudioStream* audioStream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audioSpec, nullptr, nullptr);
    bool audioPlaybackStarted = false;
    if (!audioStream)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Could not open audio output: %s", SDL_GetError());
    }
    dendyforge::Console console;
    // The first non-flag argument is the ROM path; `--zapper` shows the
    // crosshair overlay (mouse aiming itself is always active).
    bool zapperCrosshair = false;
    const char* romPath = nullptr;
    for (int index = 1; index < argc; ++index)
    {
        if (std::string_view(argv[index]) == "--zapper")
        {
            zapperCrosshair = true;
        }
        else if (romPath == nullptr)
        {
            romPath = argv[index];
        }
    }
    const bool romLoaded = romPath != nullptr && console.LoadRom(romPath);

    dendyforge::PPU demoPpu;
    if (!romLoaded)
    {
        PrepareDemoFrame(demoPpu);
    }

    bool running = true;
    std::uint64_t previousTicks = SDL_GetTicksNS();
    double pendingCpuCycles = 0.0;
    std::vector<float> pendingAudio;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
            else if (romLoaded &&
                     (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP))
            {
                SetControllerButton(console, event.key.key,
                                    event.type == SDL_EVENT_KEY_DOWN);
            }
        }

        bool frameComplete = false;
        if (romLoaded)
        {
            // The Zapper aims through the mouse: the left button is the
            // trigger. Map the window cursor into the 256x240 logical
            // presentation space before each frame.
            float mouseX = 0.0f;
            float mouseY = 0.0f;
            const SDL_MouseButtonFlags mouseButtons =
                SDL_GetMouseState(&mouseX, &mouseY);
            float logicalX = 0.0f;
            float logicalY = 0.0f;
            if (SDL_RenderCoordinatesFromWindow(
                    renderer, mouseX, mouseY, &logicalX, &logicalY))
            {
                console.SecondaryZapper().SetAim(
                    static_cast<int>(logicalX), static_cast<int>(logicalY));
            }
            console.SecondaryZapper().SetTrigger(
                (mouseButtons & SDL_BUTTON_LMASK) != 0);

            const std::uint64_t currentTicks = SDL_GetTicksNS();
            const std::uint64_t elapsedNanoseconds = std::min(
                currentTicks - previousTicks, MaximumElapsedNanoseconds);
            previousTicks = currentTicks;
            pendingCpuCycles += static_cast<double>(elapsedNanoseconds) *
                CpuClockHz / NanosecondsPerSecond;

            while (pendingCpuCycles >= 1.0 && !frameComplete)
            {
                console.Clock();
                pendingCpuCycles -= 1.0;
                frameComplete = console.VideoProcessor().ConsumeFrameComplete();
            }
        }

        if (audioStream && romLoaded)
        {
            const auto samples = console.AudioProcessor().TakeSamples();
            pendingAudio.insert(pendingAudio.end(), samples.begin(), samples.end());

            const int prebufferBytes = dendyforge::APU::SampleRate *
                AudioPrebufferDurationMilliseconds / 1'000 * static_cast<int>(sizeof(float));
            const int maximumQueuedBytes = dendyforge::APU::SampleRate *
                AudioMaximumQueueDurationMilliseconds / 1'000 * static_cast<int>(sizeof(float));
            const int queuedBytes = SDL_GetAudioStreamQueued(audioStream);
            if (queuedBytes < 0)
            {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Could not inspect queued audio: %s", SDL_GetError());
            }
            else
            {
                const int pendingBytes = static_cast<int>(
                    pendingAudio.size() * sizeof(float));
                const int bytesToQueue = std::min(
                    pendingBytes, std::max(0, maximumQueuedBytes - queuedBytes));
                if (bytesToQueue != 0 &&
                    !SDL_PutAudioStreamData(audioStream, pendingAudio.data(), bytesToQueue))
                {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "Could not queue audio: %s", SDL_GetError());
                }
                else if (bytesToQueue != 0)
                {
                    pendingAudio.erase(
                        pendingAudio.begin(),
                        pendingAudio.begin() + bytesToQueue / static_cast<int>(sizeof(float)));
                }

                if (!audioPlaybackStarted && queuedBytes + bytesToQueue >= prebufferBytes)
                {
                    if (!SDL_ResumeAudioStreamDevice(audioStream))
                    {
                        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                    "Could not start audio output: %s", SDL_GetError());
                        SDL_DestroyAudioStream(audioStream);
                        audioStream = nullptr;
                    }
                    else
                    {
                        audioPlaybackStarted = true;
                    }
                }
            }
        }

        const auto& frameBuffer = romLoaded
            ? console.VideoProcessor().FrameBuffer()
            : demoPpu.FrameBuffer();
        if (!romLoaded || frameComplete)
        {
            SDL_UpdateTexture(texture, nullptr, frameBuffer.data(),
                              ScreenWidth * static_cast<int>(sizeof(std::uint32_t)));
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, texture, nullptr, nullptr);

            // Draw the Zapper crosshair at the mouse position when the
            // launch flag asks for it.
            if (zapperCrosshair)
            {
                float mouseX = 0.0f;
                float mouseY = 0.0f;
                SDL_GetMouseState(&mouseX, &mouseY);
                float logicalX = 0.0f;
                float logicalY = 0.0f;
                if (SDL_RenderCoordinatesFromWindow(
                        renderer, mouseX, mouseY, &logicalX, &logicalY) &&
                    logicalX >= 0.0f && logicalX < ScreenWidth &&
                    logicalY >= 0.0f && logicalY < ScreenHeight)
                {
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                    SDL_RenderLine(renderer, logicalX - 6.0f, logicalY,
                                   logicalX - 2.0f, logicalY);
                    SDL_RenderLine(renderer, logicalX + 2.0f, logicalY,
                                   logicalX + 6.0f, logicalY);
                    SDL_RenderLine(renderer, logicalX, logicalY - 6.0f,
                                   logicalX, logicalY - 2.0f);
                    SDL_RenderLine(renderer, logicalX, logicalY + 2.0f,
                                   logicalX, logicalY + 6.0f);
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_RenderLine(renderer, logicalX - 1.0f, logicalY,
                                   logicalX + 1.0f, logicalY);
                    SDL_RenderLine(renderer, logicalX, logicalY - 1.0f,
                                   logicalX, logicalY + 1.0f);
                }
            }

            SDL_RenderPresent(renderer);
        }

        SDL_Delay(1);
    }

    SDL_DestroyAudioStream(audioStream);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
