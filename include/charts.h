#pragma once

#include "common.h"
#include <SDL2/SDL.h>
#include <cstdint>
#include <string>

class Renderer;
struct FontSet;

int chartNaturalHeight(const Chart& chart);
int iconBlockNaturalHeight(Renderer* renderer, const FontSet& fonts);

void renderChart(Renderer* renderer, SDL_Surface* surface,
                 const Chart& chart, const FontSet& fonts,
                 int x, int y, int width, int height);

void renderIconBlock(Renderer* renderer, SDL_Surface* surface,
                     const IconBlock& icon, const FontSet& fonts,
                     int x, int y, int width);

uint32_t iconCodepoint(const std::string& name);
