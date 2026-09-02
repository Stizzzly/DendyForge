#include <SDL3/SDL.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
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
    int coverWidth = 0;
    int coverHeight = 0;
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
            game.coverWidth = 0;
            game.coverHeight = 0;
        }
    }
};

enum class ControlAction
{
    Up,
    Down,
    Left,
    Right,
    Select,
    Start,
    A,
    B,
};

struct ControllerBindings
{
    SDL_Keycode up = SDLK_W;
    SDL_Keycode down = SDLK_S;
    SDL_Keycode left = SDLK_A;
    SDL_Keycode right = SDLK_D;
    SDL_Keycode select = SDLK_BACKSPACE;
    SDL_Keycode start = SDLK_RETURN;
    SDL_Keycode a = SDLK_K;
    SDL_Keycode b = SDLK_L;
};

struct CoverServiceConfig
{
    std::string theGamesDbApiKey;
    ControllerBindings controller;
};

struct CoverDownloadResult
{
    std::filesystem::path romPath;
    std::filesystem::path cachePath;
    std::string message;
    bool downloaded = false;
};

struct LibraryUiAction
{
    std::optional<std::size_t> selectedGame;
    bool refresh = false;
    bool openSettings = false;
};

struct SettingsUiAction
{
    bool close = false;
    bool save = false;
    bool resetControls = false;
    bool downloadMissingCovers = false;
    std::optional<ControlAction> selectControl;
};

struct DebuggerState
{
    bool visible = false;
    bool paused = false;
    bool pauseRequested = false;
    bool stepRequested = false;
    std::uint16_t memoryAddress = 0;
    std::array<char, 8> memoryAddressText{{'0', '0', '0', '0', '\0'}};
    std::array<char, 8> breakpointText{};
    std::vector<std::uint16_t> breakpoints;
    std::optional<std::uint16_t> ignoredBreakpoint;
};

using Json = nlohmann::json;

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
                              const std::filesystem::path& imagePath,
                              int* imageWidth = nullptr, int* imageHeight = nullptr)
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
    if (texture)
    {
        if (imageWidth)
        {
            *imageWidth = width;
        }
        if (imageHeight)
        {
            *imageHeight = height;
        }
    }
    return texture;
}

std::filesystem::path ApplicationAssetPath(std::string_view relativePath)
{
    const char* basePath = SDL_GetBasePath();
    if (basePath)
    {
        const std::filesystem::path path = std::filesystem::path(basePath) / relativePath;
        if (std::filesystem::is_regular_file(path))
        {
            return path;
        }
    }
    return std::filesystem::path(DENDYFORGE_SOURCE_DIR) / relativePath;
}

std::filesystem::path SavePathForRom(const std::filesystem::path& romPath)
{
    std::filesystem::path savePath = romPath;
    savePath.replace_extension(".sav");
    return savePath;
}

bool LoadBatterySave(dendyforge::Console& console, const std::filesystem::path& savePath)
{
    if (!console.HasBatteryBackedPrgRam())
    {
        return true;
    }

    std::error_code error;
    if (!std::filesystem::exists(savePath, error))
    {
        if (error)
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot inspect save %s: %s",
                        savePath.string().c_str(), error.message().c_str());
        }
        return !error;
    }
    const std::uintmax_t expectedSize = console.BatteryBackedPrgRam().size();
    const std::uintmax_t actualSize = std::filesystem::file_size(savePath, error);
    if (error || actualSize != expectedSize)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Ignoring save %s: expected %llu bytes, found %llu.",
                    savePath.string().c_str(), static_cast<unsigned long long>(expectedSize),
                    static_cast<unsigned long long>(actualSize));
        return false;
    }

    std::vector<std::uint8_t> data(static_cast<std::size_t>(actualSize));
    std::ifstream input(savePath, std::ios::binary);
    input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!input || !console.RestoreBatteryBackedPrgRam(data))
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not load save %s.",
                    savePath.string().c_str());
        return false;
    }
    SDL_Log("Loaded battery save: %s", savePath.string().c_str());
    return true;
}

bool ReplaceFileAtomically(const std::filesystem::path& temporaryPath,
                           const std::filesystem::path& destinationPath,
                           std::error_code& error)
{
#ifdef _WIN32
    if (MoveFileExW(temporaryPath.c_str(), destinationPath.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        return true;
    }
    error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
    return false;
#else
    std::filesystem::rename(temporaryPath, destinationPath, error);
    return !error;
#endif
}

bool SaveBatterySave(const dendyforge::Console& console,
                     const std::filesystem::path& savePath)
{
    if (!console.HasBatteryBackedPrgRam())
    {
        return true;
    }
    const std::span<const std::uint8_t> data = console.BatteryBackedPrgRam();
    const std::filesystem::path temporaryPath = savePath.string() + ".tmp";
    std::error_code error;
    if (!savePath.parent_path().empty())
    {
        std::filesystem::create_directories(savePath.parent_path(), error);
    }
    if (error)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot create save directory %s: %s",
                    savePath.parent_path().string().c_str(), error.message().c_str());
        return false;
    }
    {
        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(data.data()),
                     static_cast<std::streamsize>(data.size()));
        if (!output)
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not write temporary save %s.",
                        temporaryPath.string().c_str());
            std::filesystem::remove(temporaryPath, error);
            return false;
        }
    }
    if (!ReplaceFileAtomically(temporaryPath, savePath, error))
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not replace save %s: %s",
                    savePath.string().c_str(), error.message().c_str());
        std::filesystem::remove(temporaryPath, error);
        return false;
    }
    SDL_Log("Saved battery RAM: %s", savePath.string().c_str());
    return true;
}

struct InterfaceFonts
{
    ImFont* body = nullptr;
    ImFont* display = nullptr;
};

InterfaceFonts LoadInterfaceFonts()
{
    const std::filesystem::path fontPath = ApplicationAssetPath("assets/fonts/Jura[wght].ttf");
    const std::string fontPathString = fontPath.string();
    ImGuiIO& io = ImGui::GetIO();
    InterfaceFonts fonts;
    fonts.body = io.Fonts->AddFontFromFileTTF(fontPathString.c_str(), 18.0f);
    fonts.display = io.Fonts->AddFontFromFileTTF(fontPathString.c_str(), 31.0f);
    if (fonts.body)
    {
        io.FontDefault = fonts.body;
    }
    if (!fonts.body || !fonts.display)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Could not load bundled Jura interface font; using Dear ImGui default.");
    }
    return fonts;
}

std::filesystem::path CoverConfigPath(const GameLibrary& library)
{
    return library.root / ".dendyforge-covers.json";
}

