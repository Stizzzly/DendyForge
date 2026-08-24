#include <SDL3/SDL.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"

namespace
{

constexpr int ScreenWidth = 256;
constexpr int ScreenHeight = 240;
constexpr int LibraryWindowWidth = 1280;
constexpr int LibraryWindowHeight = 760;
constexpr double CpuClockHz = 1'789'773.0;
constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;
constexpr std::uint64_t MaximumElapsedNanoseconds = 100'000'000;
constexpr int AudioPrebufferDurationMilliseconds = 50;
constexpr int AudioMaximumQueueDurationMilliseconds = 100;

struct GameEntry
{
    std::filesystem::path romPath;
    std::string title;
    SDL_Texture* coverTexture = nullptr;
};

struct GameLibrary
{
    std::filesystem::path root;
    std::vector<GameEntry> games;

    ~GameLibrary()
    {
        ReleaseTextures();
    }

    void ReleaseTextures()
    {
        for (GameEntry& game : games)
        {
            SDL_DestroyTexture(game.coverTexture);
            game.coverTexture = nullptr;
        }
    }
};

std::string DisplayTitle(const std::filesystem::path& romPath)
{
    std::string title = romPath.stem().string();
    const std::size_t region = title.find_last_of('(');
    if (region != std::string::npos && !title.empty() && title.back() == ')')
    {
        title.erase(region);
        while (!title.empty() && title.back() == ' ')
        {
            title.pop_back();
        }
    }
    return title.empty() ? "Untitled cartridge" : title;
}

bool HasNesExtension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return extension == ".nes";
}

SDL_Texture* LoadCoverTexture(SDL_Renderer* renderer,
                              const std::filesystem::path& imagePath)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(imagePath.string().c_str(), &width, &height,
                                &channels, STBI_rgb_alpha);
    if (!pixels)
    {
        return nullptr;
    }

    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        width, height, SDL_PIXELFORMAT_RGBA32, pixels, width * 4);
    SDL_Texture* texture = surface
        ? SDL_CreateTextureFromSurface(renderer, surface)
        : nullptr;
    SDL_DestroySurface(surface);
    stbi_image_free(pixels);
    return texture;
}

std::filesystem::path FindCover(const GameLibrary& library,
                                const std::filesystem::path& romPath)
{
    static constexpr std::array<std::string_view, 3> extensions{
        ".png", ".jpg", ".jpeg"};

    for (std::string_view extension : extensions)
    {
        std::filesystem::path besideRom = romPath;
        besideRom.replace_extension(extension);
        if (std::filesystem::is_regular_file(besideRom))
        {
            return besideRom;
        }

        const std::filesystem::path inCovers = library.root / "covers" /
            (romPath.stem().string() + std::string(extension));
        if (std::filesystem::is_regular_file(inCovers))
        {
            return inCovers;
        }
    }
    return {};
}

void RefreshLibrary(GameLibrary& library, SDL_Renderer* renderer)
{
    library.ReleaseTextures();
    library.games.clear();

    std::error_code error;
    std::filesystem::create_directories(library.root, error);
    if (error)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot create game library: %s",
                    error.message().c_str());
        return;
    }

    for (std::filesystem::recursive_directory_iterator iterator(
             library.root, std::filesystem::directory_options::skip_permission_denied,
             error), end;
         !error && iterator != end; iterator.increment(error))
    {
        const std::filesystem::directory_entry& entry = *iterator;
        if (!entry.is_regular_file(error) || !HasNesExtension(entry.path()))
        {
            continue;
        }

        GameEntry game;
        game.romPath = entry.path();
        game.title = DisplayTitle(game.romPath);
        const std::filesystem::path coverPath = FindCover(library, game.romPath);
        if (!coverPath.empty())
        {
            game.coverTexture = LoadCoverTexture(renderer, coverPath);
        }
        library.games.push_back(std::move(game));
    }

    if (error)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot scan game library: %s",
                    error.message().c_str());
    }

    std::sort(library.games.begin(), library.games.end(),
              [](const GameEntry& left, const GameEntry& right)
              {
                  return left.title < right.title;
              });
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

