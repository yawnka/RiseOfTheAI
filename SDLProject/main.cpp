#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION
#define LOG(argument) std::cout << argument << '\n'
#define GL_GLEXT_PROTOTYPES 1
#define FIXED_TIMESTEP 0.0166666f
#define PLATFORM_COUNT 11
#define ENEMY_COUNT 3
#define LEVEL1_WIDTH 24
#define LEVEL1_HEIGHT 7

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL_mixer.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include "Entity.h"
#include "Map.h"

// ————— GAME STATE ————— //
struct GameState
{
    Entity *player;
    Entity *enemies;
    Entity *platforms;
    
    Map *map;
    
    Mix_Music *bgm;
    Mix_Chunk *jump_sfx;
    
    int enemies_defeated = 0;  // variable to track defeated enemies
};



enum AppStatus { RUNNING, PAUSED, TERMINATED };

// ————— CONSTANTS ————— //
constexpr int WINDOW_WIDTH  = 640 * 2,
          WINDOW_HEIGHT = 700;

constexpr float BG_RED     = 0.1922f,
            BG_BLUE    = 0.549f,
            BG_GREEN   = 0.9059f,
            BG_OPACITY = 1.0f;

constexpr int VIEWPORT_X = 0,
          VIEWPORT_Y = 0,
          VIEWPORT_WIDTH  = WINDOW_WIDTH,
          VIEWPORT_HEIGHT = WINDOW_HEIGHT;

constexpr char GAME_WINDOW_NAME[] = "Hello, Maps!";

constexpr char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
           F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

constexpr float MILLISECONDS_IN_SECOND = 1000.0;

constexpr char SPRITESHEET_FILEPATH[] = "assets/images/player0.png",
           MAP_TILESET_FILEPATH[] = "assets/images/tileset_1.png",
           BGM_FILEPATH[]         = "assets/audio/dooblydoo.mp3",
           JUMP_SFX_FILEPATH[]    = "assets/audio/bounce.wav",
           PLATFORM_FILEPATH[]    = "assets/images/platform_tileset.png",
           ENEMY1_FILEPATH[] = "assets/images/enemy.png",
           ENEMY_FILEPATH[]       = "assets/images/soph.png";

constexpr char FONTSHEET_FILEPATH[]   = "assets/images/font1.png";
constexpr int FONTBANK_SIZE = 16;

constexpr int NUMBER_OF_TEXTURES = 1;
constexpr GLint LEVEL_OF_DETAIL  = 0;
constexpr GLint TEXTURE_BORDER   = 0;

constexpr float PLATFORM_OFFSET = 5.0f;

unsigned int LEVEL_1_DATA[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1,
    2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2
};


// ————— VARIABLES ————— //
GameState g_game_state;

SDL_Window* g_display_window;
AppStatus g_app_status = RUNNING;
ShaderProgram g_shader_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f,
      g_accumulator    = 0.0f;

GLuint load_texture(const char* filepath);
GLuint g_font_texture_id;
GLuint g_bg_texture_id;
glm::mat4 g_bg_matrix;

void print_vec3(const glm::vec3& vec)
{
    std::cout << "(" << vec.x << ", " << vec.y << ", " << vec.z << ")" << std::endl;
}

void initialise();
void process_input();
void update();
void render();
void shutdown();

// ————— GENERAL FUNCTIONS ————— //
GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);
    
    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }
    
    GLuint texture_id;
    glGenTextures(NUMBER_OF_TEXTURES, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    stbi_image_free(image);
    
    return texture_id;
}