std::filesystem::path CachedCoverPath(const GameLibrary& library,
                                      const std::filesystem::path& romPath)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char character : romPath.lexically_normal().generic_string())
    {
        hash = (hash ^ character) * 1099511628211ull;
    }
    return library.root / "covers" / ".dendyforge-cache" /
        (std::to_string(hash) + ".jpg");
}

const char* ControlName(ControlAction action)
{
    switch (action)
    {
    case ControlAction::Up: return "Up";
    case ControlAction::Down: return "Down";
    case ControlAction::Left: return "Left";
    case ControlAction::Right: return "Right";
    case ControlAction::Select: return "Select";
    case ControlAction::Start: return "Start";
    case ControlAction::A: return "A";
    case ControlAction::B: return "B";
    }
    return "Unknown";
}

SDL_Keycode& BindingFor(ControllerBindings& bindings, ControlAction action)
{
    switch (action)
    {
    case ControlAction::Up: return bindings.up;
    case ControlAction::Down: return bindings.down;
    case ControlAction::Left: return bindings.left;
    case ControlAction::Right: return bindings.right;
    case ControlAction::Select: return bindings.select;
    case ControlAction::Start: return bindings.start;
    case ControlAction::A: return bindings.a;
    case ControlAction::B: return bindings.b;
    }
    return bindings.a;
}

SDL_Keycode BindingFor(const ControllerBindings& bindings, ControlAction action)
{
    switch (action)
    {
    case ControlAction::Up: return bindings.up;
    case ControlAction::Down: return bindings.down;
    case ControlAction::Left: return bindings.left;
    case ControlAction::Right: return bindings.right;
    case ControlAction::Select: return bindings.select;
    case ControlAction::Start: return bindings.start;
    case ControlAction::A: return bindings.a;
    case ControlAction::B: return bindings.b;
    }
    return bindings.a;
}

std::string BindingName(SDL_Keycode key)
{
    const char* name = SDL_GetKeyName(key);
    return name && *name ? name : "Unassigned";
}

ControllerBindings ReadBindings(const Json& config)
{
    ControllerBindings bindings;
    const Json keyboard = config.value("keyboard", Json::object());
    const auto read = [&keyboard](std::string_view name, SDL_Keycode fallback)
    {
        return static_cast<SDL_Keycode>(keyboard.value(std::string(name),
                                                        static_cast<int>(fallback)));
    };
    bindings.up = read("up", bindings.up);
    bindings.down = read("down", bindings.down);
    bindings.left = read("left", bindings.left);
    bindings.right = read("right", bindings.right);
    bindings.select = read("select", bindings.select);
    bindings.start = read("start", bindings.start);
    bindings.a = read("a", bindings.a);
    bindings.b = read("b", bindings.b);
    return bindings;
}

Json WriteBindings(const ControllerBindings& bindings)
{
    return {
        {"up", static_cast<int>(bindings.up)},
        {"down", static_cast<int>(bindings.down)},
        {"left", static_cast<int>(bindings.left)},
        {"right", static_cast<int>(bindings.right)},
        {"select", static_cast<int>(bindings.select)},
        {"start", static_cast<int>(bindings.start)},
        {"a", static_cast<int>(bindings.a)},
        {"b", static_cast<int>(bindings.b)},
    };
}

CoverServiceConfig LoadCoverServiceConfig(const GameLibrary& library)
{
    std::ifstream input(CoverConfigPath(library));
    if (!input)
    {
        return {};
    }

    try
    {
        const Json config = Json::parse(input, nullptr, true, true);
        return {config.value("thegamesdb_api_key", ""), ReadBindings(config)};
    }
    catch (const Json::exception&)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Ignoring invalid cover service configuration.");
        return {};
    }
}

bool SaveCoverServiceConfig(const GameLibrary& library, const CoverServiceConfig& config)
{
    std::ofstream output(CoverConfigPath(library), std::ios::trunc);
    if (!output)
    {
        return false;
    }
    output << Json{
        {"thegamesdb_api_key", config.theGamesDbApiKey},
        {"keyboard", WriteBindings(config.controller)},
    }.dump(2) << '\n';
    return static_cast<bool>(output);
}

std::string UrlEncode(std::string_view value)
{
    static constexpr char Hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size());
    for (unsigned char character : value)
    {
        if (std::isalnum(character) || character == '-' || character == '_' ||
            character == '.' || character == '~')
        {
            encoded.push_back(static_cast<char>(character));
        }
        else
        {
            encoded.push_back('%');
            encoded.push_back(Hex[character >> 4]);
            encoded.push_back(Hex[character & 0x0F]);
        }
    }
    return encoded;
}

std::string Lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

template <std::size_t Size>
void CopyToBuffer(std::array<char, Size>& destination, std::string_view source)
{
    destination.fill('\0');
    const std::size_t count = std::min(source.size(), destination.size() - 1);
    std::copy_n(source.data(), count, destination.data());
}

std::optional<std::string> DownloadHttps(std::string_view url, std::string& error)
{
    constexpr std::string_view httpsPrefix = "https://";
    if (!url.starts_with(httpsPrefix))
    {
        error = "The artwork service returned a non-HTTPS URL.";
        return std::nullopt;
    }

    const std::size_t hostStart = httpsPrefix.size();
    const std::size_t pathStart = url.find('/', hostStart);
    const std::string host(url.substr(hostStart, pathStart - hostStart));
    const std::string path(pathStart == std::string_view::npos ? "/" : url.substr(pathStart));
    httplib::SSLClient client(host);
    client.set_connection_timeout(5, 0);
    client.set_read_timeout(15, 0);
    client.set_follow_location(true);
    const auto response = client.Get(path);
    if (!response)
    {
        error = "Could not reach the cover service: " + httplib::to_string(response.error()) + ".";
        return std::nullopt;
    }
    if (response->status != 200)
    {
        const Json responseJson = Json::parse(response->body, nullptr, false);
        const std::string providerMessage = !responseJson.is_discarded()
            ? responseJson.value("status", "")
            : "";
        error = providerMessage.empty()
            ? "The cover service returned HTTP " + std::to_string(response->status) + "."
            : "TheGamesDB: " + providerMessage + " (HTTP " +
                std::to_string(response->status) + ").";
        return std::nullopt;
    }
    if (response->body.size() > 15 * 1024 * 1024)
    {
        error = "The downloaded cover is too large.";
        return std::nullopt;
    }
    return response->body;
}