ImU32 TileAccent(std::string_view title)
{
    std::uint32_t hash = 2166136261u;
    for (unsigned char character : title)
    {
        hash = (hash ^ character) * 16777619u;
    }
    static constexpr std::array<ImU32, 4> colours{
        IM_COL32(240, 91, 96, 255), IM_COL32(255, 170, 58, 255),
        IM_COL32(58, 185, 183, 255), IM_COL32(124, 106, 246, 255)};
    return colours[hash % colours.size()];
}

std::string ShortTitle(const std::string& title, std::size_t maximumLength)
{
    return title.size() <= maximumLength
        ? title
        : title.substr(0, maximumLength - 3) + "...";
}

void DrawCoverPlaceholder(ImDrawList* drawList, const ImVec2& topLeft,
                         const ImVec2& bottomRight, std::string_view title)
{
    const ImU32 accent = TileAccent(title);
    drawList->AddRectFilledMultiColor(topLeft, bottomRight,
                                      IM_COL32(20, 26, 42, 255), accent,
                                      IM_COL32(12, 17, 29, 255),
                                      IM_COL32(35, 40, 60, 255));
    const ImVec2 centre((topLeft.x + bottomRight.x) * 0.5f,
                        (topLeft.y + bottomRight.y) * 0.42f);
    drawList->AddCircleFilled(centre, 31.0f, IM_COL32(248, 245, 238, 235));
    drawList->AddRectFilled(ImVec2(centre.x - 47.0f, centre.y + 21.0f),
                            ImVec2(centre.x + 47.0f, centre.y + 48.0f),
                            IM_COL32(248, 245, 238, 235), 7.0f);
    drawList->AddCircleFilled(ImVec2(centre.x - 25.0f, centre.y + 34.0f), 5.0f,
                            IM_COL32(26, 31, 47, 255));
    drawList->AddCircleFilled(ImVec2(centre.x + 25.0f, centre.y + 34.0f), 5.0f,
                            IM_COL32(26, 31, 47, 255));
    drawList->AddLine(ImVec2(centre.x - 10.0f, centre.y + 34.0f),
                      ImVec2(centre.x + 10.0f, centre.y + 34.0f),
                      IM_COL32(26, 31, 47, 255), 3.0f);
    drawList->AddLine(ImVec2(centre.x, centre.y + 24.0f),
                      ImVec2(centre.x, centre.y + 44.0f),
                      IM_COL32(26, 31, 47, 255), 3.0f);
}

void SetupLibraryStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 18.0f;
    style.FrameRounding = 10.0f;
    style.GrabRounding = 10.0f;
    style.PopupRounding = 10.0f;
    style.ScrollbarRounding = 12.0f;
    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.FramePadding = ImVec2(12.0f, 9.0f);
    style.ItemSpacing = ImVec2(12.0f, 12.0f);

    ImVec4* colours = style.Colors;
    colours[ImGuiCol_Text] = ImVec4(0.93f, 0.95f, 0.98f, 1.0f);
    colours[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.07f, 0.11f, 1.0f);
    colours[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.10f, 0.16f, 0.92f);
    colours[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.16f, 0.24f, 1.0f);
    colours[ImGuiCol_FrameBgHovered] = ImVec4(0.17f, 0.22f, 0.32f, 1.0f);
    colours[ImGuiCol_Button] = ImVec4(0.15f, 0.32f, 0.54f, 1.0f);
    colours[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.43f, 0.68f, 1.0f);
    colours[ImGuiCol_ButtonActive] = ImVec4(0.12f, 0.25f, 0.43f, 1.0f);
    colours[ImGuiCol_Border] = ImVec4(0.22f, 0.29f, 0.42f, 0.7f);
}

