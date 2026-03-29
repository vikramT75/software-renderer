#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include <stdexcept>
#include <cstring>

struct InputState
{
    // Keyboard
    bool w, a, s, d, q, e; // move forward/left/back/right/down/up
    bool shift;            // speed modifier

    // Mouse
    bool mouseDown;         // left button held
    float mouseDX, mouseDY; // delta this frame (pixels)
};

class SDLWindow
{
public:
    SDLWindow(const char *title, int width, int height)
        : m_width(width), m_height(height)
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
            throw std::runtime_error(SDL_GetError());

        m_window = SDL_CreateWindow(
            title,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height,
            SDL_WINDOW_SHOWN);
        if (!m_window)
            throw std::runtime_error(SDL_GetError());

        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
        if (!m_renderer)
            throw std::runtime_error(SDL_GetError());

        m_texture = SDL_CreateTexture(
            m_renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            width, height);
        if (!m_texture)
            throw std::runtime_error(SDL_GetError());

        m_pixelFormat = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
        std::memset(&m_input, 0, sizeof(m_input));
    }

    ~SDLWindow()
    {
        SDL_FreeFormat(m_pixelFormat);
        SDL_DestroyTexture(m_texture);
        SDL_DestroyRenderer(m_renderer);
        SDL_DestroyWindow(m_window);
        SDL_Quit();
    }

    // Returns false when the window should close.
    bool pollEvents()
    {
        // Reset per-frame deltas
        m_input.mouseDX = 0.f;
        m_input.mouseDY = 0.f;

        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                return false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
                return false;

            if (e.type == SDL_MOUSEMOTION && m_input.mouseDown)
            {
                m_input.mouseDX += static_cast<float>(e.motion.xrel);
                m_input.mouseDY += static_cast<float>(e.motion.yrel);
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
                m_input.mouseDown = true;
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
                m_input.mouseDown = false;
        }

        // Keyboard held-state (checked every frame, not event-driven)
        const uint8_t *keys = SDL_GetKeyboardState(nullptr);
        m_input.w = keys[SDL_SCANCODE_W];
        m_input.a = keys[SDL_SCANCODE_A];
        m_input.s = keys[SDL_SCANCODE_S];
        m_input.d = keys[SDL_SCANCODE_D];
        m_input.q = keys[SDL_SCANCODE_Q];
        m_input.e = keys[SDL_SCANCODE_E];
        m_input.shift = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];

        return true;
    }

    void present(const uint32_t *pixels)
    {
        SDL_UpdateTexture(m_texture, nullptr, pixels, m_width * sizeof(uint32_t));
        SDL_RenderClear(m_renderer);
        SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);
        SDL_RenderPresent(m_renderer);
    }

    const InputState &input() const { return m_input; }

    uint32_t mapRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) const
    {
        return SDL_MapRGBA(m_pixelFormat, r, g, b, a);
    }

    int width() const { return m_width; }
    int height() const { return m_height; }

    SDLWindow(const SDLWindow &) = delete;
    SDLWindow &operator=(const SDLWindow &) = delete;

private:
    int m_width, m_height;
    SDL_Window *m_window = nullptr;
    SDL_Renderer *m_renderer = nullptr;
    SDL_Texture *m_texture = nullptr;
    SDL_PixelFormat *m_pixelFormat = nullptr;
    InputState m_input;
};