// taken from lecture: sprites-and-text to write end game text
void draw_text(ShaderProgram *shader_program, GLuint font_texture_id, std::string text,
               float font_size, float spacing, glm::vec3 position)
{
    // Scale the size of the fontbank in the UV-plane
    // We will use this for spacing and positioning
    float width = 1.0f / FONTBANK_SIZE;
    float height = 1.0f / FONTBANK_SIZE;

    // Instead of having a single pair of arrays, we'll have a series of pairs—one for
    // each character. Don't forget to include <vector>!
    std::vector<float> vertices;
    std::vector<float> texture_coordinates;

    // For every character...
    for (int i = 0; i < text.size(); i++) {
        // 1. Get their index in the spritesheet, as well as their offset (i.e. their
        //    position relative to the whole sentence)
        int spritesheet_index = (int) text[i];  // ascii value of character
        float offset = (font_size + spacing) * i;

        // 2. Using the spritesheet index, we can calculate our U- and V-coordinates
        float u_coordinate = (float) (spritesheet_index % FONTBANK_SIZE) / FONTBANK_SIZE;
        float v_coordinate = (float) (spritesheet_index / FONTBANK_SIZE) / FONTBANK_SIZE;

        // 3. Inset the current pair in both vectors
        vertices.insert(vertices.end(), {
            offset + (-0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
        });

        texture_coordinates.insert(texture_coordinates.end(), {
            u_coordinate, v_coordinate,
            u_coordinate, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate + width, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate, v_coordinate + height,
        });
    }

    // 4. And render all of them using the pairs
    glm::mat4 model_matrix = glm::mat4(1.0f);
    model_matrix = glm::translate(model_matrix, position);

    shader_program->set_model_matrix(model_matrix);
    glUseProgram(shader_program->get_program_id());

    glVertexAttribPointer(shader_program->get_position_attribute(), 2, GL_FLOAT, false, 0,
                          vertices.data());
    glEnableVertexAttribArray(shader_program->get_position_attribute());

    glVertexAttribPointer(shader_program->get_tex_coordinate_attribute(), 2, GL_FLOAT,
                          false, 0, texture_coordinates.data());
    glEnableVertexAttribArray(shader_program->get_tex_coordinate_attribute());

    glBindTexture(GL_TEXTURE_2D, font_texture_id);
    glDrawArrays(GL_TRIANGLES, 0, (int) (text.size() * 6));

    glDisableVertexAttribArray(shader_program->get_position_attribute());
    glDisableVertexAttribArray(shader_program->get_tex_coordinate_attribute());
}

void initialise()
{
    // ————— GENERAL ————— //
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    g_display_window = SDL_CreateWindow(GAME_WINDOW_NAME,
                                      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      WINDOW_WIDTH, WINDOW_HEIGHT,
                                      SDL_WINDOW_OPENGL);
    
    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);
    if (context == nullptr)
    {
        LOG("ERROR: Could not create OpenGL context.\n");
        shutdown();
    }
    
#ifdef _WINDOWS
    glewInit();
#endif
    
    // ————— FONT SETUP ————— //
    g_font_texture_id = load_texture(FONTSHEET_FILEPATH);
    
    // ————— VIDEO SETUP ————— //
    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    
    g_shader_program.load(V_SHADER_PATH, F_SHADER_PATH);
    
    g_view_matrix = glm::mat4(1.0f);
    float initial_camera_y = -2.0f;
    g_view_matrix = glm::translate(g_view_matrix, glm::vec3(0.0f, -initial_camera_y, 0.0f));
    
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);
    
    g_shader_program.set_projection_matrix(g_projection_matrix);
    g_shader_program.set_view_matrix(g_view_matrix);

    glUseProgram(g_shader_program.get_program_id());
    
    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);
    
    // ————— MAP SET-UP ————— //
    GLuint map_texture_id = load_texture(MAP_TILESET_FILEPATH);
    g_game_state.map = new Map(LEVEL1_WIDTH, LEVEL1_HEIGHT, LEVEL_1_DATA, map_texture_id, 1.0f,3, 1);
    
    // ————— BACKGROUND SET-UP ————— //
    g_bg_texture_id = load_texture("assets/images/background.png");
    g_bg_matrix = glm::mat4(1.0f);
    g_bg_matrix = glm::translate(g_bg_matrix, glm::vec3(0.0f, 0.0f, 0.0f));
    g_bg_matrix = glm::scale(g_bg_matrix, glm::vec3(60.5f, 12.5f, 1.0f));   // scale
    
    // ––––– PLATFORM ––––– //
    GLuint platform_texture_id = load_texture(PLATFORM_FILEPATH);

    g_game_state.platforms = new Entity[PLATFORM_COUNT];

    float start_x = 6.0f; // Set the desired starting x position
    float start_y = 2.0f; // Set the desired starting y position

    for (int i = 0; i < PLATFORM_COUNT; i++) {
        g_game_state.platforms[i] = Entity(platform_texture_id, 0.0f, 0.4f, 1.0f, PLATFORM);
        g_game_state.platforms[i].set_position(glm::vec3(start_x + i, start_y, 0.0f));
        g_game_state.platforms[i].update(0.0f, nullptr, nullptr, 0, g_game_state.map);
    }
    
    // ————— GEORGE SET-UP ————— //

    GLuint player_texture_id = load_texture(SPRITESHEET_FILEPATH);

    int player_walking_animation[4][4] =
    {
        { 0, 1, 2, 3 },  // for George to move to the left,
        { 4, 5, 6, 7 }, // for George to move to the right,
        { 8, 9, 10, 11 }, // for George to move upwards,
        { 12, 13, 14, 15 }   // for George to move downwards
    };

    glm::vec3 acceleration = glm::vec3(0.0f,-4.905f, 0.0f);

    g_game_state.player = new Entity(
        player_texture_id,         // texture id
        3.0f,                      // speed
        acceleration,              // acceleration
        4.0f,                      // jumping power
        player_walking_animation,  // animation index sets
        0.0f,                      // animation time
        4,                         // animation frame amount
        0,                         // current animation index
        4,                         // animation column amount
        4,                         // animation row amount
        0.75f,                      // width (increase this value to scale up the player)
        0.75f,                      // height (increase this value to scale up the player)
        ENEMY
    );
    
    g_game_state.player->m_visual_scale = 2.0f; // Scale the player to twice the size
    g_game_state.player->set_position(glm::vec3(2.0f, 0.0f, 0.0f));

    // ————— ENEMY 1 SET-UP ————— //

    GLuint enemy_texture_id = load_texture(ENEMY1_FILEPATH);

    int enemy_walking_animation[4][4] = {
        {8, 9, 10, 11}, // Left
        {0, 1, 2, 3},   // Right
        {8, 9, 10, 11}, // Up
        {12, 13, 14, 15} // Down
    };
    glm::vec3 enemy_acceleration = glm::vec3(0.0f, -4.905f, 0.0f);

    g_game_state.enemies = new Entity[ENEMY_COUNT];

    for (int i = 0; i < ENEMY_COUNT; ++i) {
        g_game_state.enemies[i] = Entity(
            enemy_texture_id,          // texture id
            2.0f,                      // speed
            enemy_acceleration,        // acceleration
            1.0f,                      // jumping power (or adjust as needed)
            enemy_walking_animation,   // animation frames
            0.0f,                      // animation time
            4,                         // animation frame amount
            0,                         // current animation index
            4,                         // animation column amount
            4,                         // animation row amount
            0.35f,                     // width
            0.35f,                     // height
            ENEMY                      // type
        );

        //g_game_state.enemies[i].set_position(glm::vec3(4.0f + i * 2.0f, 1.5f, 0.0f)); // pos of enenmies
        g_game_state.enemies[i].m_visual_scale = 1.0f; // setting scale of enemies
    }
    
    g_game_state.enemies[0].set_position(glm::vec3(4.0f, -2.125f, 0.0f));
    g_game_state.enemies[1].set_position(glm::vec3(11.05f, -.125f, 0.0f));
    g_game_state.enemies[2].set_position(glm::vec3(15.8751f, -2.125f, 0.0f));

    // Jumping
    g_game_state.player->set_jumping_power(5.0f);

    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
    
    g_game_state.bgm = Mix_LoadMUS(BGM_FILEPATH);
