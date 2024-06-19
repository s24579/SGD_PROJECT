#include <SDL.h>
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <vector>
#include <cstdlib>
#include <algorithm>

#define TILE_SIZE 64

struct game_map_t {
    int width, height;
    std::vector<int> map;
    int get(int x, int y) const  {
        if (x < 0) return 1;
        if (x >= width) return 1;
        if (y < 0) return 1;
        if (y >= height) return 1;
        return map[y*width+x];
    }
};

game_map_t game_map = {
        20, 10, {
                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
        }
};

std::shared_ptr<SDL_Texture> load_image(SDL_Renderer *renderer, const std::string &file_name) {
    SDL_Surface *surface;
    SDL_Texture *texture;
    surface = SDL_LoadBMP(file_name.c_str());
    if (!surface) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create surface from image: %s", SDL_GetError());
        throw std::invalid_argument(SDL_GetError());
    }
    SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format,0, 255, 255));
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture from surface: %s", SDL_GetError());
        throw std::invalid_argument(SDL_GetError());
    }
    SDL_FreeSurface(surface);
    return {texture, [](SDL_Texture *t) {
        SDL_DestroyTexture(t);
    }};
}

union vect_t {
    struct { double x; double y;} v;
    double p[2];
};

vect_t operator+(const vect_t a, const vect_t b) {
    vect_t ret = a;
    ret.v.x += b.v.x;
    ret.v.y += b.v.y;
    return ret;
}
vect_t operator*(const vect_t a, const double b) {
    vect_t ret = a;
    ret.v.x *= b;
    ret.v.y *= b;
    return ret;
}

struct player_t {
    vect_t p; ///< position
    vect_t v; ///< velocity
    vect_t a; ///< acceleration
};

struct bullet_t {
    vect_t p; ///< position
    vect_t v; ///< velocity
    vect_t a; ///< acceleration
};

bool is_in_collision(vect_t pos, const game_map_t &map) {
    return map.get(pos.v.x, pos.v.y) > 0;
}

bool is_on_the_ground(player_t player, const game_map_t &map) {
    return map.get(player.p.v.x, player.p.v.y+0.01) > 0;
}

player_t update_player(player_t player_old, const game_map_t &map, double dt) {
    player_t player = player_old;

    if (!is_on_the_ground (player_old, map)) player_old.a.v.y = 10; // gravity acceleration (in the air)

    player.p = player_old.p + (player_old.v * dt) + (player_old.a*dt*dt)*0.5;
    player.v = player_old.v + (player_old.a * dt);
    player.v =  player.v * 0.99;

    // Prevent the player from moving outside the screen boundaries
    if (player.p.v.x < 0.5) {
        player.p.v.x = 0.5;
        player.v.v.x = 0;
    } else if (player.p.v.x > 9.5) {
        player.p.v.x = 9.5;
        player.v.v.x = 0;
    }

    std::vector<vect_t> collision_points = {
            {v:{-0.4,0.0}},
            {v:{0.4,0.0}}
    };
    std::vector<vect_t> collision_mods = {
            {v:{0.0,-1.0}},
            {v:{0.0,-1.0}}
    };

    for (int i = 0; i < collision_points.size(); i++) {
        auto test_point = player.p + collision_points[i];

        if (is_in_collision(test_point, map)) {
            if(collision_mods[i].v.y < 0) { // need to lift the player
                player.v.v.y = 0;
                player.p.v.y = player_old.p.v.y;
            }
        }
    }

    return player;
}

bullet_t update_bullet(bullet_t bullet, double dt) {
    bullet.p = bullet.p + (bullet.v * dt) + (bullet.a * dt * dt) * 0.5;
    bullet.v = bullet.v + (bullet.a * dt);
    return bullet;
}

void draw_map(SDL_Renderer *renderer, game_map_t & map, std::shared_ptr<SDL_Texture> tex) {
    for (int y = map.height-1; y >= 0; y--)
        for (int x = 0; x < map.width; x++) {
            SDL_Rect dst = {x * TILE_SIZE, y*TILE_SIZE, TILE_SIZE*2,TILE_SIZE*2};
            if (map.get(x,y) > 0) {
                SDL_Rect src = {128*(map.get(x,y) - 1), 0, TILE_SIZE*2,TILE_SIZE*2};
                SDL_RenderCopy(renderer, tex.get(), &src, &dst);
            }
        }
}

void draw_bullets(SDL_Renderer *renderer, std::shared_ptr<SDL_Texture> tex, const std::vector<bullet_t>& bullets) {
    for (const auto& bullet : bullets) {
        SDL_Rect dst = {static_cast<int>(bullet.p.v.x), static_cast<int>(bullet.p.v.y), 64, 100}; // Adjust size as needed
        SDL_RenderCopy(renderer, tex.get(), NULL, &dst);
    }
}

