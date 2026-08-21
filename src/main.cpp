#include <SDL3/SDL.h>

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

}

int main()
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

    dendyforge::PPU ppu;
    PrepareDemoFrame(ppu);

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
        }

        SDL_UpdateTexture(texture, nullptr, ppu.FrameBuffer().data(),
                          ScreenWidth * static_cast<int>(sizeof(std::uint32_t)));
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