//    Mix_PlayMusic(g_game_state.bgm, -1);
//    Mix_VolumeMusic(MIX_MAX_VOLUME / 16.0f);
    
    g_game_state.jump_sfx = Mix_LoadWAV(JUMP_SFX_FILEPATH);
    
    // ————— BLENDING ————— //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    g_game_state.player->set_movement(glm::vec3(0.0f));
    g_game_state.enemies->set_movement(glm::vec3(0.0f));
    
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            case SDL_QUIT:
            case SDL_WINDOWEVENT_CLOSE:
                g_app_status = TERMINATED;
                break;
                
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_q:
                        // Quit the game with a keystroke
                        g_app_status = TERMINATED;
                        break;
                        
                    case SDLK_SPACE:
                        // Jump
                        if (g_game_state.player->get_collided_bottom())
                        {
                            g_game_state.player->jump();
                            Mix_PlayChannel(-1, g_game_state.jump_sfx, 0);
                        }
                        break;
                        
                    default:
                        break;
                }
                
            default:
                break;
        }
    }
    
    const Uint8 *key_state = SDL_GetKeyboardState(NULL);

    if (key_state[SDL_SCANCODE_LEFT])        {
        g_game_state.player->move_left();
    }
    else if (key_state[SDL_SCANCODE_RIGHT]) {
        g_game_state.player->move_right();
    }
         
    if (glm::length(g_game_state.player->get_movement()) > 1.0f)
        g_game_state.player->normalise_movement();
}