int main(int argc, char *argv[]) {
    using namespace std::chrono_literals;
    using namespace std::chrono;
    using namespace std;
    SDL_Window *window;
    SDL_Renderer *renderer;

    double dt = 1./60.;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        return 3;
    }

    if (SDL_CreateWindowAndRenderer(640, 640, 0, &window, &renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window and renderer: %s", SDL_GetError());
        return 3;
    }

    auto player_texture = load_image(renderer, "player.bmp");
    auto tiles_texture = load_image(renderer, "tiles.bmp");
    auto background_texture = load_image(renderer, "background.bmp");
    auto bullet_texture = load_image(renderer, "bullet.bmp");

    bool still_playing = true;
    player_t player;
    player.p.v.x = 1;
    player.p.v.y = 1;
    player.a.v.x = 0;
    player.a.v.y = 0;
    player.v.v.x = 0;
    player.v.v.y = 0;

    std::vector<bullet_t> bullets;
    steady_clock::time_point last_bullet_spawn_time = steady_clock::now();

    double game_time = 0.;
    steady_clock::time_point current_time = steady_clock::now();

    int score = 0;

    while (still_playing) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    still_playing = false;
                    break;
                case SDL_KEYDOWN:
                    if (is_on_the_ground(player, game_map)) {
                        if (event.key.keysym.scancode == SDL_SCANCODE_UP) player.a.v.y = -500;
                        if (event.key.keysym.scancode == SDL_SCANCODE_LEFT) player.a.v.x = -5;
                        if (event.key.keysym.scancode == SDL_SCANCODE_RIGHT) player.a.v.x = 5;
                    }
                    break;
                case SDL_KEYUP:
                    if (event.key.keysym.scancode == SDL_SCANCODE_Q) still_playing = false;
                    if (event.key.keysym.scancode == SDL_SCANCODE_UP) player.a.v.y = 0;
                    if (event.key.keysym.scancode == SDL_SCANCODE_LEFT) player.a.v.x = 0;
                    if (event.key.keysym.scancode == SDL_SCANCODE_RIGHT) player.a.v.x = 0;
                    break;
            }
        }

        game_time += dt;
        player = update_player(player, game_map, dt);
        score += 1;//score counting

        // Bullet spawning logic
        steady_clock::time_point now = steady_clock::now();
        double time_since_last_bullet = duration_cast<seconds>(now - last_bullet_spawn_time).count();
        if (time_since_last_bullet >= (rand() % 3 + 1)) {
            bullet_t new_bullet;
            new_bullet.p.v.x = rand() % 640; // Random x position
            new_bullet.p.v.y = 0; // Start at the top of the screen
            new_bullet.v.v.x = (rand() % 100 - 50); // Random horizontal velocity
            new_bullet.v.v.y = (rand() % 75 + 15); // Random vertical velocity
            new_bullet.a.v.x = 0; // No horizontal acceleration
            new_bullet.a.v.y = (rand() % 50 + 15); // Random vertical acceleration
            bullets.push_back(new_bullet);
            last_bullet_spawn_time = now;
        }

        // Update bullets
        for (auto& bullet : bullets) {
            bullet = update_bullet(bullet, dt);
        }

        // Remove bullets that have fallen off the screen
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](const bullet_t& bullet) {
            return bullet.p.v.y > 640;
        }), bullets.end());

        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, background_texture.get(), NULL, NULL);
        draw_map(renderer, game_map, tiles_texture);
        draw_bullets(renderer, bullet_texture, bullets);


        SDL_Rect player_rect = {(int)(player.p.v.x*TILE_SIZE-(TILE_SIZE/2)),(int)(player.p.v.y*TILE_SIZE-TILE_SIZE),64, 128};
        {
            int r = 0, g = 0, b = 0;
            if (is_on_the_ground(player, game_map)) {
                r = 255;
            }
            if (is_in_collision(player.p, game_map)) {
                g = 255;
            }
            SDL_SetRenderDrawColor(renderer, r,g,b, 0xFF);
        }
        SDL_RenderDrawRect(renderer,  &player_rect);
        SDL_RenderCopyEx(renderer, player_texture.get(), NULL, &player_rect,0, NULL, SDL_FLIP_NONE);

        SDL_RenderPresent(renderer);
        current_time = current_time + microseconds((long long int)(dt*1000000.0));
        std::this_thread::sleep_until(current_time);
    }


    // After the game loop ends, display the final score
    std::cout << "Score: " << score << std::endl;

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