std::optional<std::size_t> DrawGameLibrary(GameLibrary& library,
                                           std::array<char, 128>& searchBuffer)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("Game Library", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 windowPosition = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    drawList->AddRectFilledMultiColor(windowPosition,
                                      ImVec2(windowPosition.x + windowSize.x,
                                             windowPosition.y + windowSize.y),
                                      IM_COL32(10, 15, 27, 255), IM_COL32(18, 32, 58, 255),
                                      IM_COL32(8, 13, 24, 255), IM_COL32(12, 20, 37, 255));

    const float contentWidth = windowSize.x - 72.0f;
    ImGui::SetCursorPos(ImVec2(36.0f, 30.0f));
    ImGui::PushFont(nullptr, 1.65f);
    ImGui::TextUnformatted("DENDYFORGE");
    ImGui::PopFont();
    ImGui::TextDisabled("Your cartridge library");
    ImGui::SameLine(contentWidth - 210.0f);
    ImGui::SetNextItemWidth(210.0f);
    ImGui::InputTextWithHint("##search", "Search games...", searchBuffer.data(), searchBuffer.size());

    ImGui::SetCursorPos(ImVec2(36.0f, 111.0f));
    ImGui::PushFont(nullptr, 1.35f);
    ImGui::TextUnformatted("Ready to play");
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled("%d game%s", static_cast<int>(library.games.size()),
                        library.games.size() == 1 ? "" : "s");
    ImGui::SameLine(contentWidth - 82.0f);
    const bool refreshRequested = ImGui::Button("Refresh");

    ImGui::SetCursorPos(ImVec2(36.0f, 154.0f));
    ImGui::BeginChild("Library grid", ImVec2(contentWidth, windowSize.y - 184.0f),
                      ImGuiChildFlags_Borders);

    std::string search(searchBuffer.data());
    std::transform(search.begin(), search.end(), search.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    std::vector<std::size_t> visibleGames;
    for (std::size_t index = 0; index < library.games.size(); ++index)
    {
        std::string title = library.games[index].title;
        std::transform(title.begin(), title.end(), title.begin(),
                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        if (search.empty() || title.find(search) != std::string::npos)
        {
            visibleGames.push_back(index);
        }
    }

    std::optional<std::size_t> selectedGame;
    if (visibleGames.empty())
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos(ImVec2((available.x - 350.0f) * 0.5f, available.y * 0.35f));
        ImGui::PushFont(nullptr, 1.20f);
        ImGui::TextUnformatted(search.empty() ? "Your library is waiting" : "No matching games");
        ImGui::PopFont();
        ImGui::SetCursorPosX((available.x - 490.0f) * 0.5f);
        ImGui::TextDisabled(search.empty()
            ? "Put .nes files in roms/library, then click Refresh."
            : "Try another title or clear the search.");
    }
    else
    {
        constexpr float tileWidth = 190.0f;
        constexpr float tileHeight = 276.0f;
        constexpr float gap = 16.0f;
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const int columns = std::max(1, static_cast<int>((availableWidth + gap) / (tileWidth + gap)));
        const ImVec2 gridStart = ImGui::GetCursorScreenPos();
        ImDrawList* gridDrawList = ImGui::GetWindowDrawList();

        for (std::size_t position = 0; position < visibleGames.size(); ++position)
        {
            const std::size_t index = visibleGames[position];
            const int column = static_cast<int>(position % columns);
            const int row = static_cast<int>(position / columns);
            const ImVec2 tileTopLeft(gridStart.x + column * (tileWidth + gap),
                                     gridStart.y + row * (tileHeight + gap));
            const ImVec2 tileBottomRight(tileTopLeft.x + tileWidth, tileTopLeft.y + tileHeight);
            ImGui::SetCursorScreenPos(tileTopLeft);
            ImGui::PushID(static_cast<int>(index));
            const bool pressed = ImGui::InvisibleButton("play", ImVec2(tileWidth, tileHeight));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();

            gridDrawList->AddRectFilled(tileTopLeft, tileBottomRight,
                                        IM_COL32(18, 24, 38, 245), 13.0f);
            const ImVec2 coverTopLeft(tileTopLeft.x + 8.0f, tileTopLeft.y + 8.0f);
            const ImVec2 coverBottomRight(tileBottomRight.x - 8.0f, tileTopLeft.y + 194.0f);
            if (library.games[index].coverTexture)
            {
                gridDrawList->AddImage(ImTextureRef(library.games[index].coverTexture),
                                       coverTopLeft, coverBottomRight);
            }
            else
            {
                DrawCoverPlaceholder(gridDrawList, coverTopLeft, coverBottomRight,
                                     library.games[index].title);
            }
            gridDrawList->AddRect(coverTopLeft, coverBottomRight, IM_COL32(255, 255, 255, 42), 9.0f);
            gridDrawList->AddText(ImVec2(tileTopLeft.x + 12.0f, tileTopLeft.y + 210.0f),
                                  hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(226, 232, 242, 255),
                                  ShortTitle(library.games[index].title, 24).c_str());
            gridDrawList->AddText(ImVec2(tileTopLeft.x + 12.0f, tileTopLeft.y + 238.0f),
                                  IM_COL32(139, 159, 190, 255),
                                  library.games[index].coverTexture ? "PLAY" : "PLAY  ·  COVER OPTIONAL");
            if (hovered)
            {
                gridDrawList->AddRect(tileTopLeft, tileBottomRight, TileAccent(library.games[index].title),
                                      13.0f, 0, 2.5f);
            }
            if (pressed)
            {
                selectedGame = index;
            }
        }

        const int rows = static_cast<int>((visibleGames.size() + columns - 1) / columns);
        ImGui::SetCursorScreenPos(ImVec2(gridStart.x, gridStart.y + rows * (tileHeight + gap)));
        ImGui::Dummy(ImVec2(1.0f, 1.0f));
    }

    ImGui::EndChild();
    ImGui::End();
    return refreshRequested ? std::optional<std::size_t>(library.games.size()) : selectedGame;
}

}

