#include "screenshot.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

int main() {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, 2, 3, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return 1;
    SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 20, 40, 60, 255));

    std::string path = (
        std::filesystem::temp_directory_path() /
        "presenter_test_screenshot.png").string();
    bool saved = saveSurfacePng(surface, path);
    SDL_FreeSurface(surface);
    if (!saved) return 2;

    std::ifstream file(path, std::ios::binary);
    std::vector<unsigned char> bytes(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    std::remove(path.c_str());

    const unsigned char signature[] = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
    };
    if (bytes.size() < 24) return 3;
    for (size_t i = 0; i < sizeof(signature); ++i) {
        if (bytes[i] != signature[i]) return 4;
    }
    if (bytes[16] != 0 || bytes[17] != 0 ||
        bytes[18] != 0 || bytes[19] != 2 ||
        bytes[20] != 0 || bytes[21] != 0 ||
        bytes[22] != 0 || bytes[23] != 3) {
        return 5;
    }

    fprintf(stderr, "PNG screenshot writer passed\n");
    return 0;
}
