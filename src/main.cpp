#include <SDL3/SDL.h>

#include "console/console.hpp"
#include "ppu/ppu.hpp"

namespace
{

constexpr int ScreenWidth = 256;
constexpr int ScreenHeight = 240;

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
    if (!SDL_Init(SDL_INIT_VIDEO))
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

    dendyforge::Console console;
    const bool romLoaded = argc > 1 && console.LoadRom(argv[1]);

    dendyforge::PPU demoPpu;
    if (!romLoaded)
    {
        PrepareDemoFrame(demoPpu);
    }

    bool running = true;
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

        if (romLoaded)
        {
            for (int cycle = 0; cycle < 1'000; ++cycle)
            {
                console.Clock();
            }
        }

        const auto& frameBuffer = romLoaded
            ? console.VideoProcessor().FrameBuffer()
            : demoPpu.FrameBuffer();
        const bool frameComplete = !romLoaded || console.VideoProcessor().ConsumeFrameComplete();
        if (frameComplete)
        {
            SDL_UpdateTexture(texture, nullptr, frameBuffer.data(),
                              ScreenWidth * static_cast<int>(sizeof(std::uint32_t)));
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        }
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
