#pragma once

#include <string>

struct SDL_Surface;

bool saveSurfacePng(SDL_Surface* surface, const std::string& path);