CoverDownloadResult DownloadCoverFromTheGamesDb(const GameLibrary& library,
                                                const std::filesystem::path& romPath,
                                                const std::string& title,
                                                const CoverServiceConfig& config)
{
    CoverDownloadResult result;
    result.romPath = romPath;
    result.cachePath = CachedCoverPath(library, romPath);
    if (config.theGamesDbApiKey.empty())
    {
        result.message = "Enter and save a TheGamesDB API key first.";
        return result;
    }

    std::string error;
    const std::string searchUrl = "https://api.thegamesdb.net/v1.1/Games/ByGameName?apikey=" +
        UrlEncode(config.theGamesDbApiKey) + "&name=" + UrlEncode(title) +
        "&include=boxart,platform";
    const std::optional<std::string> responseBody = DownloadHttps(searchUrl, error);
    if (!responseBody)
    {
        result.message = error;
        return result;
    }

    try
    {
        const Json response = Json::parse(*responseBody);
        const Json games = response.at("data").at("games");
        if (!games.is_array() || games.empty())
        {
            result.message = "TheGamesDB did not find " + title + ".";
            return result;
        }

        const auto isNesGame = [&response](const Json& game)
        {
            const std::string platformId = std::to_string(game.value("platform", 0));
            const Json& platforms = response.at("include").at("platform").at("data");
            const auto platform = platforms.find(platformId);
            return platform != platforms.end() &&
                Lowercase(platform->value("name", "")).find("nintendo entertainment system") !=
                    std::string::npos;
        };

        const Json* chosenGame = &games.front();
        const Json* exactTitleGame = nullptr;
        const std::string normalizedTitle = Lowercase(title);
        for (const Json& game : games)
        {
            if (Lowercase(game.value("game_title", "")) == normalizedTitle)
            {
                exactTitleGame = &game;
                if (isNesGame(game))
                {
                    chosenGame = &game;
                    break;
                }
            }
            if (isNesGame(game))
            {
                chosenGame = &game;
            }
        }
        if (exactTitleGame && !isNesGame(*chosenGame))
        {
            chosenGame = exactTitleGame;
        }
        const std::string gameId = std::to_string(chosenGame->at("id").get<int>());
        const Json& boxart = response.at("include").at("boxart");
        const Json& images = boxart.at("data").at(gameId);
        if (!images.is_array() || images.empty())
        {
            result.message = "TheGamesDB has no box art for " + title + ".";
            return result;
        }

        const Json* chosenImage = &images.front();
        for (const Json& image : images)
        {
            if (image.value("type", "") == "boxart" && image.value("side", "") == "front")
            {
                chosenImage = &image;
                break;
            }
        }
        const std::string imageUrl = boxart.at("base_url").at("original").get<std::string>() +
            chosenImage->at("filename").get<std::string>();
        const std::optional<std::string> image = DownloadHttps(imageUrl, error);
        if (!image)
        {
            result.message = error;
            return result;
        }

        std::error_code filesystemError;
        std::filesystem::create_directories(result.cachePath.parent_path(), filesystemError);
        if (filesystemError)
        {
            result.message = "Could not create the local cover cache.";
            return result;
        }
        std::ofstream output(result.cachePath, std::ios::binary | std::ios::trunc);
        output.write(image->data(), static_cast<std::streamsize>(image->size()));
        if (!output)
        {
            result.message = "Could not save the downloaded cover.";
            return result;
        }
        result.downloaded = true;
        result.message = "Downloaded cover for " + title + ".";
    }
    catch (const Json::exception&)
    {
        result.message = "TheGamesDB returned an unexpected response.";
    }
    return result;
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
    const std::filesystem::path cachedCover = CachedCoverPath(library, romPath);
    return std::filesystem::is_regular_file(cachedCover) ? cachedCover : std::filesystem::path{};
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
            game.coverTexture = LoadCoverTexture(renderer, coverPath,
                                                 &game.coverWidth, &game.coverHeight);
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

void SetControllerButton(dendyforge::Console& console, const ControllerBindings& bindings,
                         SDL_Keycode key, bool pressed)
{
    using Button = dendyforge::Controller::Button;
    if (key == bindings.up) { console.PrimaryController().SetButton(Button::Up, pressed); }
    if (key == bindings.down) { console.PrimaryController().SetButton(Button::Down, pressed); }
    if (key == bindings.left) { console.PrimaryController().SetButton(Button::Left, pressed); }
    if (key == bindings.right) { console.PrimaryController().SetButton(Button::Right, pressed); }
    if (key == bindings.select) { console.PrimaryController().SetButton(Button::Select, pressed); }
    if (key == bindings.start) { console.PrimaryController().SetButton(Button::Start, pressed); }
    if (key == bindings.a) { console.PrimaryController().SetButton(Button::A, pressed); }
    if (key == bindings.b) { console.PrimaryController().SetButton(Button::B, pressed); }
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

bool ParseHexAddress(std::string_view text, std::uint16_t& address)
{
    unsigned int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    if (error != std::errc{} || end != text.data() + text.size() || value > 0xFFFF)
    {
        return false;
    }
    address = static_cast<std::uint16_t>(value);
    return true;
}

std::string FormatOperand(const dendyforge::CPU6502::OpcodeInfo& opcode,
                          std::uint16_t address, std::uint8_t low, std::uint8_t high)
{
    char text[16]{};
    const std::uint16_t word = static_cast<std::uint16_t>(low) |
                               (static_cast<std::uint16_t>(high) << 8);
    using AddressMode = dendyforge::CPU6502::AddressMode;
    switch (opcode.addressMode)
    {
    case AddressMode::Immediate: std::snprintf(text, sizeof(text), "#$%02X", low); break;
    case AddressMode::ZeroPage: std::snprintf(text, sizeof(text), "$%02X", low); break;
    case AddressMode::ZeroPageX: std::snprintf(text, sizeof(text), "$%02X,X", low); break;
    case AddressMode::ZeroPageY: std::snprintf(text, sizeof(text), "$%02X,Y", low); break;
    case AddressMode::Relative:
        std::snprintf(text, sizeof(text), "$%04X", static_cast<std::uint16_t>(
            address + 2 + static_cast<std::int8_t>(low)));
        break;
    case AddressMode::Absolute: std::snprintf(text, sizeof(text), "$%04X", word); break;
    case AddressMode::AbsoluteX: std::snprintf(text, sizeof(text), "$%04X,X", word); break;
    case AddressMode::AbsoluteY: std::snprintf(text, sizeof(text), "$%04X,Y", word); break;
    case AddressMode::Indirect: std::snprintf(text, sizeof(text), "($%04X)", word); break;
    case AddressMode::IndexedIndirect: std::snprintf(text, sizeof(text), "($%02X,X)", low); break;
    case AddressMode::IndirectIndexed: std::snprintf(text, sizeof(text), "($%02X),Y", low); break;
    case AddressMode::Implied: break;
    }
    return text;
}

void DrawCpuRegisters(const dendyforge::CPU6502& cpu)
{
    ImGui::Text("A  %02X", cpu.Accumulator());
    ImGui::SameLine(104.0f);
    ImGui::Text("X  %02X", cpu.X());
    ImGui::SameLine(196.0f);
    ImGui::Text("Y  %02X", cpu.Y());
    ImGui::SameLine(288.0f);
    ImGui::Text("SP  %02X", cpu.StackPointer());
    ImGui::Text("PC  %04X", cpu.ProgramCounter());
    ImGui::SameLine(150.0f);
    ImGui::Text("OP  %02X  %s", cpu.Opcode(), cpu.CurrentInstruction());
    ImGui::SameLine(355.0f);
    ImGui::Text("Cycles  %u", cpu.Cycles());

    ImGui::TextUnformatted("Flags");
    constexpr std::array flagNames{"N", "V", "U", "B", "D", "I", "Z", "C"};
    const std::uint8_t status = cpu.Status();
    for (std::size_t index = 0; index < flagNames.size(); ++index)
    {
        ImGui::SameLine();
        const bool active = (status & (0x80 >> index)) != 0;
        ImGui::TextColored(active ? ImVec4(0.36f, 0.90f, 0.58f, 1.0f)
                                  : ImVec4(0.42f, 0.47f, 0.57f, 1.0f),
                           "%s", flagNames[index]);
    }
}

void DrawDisassembly(dendyforge::Console& console)
{
    const std::uint16_t programCounter = console.Cpu().ProgramCounter();
    std::uint16_t address = static_cast<std::uint16_t>(programCounter - 16);
    for (int line = 0; line < 15; ++line)
    {
        const auto byteAt = [&console](std::uint16_t value) {
            return console.DebugPeekCpu(value).value_or(0);
        };
        const std::uint8_t instructionByte = byteAt(address);
        const auto opcode = dendyforge::CPU6502::DescribeOpcode(instructionByte);
        const std::uint8_t low = byteAt(static_cast<std::uint16_t>(address + 1));
        const std::uint8_t high = byteAt(static_cast<std::uint16_t>(address + 2));
        const std::string operand = FormatOperand(opcode, address, low, high);
        const bool current = address == programCounter;
        if (current)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.77f, 0.31f, 1.0f));
        }
        ImGui::Text("%04X  %02X %02X %02X  %-3s %s", address, instructionByte, low, high,
                    opcode.mnemonic, operand.c_str());
        if (current)
        {
            ImGui::PopStyleColor();
        }
        address = static_cast<std::uint16_t>(address + opcode.bytes);
    }
}

