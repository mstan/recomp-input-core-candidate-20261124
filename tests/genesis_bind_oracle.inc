/* Exact donor helpers; backend reads supplied by fake_sdl.c. */
static int player_deadzone(int player)
{
    int pct = g_input_map.p[player].deadzone_pct;
    if (pct < 0) pct = 0; else if (pct > 100) pct = 100;
    return pct * 32767 / 100;
}
static int bind_held(SDL_GameController *c, const GamepadBind *bind, int deadzone)
{
    if (!c) return 0;
    if (bind->kind == GP_BIND_BUTTON) {
        return SDL_GameControllerGetButton(c, (SDL_GameControllerButton)bind->code) ? 1 : 0;
    }
    if (bind->kind == GP_BIND_AXIS) {
        int v = SDL_GameControllerGetAxis(c, (SDL_GameControllerAxis)bind->code);
        return bind->axis_dir < 0 ? (v < -deadzone) : (v > deadzone);
    }
    return 0;
}