void update() {
    if (g_app_status != RUNNING) return; // Skip updating if the game is paused or terminated

    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND;
    float delta_time = ticks - g_previous_ticks;
    g_previous_ticks = ticks;

    delta_time += g_accumulator;

    if (delta_time < FIXED_TIMESTEP) {
        g_accumulator = delta_time;
        return;
    }

    while (delta_time >= FIXED_TIMESTEP) {
        g_game_state.player->update(FIXED_TIMESTEP, g_game_state.player, g_game_state.platforms, PLATFORM_COUNT, g_game_state.map);

        // Update each enemy and check for collisions with the player
        for (int i = 0; i < ENEMY_COUNT; i++) {
            if (!g_game_state.enemies[i].is_active()) continue;

            // Update enemy
            g_game_state.enemies[i].update(FIXED_TIMESTEP, g_game_state.player, g_game_state.platforms, PLATFORM_COUNT, g_game_state.map);

            // Check for collisions
            if (g_game_state.player->check_collision(&g_game_state.enemies[i])) {
                if (g_game_state.player->get_position().y > g_game_state.enemies[i].get_position().y + g_game_state.enemies[i].get_height() / 2.0f) {
                    // When the player jumps on top of the enemy, deactivate the enemy
                    g_game_state.enemies[i].deactivate();
                    g_game_state.enemies_defeated++;

                    // Check if player has won by defeating all enemies
                    if (g_game_state.enemies_defeated == ENEMY_COUNT) {
                        g_app_status = PAUSED;  // pause the game to display message
                        std::cout << "You win!" << std::endl;
                        break;
                    }
                } else {
                    // Side or bottom collision; game over
                    g_app_status = PAUSED;  // pause the game to display message
                    std::cout << "¥ou lose!" << std::endl;
                    break;
                }
            }
        }

        delta_time -= FIXED_TIMESTEP;
    }

    g_accumulator = delta_time;

    float camera_y_offset = -2.0f;
    g_view_matrix = glm::mat4(1.0f);
    // Camera follows the player
    g_view_matrix = glm::translate(g_view_matrix, glm::vec3(-g_game_state.player->get_position().x, -camera_y_offset, 0.0f));

    g_shader_program.set_view_matrix(g_view_matrix);
    print_vec3(g_game_state.player->get_position());
}

void render()
{
    glClear(GL_COLOR_BUFFER_BIT);

    // Vertices for a square
    float vertices[] = {
        -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f,  // triangle 1
        -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f   // triangle 2
    };

    // Texture coordinates
    float texture_coordinates[] = {
        0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,     // triangle 1
        0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f      // triangle 2
    };

    glVertexAttribPointer(g_shader_program.get_position_attribute(), 2, GL_FLOAT, false, 0, vertices);
    glEnableVertexAttribArray(g_shader_program.get_position_attribute());

    glVertexAttribPointer(g_shader_program.get_tex_coordinate_attribute(), 2, GL_FLOAT, false, 0, texture_coordinates);
    glEnableVertexAttribArray(g_shader_program.get_tex_coordinate_attribute());

    g_shader_program.set_model_matrix(g_bg_matrix);
    glBindTexture(GL_TEXTURE_2D, g_bg_texture_id);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    g_shader_program.set_view_matrix(g_view_matrix);

    g_game_state.map->render(&g_shader_program);
    g_game_state.player->render(&g_shader_program);

//    for (int i = 0; i < PLATFORM_COUNT; i++) {
//        g_game_state.platforms[i].render(&g_shader_program);
//    }

    for (int i = 0; i < ENEMY_COUNT; i++) {
        if (g_game_state.enemies[i].is_active()) {
            g_game_state.enemies[i].render(&g_shader_program);
        }
    }

    // Display end-game messages if the game is paused which means its the end state
    if (g_app_status == PAUSED) {
        glm::vec3 player_position = g_game_state.player->get_position();
        glm::vec3 message_position = player_position + glm::vec3(-1.5f, 1.5f, 0.0f);  // Adjust y-offset as needed

        if (g_game_state.enemies_defeated == ENEMY_COUNT) {
            draw_text(&g_shader_program, g_font_texture_id, "You Win!", 0.5f, 0.05f, message_position);
        } else {
            draw_text(&g_shader_program, g_font_texture_id, "You Lose!", 0.5f, 0.05f, message_position);
        }
    }

    SDL_GL_SwapWindow(g_display_window);
}




void shutdown()
{
    SDL_Quit();
    
    delete [] g_game_state.platforms;
    delete [] g_game_state.enemies;
    delete    g_game_state.player;
    delete    g_game_state.map;
    Mix_FreeChunk(g_game_state.jump_sfx);
    Mix_FreeMusic(g_game_state.bgm);
}

// ————— GAME LOOP ————— //
int main(int argc, char* argv[])
{
    initialise();

    while (g_app_status != TERMINATED)
    {
        process_input();
        
        if (g_app_status == RUNNING) {
            update();
        }
        
        render();
    }

    shutdown();
    return 0;
}