void DrawMemoryView(dendyforge::Console& console, DebuggerState& debugger)
{
    if (ImGui::InputText("Go to address", debugger.memoryAddressText.data(),
                         debugger.memoryAddressText.size(), ImGuiInputTextFlags_CharsHexadecimal |
                         ImGuiInputTextFlags_EnterReturnsTrue))
    {
        ParseHexAddress(debugger.memoryAddressText.data(), debugger.memoryAddress);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Unavailable I/O reads are --");
    ImGui::BeginChild("Memory bytes", ImVec2(0.0f, 235.0f), ImGuiChildFlags_Borders);
    for (int row = 0; row < 16; ++row)
    {
        const std::uint16_t rowAddress = static_cast<std::uint16_t>(
            debugger.memoryAddress + row * 16);
        ImGui::Text("%04X", rowAddress);
        std::string ascii;
        for (int column = 0; column < 16; ++column)
        {
            const auto value = console.DebugPeekCpu(static_cast<std::uint16_t>(rowAddress + column));
            ImGui::SameLine(62.0f + column * 25.0f);
            if (value)
            {
                ImGui::Text("%02X", *value);
                ascii.push_back(*value >= 32 && *value <= 126 ? static_cast<char>(*value) : '.');
            }
            else
            {
                ImGui::TextDisabled("--");
                ascii.push_back('.');
            }
        }
        ImGui::SameLine(478.0f);
        ImGui::TextDisabled("%s", ascii.c_str());
    }
    ImGui::EndChild();
}

void DrawBreakpoints(DebuggerState& debugger)
{
    if (ImGui::InputText("Address", debugger.breakpointText.data(), debugger.breakpointText.size(),
                         ImGuiInputTextFlags_CharsHexadecimal |
                         ImGuiInputTextFlags_EnterReturnsTrue))
    {
        std::uint16_t address = 0;
        if (ParseHexAddress(debugger.breakpointText.data(), address) &&
            std::find(debugger.breakpoints.begin(), debugger.breakpoints.end(), address) ==
                debugger.breakpoints.end())
        {
            debugger.breakpoints.push_back(address);
        }
        debugger.breakpointText.fill('\0');
    }
    ImGui::SameLine();
    if (ImGui::Button("Add breakpoint"))
    {
        std::uint16_t address = 0;
        if (ParseHexAddress(debugger.breakpointText.data(), address) &&
            std::find(debugger.breakpoints.begin(), debugger.breakpoints.end(), address) ==
                debugger.breakpoints.end())
        {
            debugger.breakpoints.push_back(address);
            debugger.breakpointText.fill('\0');
        }
    }
    for (std::size_t index = 0; index < debugger.breakpoints.size(); ++index)
    {
        ImGui::PushID(static_cast<int>(index));
        ImGui::Text("$%04X", debugger.breakpoints[index]);
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove"))
        {
            debugger.breakpoints.erase(debugger.breakpoints.begin() + index);
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
}

void DrawDebugger(dendyforge::Console& console, DebuggerState& debugger,
                  const InterfaceFonts& fonts)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 18.0f, viewport->WorkPos.y + 18.0f),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(std::min(620.0f, viewport->WorkSize.x - 36.0f),
                                    viewport->WorkSize.y - 36.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.96f);
    bool visible = debugger.visible;
    ImGui::Begin("DendyForge Debugger", &visible,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    if (!visible)
    {
        debugger.visible = false;
        debugger.paused = false;
        debugger.pauseRequested = false;
        ImGui::End();
        return;
    }

    ImGui::PushFont(fonts.display, 22.0f);
    ImGui::TextUnformatted("CPU debugger");
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextColored(debugger.paused ? ImVec4(1.0f, 0.71f, 0.25f, 1.0f)
                                       : ImVec4(0.32f, 0.88f, 0.56f, 1.0f),
                       "%s", debugger.paused ? "PAUSED" : "RUNNING");
    if (ImGui::Button(debugger.paused ? "Resume (F5)" : "Pause (F5)"))
    {
        if (debugger.paused)
        {
            debugger.ignoredBreakpoint = console.Cpu().ProgramCounter();
            debugger.paused = false;
        }
        else
        {
            debugger.pauseRequested = true;
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!debugger.paused);
    if (ImGui::Button("Step instruction (F10)"))
    {
        debugger.stepRequested = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("F1 closes");

    ImGui::SeparatorText("Registers");
    DrawCpuRegisters(console.Cpu());
    ImGui::SeparatorText("Disassembly");
    ImGui::BeginChild("Disassembly", ImVec2(0.0f, 272.0f), ImGuiChildFlags_Borders);
    DrawDisassembly(console);
    ImGui::EndChild();
    ImGui::SeparatorText("Memory");
    DrawMemoryView(console, debugger);
    ImGui::SeparatorText("Breakpoints");
    DrawBreakpoints(debugger);
    ImGui::End();
}

LibraryUiAction DrawGameLibrary(GameLibrary& library,
                                std::array<char, 128>& searchBuffer,
                                const InterfaceFonts& fonts)
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
    ImGui::PushFont(fonts.display, 31.0f);
    ImGui::TextUnformatted("DENDYFORGE");
    ImGui::PopFont();
    ImGui::TextDisabled("Your cartridge library");
    ImGui::SameLine(contentWidth - 305.0f);
    const bool settingsRequested = ImGui::Button("Settings");
    ImGui::SameLine(contentWidth - 210.0f);
    ImGui::SetNextItemWidth(210.0f);
    ImGui::InputTextWithHint("##search", "Search games...", searchBuffer.data(), searchBuffer.size());

    ImGui::SetCursorPos(ImVec2(36.0f, 111.0f));
    ImGui::PushFont(fonts.display, 24.0f);
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
        ImGui::PushFont(fonts.display, 21.0f);
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
            gridDrawList->AddRectFilled(coverTopLeft, coverBottomRight,
                                        IM_COL32(8, 12, 21, 255), 9.0f);
            if (library.games[index].coverTexture)
            {
                const float availableWidth = coverBottomRight.x - coverTopLeft.x;
                const float availableHeight = coverBottomRight.y - coverTopLeft.y;
                const float scale = std::min(
                    availableWidth / static_cast<float>(library.games[index].coverWidth),
                    availableHeight / static_cast<float>(library.games[index].coverHeight));
                const ImVec2 imageSize(library.games[index].coverWidth * scale,
                                      library.games[index].coverHeight * scale);
                const ImVec2 imageTopLeft(
                    coverTopLeft.x + (availableWidth - imageSize.x) * 0.5f,
                    coverTopLeft.y + (availableHeight - imageSize.y) * 0.5f);
                gridDrawList->AddImage(ImTextureRef(library.games[index].coverTexture),
                                       imageTopLeft,
                                       ImVec2(imageTopLeft.x + imageSize.x,
                                              imageTopLeft.y + imageSize.y));
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
    return {selectedGame, refreshRequested, settingsRequested};
}

void DrawCenteredText(ImDrawList* drawList, const ImVec2& centre, std::string_view text,
                      ImU32 colour)
{
    const ImVec2 size = ImGui::CalcTextSize(text.data(), text.data() + text.size());
    drawList->AddText(ImVec2(centre.x - size.x * 0.5f, centre.y - size.y * 0.5f),
                      colour, text.data(), text.data() + text.size());
}

std::optional<ControlAction> DrawDendyController(const ImVec2& topLeft,
                                                  const ControllerBindings& bindings,
                                                  std::optional<ControlAction> capturing)
{
    struct HitBox
    {
        ControlAction action;
        ImVec2 minimum;
        ImVec2 maximum;
    };

    const float x = topLeft.x;
    const float y = topLeft.y;
    const std::array<HitBox, 8> hitBoxes{{
        {ControlAction::Up, {x + 116.0f, y + 54.0f}, {x + 186.0f, y + 116.0f}},
        {ControlAction::Down, {x + 116.0f, y + 168.0f}, {x + 186.0f, y + 230.0f}},
        {ControlAction::Left, {x + 58.0f, y + 112.0f}, {x + 120.0f, y + 174.0f}},
        {ControlAction::Right, {x + 182.0f, y + 112.0f}, {x + 244.0f, y + 174.0f}},
        {ControlAction::Select, {x + 314.0f, y + 136.0f}, {x + 424.0f, y + 181.0f}},
        {ControlAction::Start, {x + 465.0f, y + 136.0f}, {x + 575.0f, y + 181.0f}},
        {ControlAction::B, {x + 633.0f, y + 103.0f}, {x + 721.0f, y + 191.0f}},
        {ControlAction::A, {x + 739.0f, y + 103.0f}, {x + 827.0f, y + 191.0f}},
    }};

    std::array<bool, 8> hovered{};
    std::optional<ControlAction> selected;
    ImGui::PushID("Dendy controller mapping");
    for (std::size_t index = 0; index < hitBoxes.size(); ++index)
    {
        const HitBox& hitBox = hitBoxes[index];
        ImGui::SetCursorScreenPos(hitBox.minimum);
        ImGui::PushID(static_cast<int>(hitBox.action));
        const bool pressed = ImGui::InvisibleButton(
            "bind", ImVec2(hitBox.maximum.x - hitBox.minimum.x,
                             hitBox.maximum.y - hitBox.minimum.y));
        hovered[index] = ImGui::IsItemHovered();
        ImGui::PopID();
        if (pressed)
        {
            selected = hitBox.action;
        }
    }
    ImGui::PopID();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 bodyMin(x, y + 16.0f);
    const ImVec2 bodyMax(x + 875.0f, y + 274.0f);
    drawList->AddRectFilled(ImVec2(bodyMin.x + 5.0f, bodyMin.y + 10.0f),
                            ImVec2(bodyMax.x + 5.0f, bodyMax.y + 10.0f),
                            IM_COL32(0, 0, 0, 80), 28.0f);
    drawList->AddRectFilled(bodyMin, bodyMax, IM_COL32(231, 232, 226, 255), 28.0f);
    drawList->AddRect(bodyMin, bodyMax, IM_COL32(102, 115, 128, 255), 28.0f, 0, 4.0f);
    drawList->AddLine(ImVec2(x + 25.0f, y + 214.0f), ImVec2(x + 850.0f, y + 214.0f),
                      IM_COL32(25, 59, 98, 210), 3.0f);
    drawList->AddLine(ImVec2(x + 25.0f, y + 221.0f), ImVec2(x + 850.0f, y + 221.0f),
                      IM_COL32(25, 59, 98, 210), 3.0f);

    drawList->AddText(ImVec2(x + 70.0f, y + 42.0f), IM_COL32(183, 31, 40, 255), "Dendy");
    drawList->AddText(ImVec2(x + 83.0f, y + 61.0f), IM_COL32(183, 31, 40, 255), "JUNIOR");
    drawList->AddRectFilled(ImVec2(x + 104.0f, y + 45.0f), ImVec2(x + 198.0f, y + 239.0f),
                            IM_COL32(45, 49, 53, 255), 11.0f);
    drawList->AddRectFilled(ImVec2(x + 49.0f, y + 101.0f), ImVec2(x + 253.0f, y + 184.0f),
                            IM_COL32(45, 49, 53, 255), 11.0f);
    drawList->AddCircleFilled(ImVec2(x + 151.0f, y + 142.0f), 19.0f, IM_COL32(30, 34, 38, 255));

    drawList->AddRectFilled(ImVec2(x + 287.0f, y + 119.0f), ImVec2(x + 602.0f, y + 196.0f),
                            IM_COL32(88, 93, 97, 255), 38.0f);
    drawList->AddRectFilled(ImVec2(x + 305.0f, y + 130.0f), ImVec2(x + 438.0f, y + 187.0f),
                            IM_COL32(45, 49, 53, 255), 27.0f);
    drawList->AddRectFilled(ImVec2(x + 452.0f, y + 130.0f), ImVec2(x + 584.0f, y + 187.0f),
                            IM_COL32(45, 49, 53, 255), 27.0f);
    drawList->AddText(ImVec2(x + 322.0f, y + 94.0f), IM_COL32(23, 56, 94, 255), "SELECT");
    drawList->AddText(ImVec2(x + 486.0f, y + 94.0f), IM_COL32(23, 56, 94, 255), "START");
    drawList->AddCircleFilled(ImVec2(x + 677.0f, y + 147.0f), 53.0f, IM_COL32(48, 52, 56, 255));
    drawList->AddCircleFilled(ImVec2(x + 783.0f, y + 147.0f), 53.0f, IM_COL32(48, 52, 56, 255));
    drawList->AddText(ImVec2(x + 663.0f, y + 211.0f), IM_COL32(23, 56, 94, 255), "B");
    drawList->AddText(ImVec2(x + 770.0f, y + 211.0f), IM_COL32(23, 56, 94, 255), "A");

    for (std::size_t index = 0; index < hitBoxes.size(); ++index)
    {
        const HitBox& hitBox = hitBoxes[index];
        const bool active = capturing && *capturing == hitBox.action;
        const ImU32 outline = active ? IM_COL32(255, 175, 57, 255)
            : hovered[index] ? IM_COL32(110, 201, 255, 255) : IM_COL32(255, 255, 255, 40);
        drawList->AddRect(hitBox.minimum, hitBox.maximum, outline, 9.0f, 0, active ? 3.0f : 1.0f);
        DrawCenteredText(drawList,
                         ImVec2((hitBox.minimum.x + hitBox.maximum.x) * 0.5f,
                                (hitBox.minimum.y + hitBox.maximum.y) * 0.5f),
                         BindingName(BindingFor(bindings, hitBox.action)),
                         active ? IM_COL32(255, 215, 141, 255) : IM_COL32(245, 247, 250, 255));
    }
    ImGui::SetCursorScreenPos(ImVec2(x, y + 290.0f));
    ImGui::Dummy(ImVec2(875.0f, 1.0f));
    return selected;
}

SettingsUiAction DrawSettings(GameLibrary& library, const CoverServiceConfig& config,
                              std::array<char, 128>& apiKeyBuffer,
                              std::string_view coverStatus, bool downloadInProgress,
                              bool& showCoverSettings,
                              std::optional<ControlAction> capturing,
                              const InterfaceFonts& fonts)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("Settings", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 position = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    drawList->AddRectFilledMultiColor(position, ImVec2(position.x + size.x, position.y + size.y),
                                      IM_COL32(10, 15, 27, 255), IM_COL32(18, 32, 58, 255),
                                      IM_COL32(8, 13, 24, 255), IM_COL32(12, 20, 37, 255));

    SettingsUiAction action;
    ImGui::SetCursorPos(ImVec2(36.0f, 28.0f));
    action.close = ImGui::Button("< Back to library");
    ImGui::SameLine();
    ImGui::PushFont(fonts.display, 27.0f);
    ImGui::TextUnformatted("Settings");
    ImGui::PopFont();

    ImGui::SetCursorPos(ImVec2(36.0f, 86.0f));
    if (ImGui::Button("Controls", ImVec2(130.0f, 36.0f)))
    {
        showCoverSettings = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cover service", ImVec2(150.0f, 36.0f)))
    {
        showCoverSettings = true;
    }
    ImGui::SetCursorPos(ImVec2(36.0f, 143.0f));
    ImGui::BeginChild("Settings content", ImVec2(size.x - 72.0f, size.y - 177.0f),
                      ImGuiChildFlags_Borders);

    if (!showCoverSettings)
    {
        ImGui::SetCursorPos(ImVec2(28.0f, 24.0f));
        ImGui::PushFont(fonts.display, 22.0f);
        ImGui::TextUnformatted("Keyboard mapping");
        ImGui::PopFont();
        ImGui::SetCursorPos(ImVec2(28.0f, 55.0f));
        ImGui::TextDisabled(capturing
            ? "Press a key now. Esc cancels the change."
            : "Click a controller button, then press the keyboard key you want to use.");
        const ImVec2 contentPosition = ImGui::GetWindowPos();
        const ImVec2 contentSize = ImGui::GetWindowSize();
        const ImVec2 controllerPosition(contentPosition.x + (contentSize.x - 875.0f) * 0.5f,
                                        contentPosition.y + 92.0f);
        action.selectControl = DrawDendyController(controllerPosition, config.controller, capturing);
        ImGui::SetCursorPos(ImVec2(28.0f, 390.0f));
        action.resetControls = ImGui::Button("Reset defaults");
        ImGui::SameLine();
        action.save = ImGui::Button("Save settings");
    }
    else
    {
        int missingCoverCount = 0;
        for (const GameEntry& game : library.games)
        {
            missingCoverCount += game.coverTexture == nullptr;
        }
        ImGui::SetCursorPos(ImVec2(28.0f, 24.0f));
        ImGui::PushFont(fonts.display, 22.0f);
        ImGui::TextUnformatted("Automatic covers");
        ImGui::PopFont();
        ImGui::SetCursorPos(ImVec2(28.0f, 57.0f));
        ImGui::TextDisabled("TheGamesDB key is saved locally and is never added to Git.");
        ImGui::SetCursorPos(ImVec2(28.0f, 102.0f));
        ImGui::SetNextItemWidth(330.0f);
        ImGui::InputTextWithHint("##thegamesdb-key", "TheGamesDB API key", apiKeyBuffer.data(),
                                 apiKeyBuffer.size(), ImGuiInputTextFlags_Password);
        ImGui::SameLine();
        action.save = ImGui::Button("Save settings");
        ImGui::SetCursorPos(ImVec2(28.0f, 151.0f));
        ImGui::TextDisabled("%.*s", static_cast<int>(coverStatus.size()), coverStatus.data());
        ImGui::SetCursorPos(ImVec2(28.0f, 194.0f));
        ImGui::BeginDisabled(downloadInProgress || missingCoverCount == 0);
        action.downloadMissingCovers = ImGui::Button(downloadInProgress
            ? "Downloading covers..."
            : "Download missing covers");
        ImGui::EndDisabled();
    }

    ImGui::EndChild();
    ImGui::End();
    return action;
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
    const InterfaceFonts interfaceFonts = LoadInterfaceFonts();
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
    CoverServiceConfig coverServiceConfig = LoadCoverServiceConfig(library);
    std::array<char, 128> apiKeyBuffer{};
    CopyToBuffer(apiKeyBuffer, coverServiceConfig.theGamesDbApiKey);
    std::deque<std::size_t> pendingCoverDownloads;
    std::future<CoverDownloadResult> activeCoverDownload;
    std::string coverStatus = coverServiceConfig.theGamesDbApiKey.empty()
        ? "Save a TheGamesDB API key to fetch real covers automatically."
        : "Ready to download missing covers.";
    bool settingsOpen = false;
    bool showCoverSettings = false;
    std::optional<ControlAction> capturingControl;
    std::unique_ptr<dendyforge::Console> console;
    std::filesystem::path activeBatterySavePath;
    bool batterySaveWritable = true;
    std::vector<float> pendingAudio;
    bool gameRunning = false;
    bool hasGameFrame = false;
    bool zapperCrosshair = false;
    DebuggerState debugger;
    std::uint64_t previousTicks = SDL_GetTicksNS();
    double pendingCpuCycles = 0.0;

    auto startNextCoverDownload = [&]()
    {
        if (activeCoverDownload.valid() || pendingCoverDownloads.empty())
        {
            return;
        }
        const std::size_t gameIndex = pendingCoverDownloads.front();
        pendingCoverDownloads.pop_front();
        if (gameIndex >= library.games.size() || library.games[gameIndex].coverTexture)
        {
            return;
        }
        const std::filesystem::path romPath = library.games[gameIndex].romPath;
        const std::string title = library.games[gameIndex].title;
        activeCoverDownload = std::async(std::launch::async,
            [&library, romPath, title, coverServiceConfig]()
            {
                return DownloadCoverFromTheGamesDb(library, romPath, title, coverServiceConfig);
            });
    };

    const auto saveActiveBatteryRam = [&]()
    {
        if (console && batterySaveWritable && !activeBatterySavePath.empty())
        {
            SaveBatterySave(*console, activeBatterySavePath);
        }
    };

    auto returnToLibrary = [&]()
    {
        saveActiveBatteryRam();
        gameRunning = false;
        hasGameFrame = false;
        debugger = {};
        console.reset();
        activeBatterySavePath.clear();
        batterySaveWritable = true;
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
        newConsole->SecondaryZapper().SetConnected(zapperCrosshair);
        const std::filesystem::path savePath = SavePathForRom(romPath);
        const bool saveLoadedOrAbsent = LoadBatterySave(*newConsole, savePath);
        console = std::move(newConsole);
        activeBatterySavePath = console->HasBatteryBackedPrgRam() ? savePath
                                                                    : std::filesystem::path{};
        batterySaveWritable = saveLoadedOrAbsent;
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
        hasGameFrame = false;
        debugger = {};
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
            else if (gameRunning && event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F1)
            {
                debugger.visible = !debugger.visible;
                if (debugger.visible)
                {
                    debugger.pauseRequested = true;
                }
                else
                {
                    debugger.paused = false;
                    debugger.pauseRequested = false;
                }
            }
            else if (gameRunning && debugger.visible && event.type == SDL_EVENT_KEY_DOWN &&
                     event.key.key == SDLK_F5)
            {
                if (debugger.paused)
                {
                    debugger.ignoredBreakpoint = console->Cpu().ProgramCounter();
                    debugger.paused = false;
                }
                else
                {
                    debugger.pauseRequested = true;
                }
            }
            else if (gameRunning && debugger.visible && event.type == SDL_EVENT_KEY_DOWN &&
                     event.key.key == SDLK_F10 && debugger.paused)
            {
                debugger.stepRequested = true;
            }
            else if (!gameRunning && settingsOpen && event.type == SDL_EVENT_KEY_DOWN &&
                     capturingControl)
            {
                if (event.key.key == SDLK_ESCAPE)
                {
                    capturingControl.reset();
                    coverStatus = "Control mapping cancelled.";
                }
                else
                {
                    BindingFor(coverServiceConfig.controller, *capturingControl) = event.key.key;
                    coverStatus = std::string(ControlName(*capturingControl)) + " mapped to " +
                        BindingName(event.key.key) + ". Click Save settings to keep it.";
                    capturingControl.reset();
                }
            }
            else if (!gameRunning && settingsOpen && event.type == SDL_EVENT_KEY_DOWN &&
                     event.key.key == SDLK_ESCAPE)
            {
                settingsOpen = false;
            }
            else if (gameRunning && console &&
                     (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) &&
                     !(debugger.visible && ImGui::GetIO().WantCaptureKeyboard))
            {
                SetControllerButton(*console, coverServiceConfig.controller, event.key.key,
                                    event.type == SDL_EVENT_KEY_DOWN);
            }
        }

        if (!gameRunning)
        {
            if (activeCoverDownload.valid() &&
                activeCoverDownload.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                try
                {
                    const CoverDownloadResult result = activeCoverDownload.get();
                    coverStatus = result.message;
                    if (result.downloaded)
                    {
                        for (GameEntry& game : library.games)
                        {
                            if (game.romPath == result.romPath)
                            {
                                SDL_DestroyTexture(game.coverTexture);
                                game.coverTexture = LoadCoverTexture(renderer, result.cachePath,
                                                                     &game.coverWidth,
                                                                     &game.coverHeight);
                                break;
                            }
                        }
                    }
                }
                catch (const std::exception& exception)
                {
                    coverStatus = std::string("Cover download failed: ") + exception.what();
                }
            }
            startNextCoverDownload();

            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            LibraryUiAction libraryAction;
            SettingsUiAction settingsAction;
            if (settingsOpen)
            {
                settingsAction = DrawSettings(
                    library, coverServiceConfig, apiKeyBuffer, coverStatus,
                    activeCoverDownload.valid() || !pendingCoverDownloads.empty(),
                    showCoverSettings, capturingControl, interfaceFonts);
            }
            else
            {
                libraryAction = DrawGameLibrary(library, searchBuffer, interfaceFonts);
            }
            ImGui::Render();
            SDL_SetRenderDrawColor(renderer, 10, 15, 27, 255);
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
            SDL_RenderPresent(renderer);

            if (libraryAction.openSettings)
            {
                settingsOpen = true;
                capturingControl.reset();
            }
            if (settingsAction.close)
            {
                settingsOpen = false;
                capturingControl.reset();
            }
            if (settingsAction.selectControl)
            {
                capturingControl = settingsAction.selectControl;
                coverStatus = std::string("Mapping ") + ControlName(*capturingControl) +
                    ": press a keyboard key.";
            }
            if (settingsAction.resetControls)
            {
                coverServiceConfig.controller = {};
                capturingControl.reset();
                coverStatus = "Default controls restored. Click Save settings to keep them.";
            }
            if (settingsAction.save)
            {
                coverServiceConfig.theGamesDbApiKey = apiKeyBuffer.data();
                coverStatus = SaveCoverServiceConfig(library, coverServiceConfig)
                    ? "Settings saved locally."
                    : "Could not save the settings.";
            }
            if (settingsAction.downloadMissingCovers && !activeCoverDownload.valid())
            {
                if (coverServiceConfig.theGamesDbApiKey.empty())
                {
                    coverStatus = "Enter and save a TheGamesDB API key first.";
                }
                else
                {
                    for (std::size_t index = 0; index < library.games.size(); ++index)
                    {
                        if (!library.games[index].coverTexture &&
                            FindCover(library, library.games[index].romPath).empty())
                        {
                            pendingCoverDownloads.push_back(index);
                        }
                    }
                    coverStatus = pendingCoverDownloads.empty()
                        ? "Every game already has a cover."
                        : "Downloading covers in the background...";
                }
            }
            if (libraryAction.refresh && !activeCoverDownload.valid() && pendingCoverDownloads.empty())
            {
                RefreshLibrary(library, renderer);
            }
            if (libraryAction.selectedGame)
            {
                launchGame(library.games[*libraryAction.selectedGame].romPath);
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

        bool frameComplete = false;
        const auto consumeFrameComplete = [&]()
        {
            frameComplete = console->VideoProcessor().ConsumeFrameComplete() || frameComplete;
        };
        const auto breakpointReached = [&]()
        {
            const std::uint16_t programCounter = console->Cpu().ProgramCounter();
            if (debugger.ignoredBreakpoint && *debugger.ignoredBreakpoint != programCounter)
            {
                debugger.ignoredBreakpoint.reset();
            }
            return console->IsInstructionBoundary() && !debugger.ignoredBreakpoint &&
                std::find(debugger.breakpoints.begin(), debugger.breakpoints.end(), programCounter) !=
                    debugger.breakpoints.end();
        };

        if (debugger.pauseRequested)
        {
            while (!console->IsInstructionBoundary())
            {
                console->Clock();
                consumeFrameComplete();
            }
            debugger.pauseRequested = false;
            debugger.paused = true;
        }

        if (debugger.paused && debugger.stepRequested)
        {
            console->StepInstruction();
            consumeFrameComplete();
            debugger.stepRequested = false;
        }

        const std::uint64_t currentTicks = SDL_GetTicksNS();
        if (!debugger.paused)
        {
            const std::uint64_t elapsedNanoseconds = std::min(
                currentTicks - previousTicks, MaximumElapsedNanoseconds);
            previousTicks = currentTicks;
            pendingCpuCycles += static_cast<double>(elapsedNanoseconds) * CpuClockHz /
                NanosecondsPerSecond;
            while (pendingCpuCycles >= 1.0 && !frameComplete)
            {
                if (breakpointReached())
                {
                    debugger.paused = true;
                    debugger.visible = true;
                    break;
                }
                console->Clock();
                pendingCpuCycles -= 1.0;
                consumeFrameComplete();
                if (breakpointReached())
                {
                    debugger.paused = true;
                    debugger.visible = true;
                    break;
                }
            }
        }
        else
        {
            previousTicks = currentTicks;
            pendingCpuCycles = 0.0;
        }

        if (audioStream)
        {
            if (debugger.paused)
            {
                pendingAudio.clear();
                audioPlaybackStarted = false;
                SDL_PauseAudioStreamDevice(audioStream);
                SDL_ClearAudioStream(audioStream);
            }
            else
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
        }

        if (frameComplete)
        {
            const auto& frameBuffer = console->VideoProcessor().FrameBuffer();
            SDL_UpdateTexture(gameTexture, nullptr, frameBuffer.data(),
                              ScreenWidth * static_cast<int>(sizeof(std::uint32_t)));
            hasGameFrame = true;
        }
        if (debugger.visible)
        {
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            DrawDebugger(*console, debugger, interfaceFonts);
            ImGui::Render();
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (hasGameFrame)
        {
            SDL_RenderTexture(renderer, gameTexture, nullptr, nullptr);
        }
        if (zapperCrosshair && logicalX >= 0.0f && logicalX < ScreenWidth &&
            logicalY >= 0.0f && logicalY < ScreenHeight)
        {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderLine(renderer, logicalX - 6.0f, logicalY, logicalX - 2.0f, logicalY);
            SDL_RenderLine(renderer, logicalX + 2.0f, logicalY, logicalX + 6.0f, logicalY);
            SDL_RenderLine(renderer, logicalX, logicalY - 6.0f, logicalX, logicalY - 2.0f);
            SDL_RenderLine(renderer, logicalX, logicalY + 2.0f, logicalX, logicalY + 6.0f);
        }
        if (debugger.visible)
        {
            SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        }
        SDL_RenderPresent(renderer);
        if (debugger.visible)
        {
            SDL_SetRenderLogicalPresentation(renderer, ScreenWidth, ScreenHeight,
                                             SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        }
        SDL_Delay(1);
    }

    saveActiveBatteryRam();
    console.reset();
    if (activeCoverDownload.valid())
    {
        activeCoverDownload.wait();
    }
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