int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not initialize SDL3: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("DendyForge", LibraryWindowWidth,
                                          LibraryWindowHeight, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    SDL_Texture* gameTexture = renderer
        ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                            SDL_TEXTUREACCESS_STREAMING, ScreenWidth, ScreenHeight)
        : nullptr;
    if (!window || !renderer || !gameTexture)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Could not create SDL3 video output: %s", SDL_GetError());
        SDL_DestroyTexture(gameTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_SetRenderVSync(renderer, 1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    SetupLibraryStyle();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    const SDL_AudioSpec audioSpec{SDL_AUDIO_F32, 1, dendyforge::APU::SampleRate};
    SDL_AudioStream* audioStream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audioSpec, nullptr, nullptr);
    bool audioPlaybackStarted = false;

    GameLibrary library;
    library.root = std::filesystem::path(DENDYFORGE_SOURCE_DIR) / "roms" / "library";
    RefreshLibrary(library, renderer);
    std::array<char, 128> searchBuffer{};
    std::unique_ptr<dendyforge::Console> console;
    std::vector<float> pendingAudio;
    bool gameRunning = false;
    bool zapperCrosshair = false;
    std::uint64_t previousTicks = SDL_GetTicksNS();
    double pendingCpuCycles = 0.0;

    auto returnToLibrary = [&]()
    {
        gameRunning = false;
        console.reset();
        pendingAudio.clear();
        audioPlaybackStarted = false;
        if (audioStream)
        {
            SDL_PauseAudioStreamDevice(audioStream);
            SDL_ClearAudioStream(audioStream);
        }
        SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        SDL_SetWindowTitle(window, "DendyForge");
    };

    auto launchGame = [&](const std::filesystem::path& romPath)
    {
        auto newConsole = std::make_unique<dendyforge::Console>();
        if (!newConsole->LoadRom(romPath.string()))
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load ROM: %s", romPath.string().c_str());
            return;
        }
        console = std::move(newConsole);
        pendingAudio.clear();
        audioPlaybackStarted = false;
        if (audioStream)
        {
            SDL_PauseAudioStreamDevice(audioStream);
            SDL_ClearAudioStream(audioStream);
        }
        SDL_SetRenderLogicalPresentation(renderer, ScreenWidth, ScreenHeight,
                                         SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        const std::string windowTitle = "DendyForge - " + DisplayTitle(romPath);
        SDL_SetWindowTitle(window, windowTitle.c_str());
        previousTicks = SDL_GetTicksNS();
        pendingCpuCycles = 0.0;
        gameRunning = true;
    };

    const char* romPath = nullptr;
    for (int index = 1; index < argc; ++index)
    {
        if (std::string_view(argv[index]) == "--zapper")
        {
            zapperCrosshair = true;
        }
        else if (!romPath)
        {
            romPath = argv[index];
        }
    }
    if (romPath)
    {
        launchGame(romPath);
    }

    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
            else if (gameRunning && event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
            {
                returnToLibrary();
            }
            else if (gameRunning && console &&
                     (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP))
            {
                SetControllerButton(*console, event.key.key, event.type == SDL_EVENT_KEY_DOWN);
            }
        }

        if (!gameRunning)
        {
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            const std::optional<std::size_t> selection = DrawGameLibrary(library, searchBuffer);
            ImGui::Render();
            SDL_SetRenderDrawColor(renderer, 10, 15, 27, 255);
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
            SDL_RenderPresent(renderer);

            if (selection)
            {
                if (*selection == library.games.size())
                {
                    RefreshLibrary(library, renderer);
                }
                else
                {
                    launchGame(library.games[*selection].romPath);
                }
            }
            SDL_Delay(1);
            continue;
        }

        float mouseX = 0.0f;
        float mouseY = 0.0f;
        const SDL_MouseButtonFlags mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);
        float logicalX = 0.0f;
        float logicalY = 0.0f;
        if (SDL_RenderCoordinatesFromWindow(renderer, mouseX, mouseY, &logicalX, &logicalY))
        {
            console->SecondaryZapper().SetAim(static_cast<int>(logicalX), static_cast<int>(logicalY));
        }
        console->SecondaryZapper().SetTrigger((mouseButtons & SDL_BUTTON_LMASK) != 0);

        const std::uint64_t currentTicks = SDL_GetTicksNS();
        const std::uint64_t elapsedNanoseconds = std::min(
            currentTicks - previousTicks, MaximumElapsedNanoseconds);
        previousTicks = currentTicks;
        pendingCpuCycles += static_cast<double>(elapsedNanoseconds) * CpuClockHz / NanosecondsPerSecond;
        bool frameComplete = false;
        while (pendingCpuCycles >= 1.0 && !frameComplete)
        {
            console->Clock();
            pendingCpuCycles -= 1.0;
            frameComplete = console->VideoProcessor().ConsumeFrameComplete();
        }

        if (audioStream)
        {
            const auto samples = console->AudioProcessor().TakeSamples();
            pendingAudio.insert(pendingAudio.end(), samples.begin(), samples.end());
            const int prebufferBytes = dendyforge::APU::SampleRate *
                AudioPrebufferDurationMilliseconds / 1'000 * static_cast<int>(sizeof(float));
            const int maximumQueuedBytes = dendyforge::APU::SampleRate *
                AudioMaximumQueueDurationMilliseconds / 1'000 * static_cast<int>(sizeof(float));
            const int queuedBytes = SDL_GetAudioStreamQueued(audioStream);
            if (queuedBytes >= 0)
            {
                const int pendingBytes = static_cast<int>(pendingAudio.size() * sizeof(float));
                const int bytesToQueue = std::min(pendingBytes,
                    std::max(0, maximumQueuedBytes - queuedBytes));
                if (bytesToQueue != 0 &&
                    SDL_PutAudioStreamData(audioStream, pendingAudio.data(), bytesToQueue))
                {
                    pendingAudio.erase(pendingAudio.begin(), pendingAudio.begin() +
                        bytesToQueue / static_cast<int>(sizeof(float)));
                }
                if (!audioPlaybackStarted && queuedBytes + bytesToQueue >= prebufferBytes &&
                    SDL_ResumeAudioStreamDevice(audioStream))
                {
                    audioPlaybackStarted = true;
                }
            }
        }

        if (frameComplete)
        {
            const auto& frameBuffer = console->VideoProcessor().FrameBuffer();
            SDL_UpdateTexture(gameTexture, nullptr, frameBuffer.data(),
                              ScreenWidth * static_cast<int>(sizeof(std::uint32_t)));
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, gameTexture, nullptr, nullptr);
            if (zapperCrosshair && logicalX >= 0.0f && logicalX < ScreenWidth &&
                logicalY >= 0.0f && logicalY < ScreenHeight)
            {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderLine(renderer, logicalX - 6.0f, logicalY, logicalX - 2.0f, logicalY);
                SDL_RenderLine(renderer, logicalX + 2.0f, logicalY, logicalX + 6.0f, logicalY);
                SDL_RenderLine(renderer, logicalX, logicalY - 6.0f, logicalX, logicalY - 2.0f);
                SDL_RenderLine(renderer, logicalX, logicalY + 2.0f, logicalX, logicalY + 6.0f);
            }
            SDL_RenderPresent(renderer);
        }
        SDL_Delay(1);
    }

    console.reset();
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyAudioStream(audioStream);
    library.ReleaseTextures();
    SDL_DestroyTexture(gameTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
