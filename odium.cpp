#include "raylib.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>

// Структура для кнопок
struct Button {
    Rectangle bounds;
    std::string text;
    bool hovered;
};

// Структура для врагов
struct Enemy {
    Vector2 position;
    Vector2 velocity;
    bool active;
    float frozenTimer;
    int health;
    int maxHealth;

    Enemy(Vector2 pos) : position(pos), active(true), frozenTimer(0), health(100), maxHealth(100) {}
};

// Структура для снарядов
struct Projectile {
    Vector2 position;
    Vector2 velocity;
    bool active;
    bool isFreezing;
    int damage;

    Projectile(Vector2 pos, Vector2 vel, bool freezing = false, int dmg = 0)
        : position(pos), velocity(vel), active(true), isFreezing(freezing), damage(dmg) {}
};

// Структура для игрока
struct Player {
    Vector2 position;
    Vector2 velocity;
    float speed;
    int selectedCompanion; // 0 - manual attack, 1 - passive attack
    float passiveAttackTimer;
    float freezeCooldown;
    int health;
    int maxHealth;
    float attackCooldown;

    Player() : position({ 0, 0 }), speed(120.0f), selectedCompanion(0),
        passiveAttackTimer(0), freezeCooldown(0), health(100), maxHealth(100), attackCooldown(0) {}
};

// Структура GameState для управления состоянием игры и камерой
struct GameState {
    Camera2D camera;
    Vector2 mapSize;

    GameState() : mapSize({ 10000.0f, 10000.0f }) {
        camera.target = { 0, 0 };
        camera.offset = { 400, 300 }; // центр экрана
        camera.rotation = 0.0f;
        camera.zoom = 1.0f;
    }

    void UpdateCamera(Vector2 playerPosition) {
        // Камера следует за игроком
        camera.target = playerPosition;

        // Ограничение камеры в пределах карты
        float minX = 400; // половина ширины экрана
        float minY = 300; // половина высоты экрана
        float maxX = mapSize.x - 400;
        float maxY = mapSize.y - 300;

        camera.target.x = std::max(minX, std::min(maxX, camera.target.x));
        camera.target.y = std::max(minY, std::min(maxY, camera.target.y));
    }
};

// Функция для расчета расстояния между двумя точками
float Vector2Distance(Vector2 v1, Vector2 v2) {
    float dx = v1.x - v2.x;
    float dy = v1.y - v2.y;
    return sqrt(dx * dx + dy * dy);
}

class Game {
private:
    // Основные переменные
    GameState gamestate;
    Player player;
    std::vector<Enemy> enemies;
    std::vector<Projectile> projectiles;

    // Время и счетчики
    float enemySpawnTimer;
    float gameOverTimer;
    bool gameOver;
    bool inGame;
    bool inSettings;

    // Настройки
    float musicVolume;

    // Таймеры
    float timeSinceLastSpawn;

public:
    Game() : enemySpawnTimer(0), gameOverTimer(5.0f), gameOver(false),
        inGame(false), inSettings(false), musicVolume(0.5f),
        timeSinceLastSpawn(0) {
        player.position = { gamestate.mapSize.x / 2, gamestate.mapSize.y / 2 };
    }

    void Init() {
        // Инициализация игрока в центре карты
        player.position = { gamestate.mapSize.x / 2, gamestate.mapSize.y / 2 };
        player.health = player.maxHealth;
        enemies.clear();
        projectiles.clear();
        gameOver = false;
        gameOverTimer = 5.0f;
        enemySpawnTimer = 0;
        timeSinceLastSpawn = 0;
        player.attackCooldown = 0;
        gamestate.UpdateCamera(player.position);
    }

    void UpdateMainMenu(Button& playButton, Button& settingsButton) {
        Vector2 mousePos = GetMousePosition();

        // Проверка hover для кнопок
        playButton.hovered = CheckCollisionPointRec(mousePos, playButton.bounds);
        settingsButton.hovered = CheckCollisionPointRec(mousePos, settingsButton.bounds);

        // Обработка кликов
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (playButton.hovered) {
                inGame = true;
                Init();
            }
            if (settingsButton.hovered) {
                inSettings = true;
            }
        }
    }

    void UpdateSettings(Button& backButton) {
        Vector2 mousePos = GetMousePosition();

        // Проверка hover для кнопки назад
        backButton.hovered = CheckCollisionPointRec(mousePos, backButton.bounds);

        // Обработка кликов
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (backButton.hovered) {
                inSettings = false;
            }
        }

        // Регулировка громкости
        if (IsKeyDown(KEY_LEFT)) {
            musicVolume = std::max(0.0f, musicVolume - 0.01f);
        }
        if (IsKeyDown(KEY_RIGHT)) {
            musicVolume = std::min(1.0f, musicVolume + 0.01f);
        }

        // Установка громкости
        SetMasterVolume(musicVolume);
    }

    void UpdateGameplay() {
        if (gameOver) {
            if (IsKeyPressed(KEY_ENTER)) {
                inGame = false;
            }
            return;
        }

        float deltaTime = GetFrameTime();

        // Обновление таймеров
        timeSinceLastSpawn += deltaTime;
        if (player.selectedCompanion == 1) {
            player.passiveAttackTimer += deltaTime;
            if (player.freezeCooldown > 0) {
                player.freezeCooldown -= deltaTime;
            }
        }

        if (player.attackCooldown > 0) {
            player.attackCooldown -= deltaTime;
        }

        // Движение игрока
        UpdatePlayerMovement(deltaTime);

        // Обновление камеры
        gamestate.UpdateCamera(player.position);

        // Спавн врагов
        UpdateEnemySpawning(deltaTime);

        // Обновление врагов
        UpdateEnemies(deltaTime);

        // Обновление снарядов
        UpdateProjectiles(deltaTime);

        // Проверка столкновений врагов с игроком
        CheckPlayerEnemyCollisions();

        // Проверка условий поражения
        CheckGameOverCondition(deltaTime);

        // Обработка выбора компаньона
        if (IsKeyPressed(KEY_ONE)) {
            player.selectedCompanion = 0;
        }
        if (IsKeyPressed(KEY_TWO)) {
            player.selectedCompanion = 1;
        }

        // Обработка атак в зависимости от выбранного компаньона
        if (player.selectedCompanion == 0) {
            HandleMeleeAttack();
        }
        else {
            HandlePassiveAttack();
            HandleFreezeAttack();
        }
    }

    void UpdatePlayerMovement(float deltaTime) {
        player.velocity = { 0, 0 };

        // Управление клавишами
        if (IsKeyDown(KEY_W)) player.velocity.y = -1;
        if (IsKeyDown(KEY_S)) player.velocity.y = 1;
        if (IsKeyDown(KEY_A)) player.velocity.x = -1;
        if (IsKeyDown(KEY_D)) player.velocity.x = 1;

        // Нормализация вектора движения
        float length = sqrt(player.velocity.x * player.velocity.x + player.velocity.y * player.velocity.y);
        if (length > 0) {
            player.velocity.x /= length;
            player.velocity.y /= length;
        }

        // Применение скорости
        player.position.x += player.velocity.x * player.speed * deltaTime;
        player.position.y += player.velocity.y * player.speed * deltaTime;

        // Ограничение в пределах карты
        player.position.x = std::max(0.0f, std::min(gamestate.mapSize.x, player.position.x));
        player.position.y = std::max(0.0f, std::min(gamestate.mapSize.y, player.position.y));
    }

    void UpdateEnemySpawning(float deltaTime) {
        enemySpawnTimer += deltaTime;

        // Спавн нового врага каждые 1.5 секунды
        if (enemySpawnTimer >= 1.5f && enemies.size() < 150) {
            SpawnEnemy();
            enemySpawnTimer = 0;
        }
    }

    void SpawnEnemy() {
        // Спавн врага на краю карты
        Vector2 spawnPos;
        int side = GetRandomValue(0, 3);

        switch (side) {
        case 0: // Top
            spawnPos = { (float)GetRandomValue(0, (int)gamestate.mapSize.x), 0.0f };
            break;
        case 1: // Right
            spawnPos = { gamestate.mapSize.x, (float)GetRandomValue(0, (int)gamestate.mapSize.y) };
            break;
        case 2: // Bottom
            spawnPos = { (float)GetRandomValue(0, (int)gamestate.mapSize.x), gamestate.mapSize.y };
            break;
        case 3: // Left
            spawnPos = { 0.0f, (float)GetRandomValue(0, (int)gamestate.mapSize.y) };
            break;
        }

        enemies.emplace_back(spawnPos);
    }

    void UpdateEnemies(float deltaTime) {
        for (auto& enemy : enemies) {
            if (!enemy.active) continue;

            // Обновление таймера заморозки
            if (enemy.frozenTimer > 0) {
                enemy.frozenTimer -= deltaTime;
                continue; // Замороженные враги не двигаются
            }

            // Движение к игроку
            Vector2 direction = {
                player.position.x - enemy.position.x,
                player.position.y - enemy.position.y
            };

            // Нормализация направления
            float length = sqrt(direction.x * direction.x + direction.y * direction.y);
            if (length > 0) {
                direction.x /= length;
                direction.y /= length;
            }

            // Движение врага
            enemy.position.x += direction.x * 110.0f * deltaTime;
            enemy.position.y += direction.y * 110.0f * deltaTime;
        }

        // Удаление неактивных врагов
        enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
            [](const Enemy& e) { return !e.active || e.health <= 0; }), enemies.end());
    }

    void UpdateProjectiles(float deltaTime) {
        for (auto& projectile : projectiles) {
            if (!projectile.active) continue;

            // Движение снаряда
            projectile.position.x += projectile.velocity.x * deltaTime;
            projectile.position.y += projectile.velocity.y * deltaTime;

            // Проверка столкновений с врагами
            for (auto& enemy : enemies) {
                if (!enemy.active) continue;

                float distance = Vector2Distance(projectile.position, enemy.position);
                if (distance < 30.0f) {
                    // Наносим урон врагу
                    enemy.health -= projectile.damage;

                    // Применение эффекта заморозки
                    if (projectile.isFreezing) {
                        enemy.frozenTimer = 3.0f; // Заморозка на 3 секунды
                    }

                    projectile.active = false;
                    break;
                }
            }

            // Деактивация снарядов за пределами карты
            if (projectile.position.x < 0 || projectile.position.x > gamestate.mapSize.x ||
                projectile.position.y < 0 || projectile.position.y > gamestate.mapSize.y) {
                projectile.active = false;
            }
        }

        // Удаление неактивных снарядов
        projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile& p) { return !p.active; }), projectiles.end());
    }

    void CheckPlayerEnemyCollisions() {
        for (auto& enemy : enemies) {
            if (!enemy.active) continue;

            float distance = Vector2Distance(player.position, enemy.position);
            if (distance < 40.0f) { // Столкновение с врагом
                player.health -= 5; // Урон врага по игроку

                // Небольшое отталкивание
                Vector2 pushDirection = {
                    player.position.x - enemy.position.x,
                    player.position.y - enemy.position.y
                };

                float length = sqrt(pushDirection.x * pushDirection.x + pushDirection.y * pushDirection.y);
                if (length > 0) {
                    pushDirection.x /= length;
                    pushDirection.y /= length;
                }

                player.position.x += pushDirection.x * 20.0f;
                player.position.y += pushDirection.y * 20.0f;
            }
        }

        // Проверка смерти игрока
        if (player.health <= 0) {
            gameOver = true;
        }
    }

    void HandleMeleeAttack() {
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && player.attackCooldown <= 0) {
            Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), gamestate.camera);

            // Поиск ближайшего врага для ближней атаки (маленький радиус)
            Enemy* target = nullptr;
            float minDist = 60.0f; // Маленький радиус для ближней атаки

            for (auto& enemy : enemies) {
                if (!enemy.active) continue;

                float distance = Vector2Distance(player.position, enemy.position);
                if (distance < minDist) {
                    minDist = distance;
                    target = &enemy;
                }
            }

            if (target) {
                // Наносим урон ближайшему врагу
                target->health -= 30; // Урон игрока ПКМ

                // КД атаки
                player.attackCooldown = 0.5f;
            }
        }
    }

    void HandlePassiveAttack() {
        // Автоматическая атака каждые 1.4 секунды
        if (player.passiveAttackTimer >= 1.4f) {
            // Поиск 3 ближайших врагов
            std::vector<Enemy*> nearbyEnemies;

            for (auto& enemy : enemies) {
                if (!enemy.active) continue;

                float distance = Vector2Distance(player.position, enemy.position);
                if (distance < 250.0f) {
                    nearbyEnemies.push_back(&enemy);
                }
            }

            // Сортировка по расстоянию
            std::sort(nearbyEnemies.begin(), nearbyEnemies.end(),
                [this](Enemy* a, Enemy* b) {
                    return Vector2Distance(player.position, a->position) <
                        Vector2Distance(player.position, b->position);
                });

            // Атака до 3 ближайших врагов
            int targetsToAttack = std::min(3, (int)nearbyEnemies.size());
            for (int i = 0; i < targetsToAttack; i++) {
                Enemy* target = nearbyEnemies[i];
                Vector2 direction = {
                    target->position.x - player.position.x,
                    target->position.y - player.position.y
                };

                float length = sqrt(direction.x * direction.x + direction.y * direction.y);
                if (length > 0) {
                    direction.x /= length;
                    direction.y /= length;
                }

                // Снаряды компаньона с эффектом заморозки и уроном 40
                projectiles.emplace_back(player.position,
                    Vector2{ direction.x * 250.0f, direction.y * 250.0f }, true, 40);
            }

            player.passiveAttackTimer = 0;
        }
    }

    void HandleFreezeAttack() {
        // Дополнительная атака с заморозкой на пробел
        if (IsKeyPressed(KEY_SPACE) && player.freezeCooldown <= 0) {
            // Поиск ближайшего врага
            Enemy* target = nullptr;
            float minDist = 300.0f;

            for (auto& enemy : enemies) {
                if (!enemy.active) continue;

                float distance = Vector2Distance(player.position, enemy.position);
                if (distance < minDist) {
                    minDist = distance;
                    target = &enemy;
                }
            }

            if (target) {
                Vector2 direction = {
                    target->position.x - player.position.x,
                    target->position.y - player.position.y
                };

                float length = sqrt(direction.x * direction.x + direction.y * direction.y);
                if (length > 0) {
                    direction.x /= length;
                    direction.y /= length;
                }

                projectiles.emplace_back(player.position,
                    Vector2{ direction.x * 200.0f, direction.y * 200.0f }, true, 40);

                player.freezeCooldown = 10.0f; // КД 10 секунд
            }
        }
    }

    void CheckGameOverCondition(float deltaTime) {
        if (enemies.size() > 90 || player.health <= 0) {
            gameOverTimer -= deltaTime;
            if (gameOverTimer <= 0) {
                gameOver = true;
            }
        }
        else {
            gameOverTimer = 5.0f; // Сброс таймера
        }
    }

    void DrawMainMenu(const Button& playButton, const Button& settingsButton) {
        ClearBackground(BLACK);

        // Заголовок
        DrawText("ODIUM", 400 - MeasureText("ODIUM", 60) / 2, 150, 60, WHITE);

        // Кнопка "Play"
        DrawRectangleRec(playButton.bounds, playButton.hovered ? GRAY : DARKGRAY);
        DrawText(playButton.text.c_str(),
            playButton.bounds.x + playButton.bounds.width / 2 - MeasureText(playButton.text.c_str(), 30) / 2,
            playButton.bounds.y + playButton.bounds.height / 2 - 15, 30, WHITE);

        // Кнопка "Settings"
        DrawRectangleRec(settingsButton.bounds, settingsButton.hovered ? GRAY : DARKGRAY);
        DrawText(settingsButton.text.c_str(),
            settingsButton.bounds.x + settingsButton.bounds.width / 2 - MeasureText(settingsButton.text.c_str(), 30) / 2,
            settingsButton.bounds.y + settingsButton.bounds.height / 2 - 15, 30, WHITE);
    }

    void DrawSettings(const Button& backButton) {
        ClearBackground(BLACK);

        // Заголовок
        DrawText("SETTINGS", 400 - MeasureText("SETTINGS", 50) / 2, 100, 50, WHITE);

        // Громкость музыки
        DrawText("MUSIC VOLUME:", 200, 200, 30, WHITE);

        // Ползунок громкости
        Rectangle sliderBar = { 400, 205, 200, 20 };
        Rectangle sliderHandle = { 400 + musicVolume * 200 - 5, 200, 10, 30 };

        DrawRectangleRec(sliderBar, DARKGRAY);
        DrawRectangleRec(sliderHandle, WHITE);

        // Отображение значения громкости
        std::string volumeText = std::to_string((int)(musicVolume * 100)) + "%";
        DrawText(volumeText.c_str(), 610, 205, 20, WHITE);

        // Кнопка "Back"
        DrawRectangleRec(backButton.bounds, backButton.hovered ? GRAY : DARKGRAY);
        DrawText(backButton.text.c_str(),
            backButton.bounds.x + backButton.bounds.width / 2 - MeasureText(backButton.text.c_str(), 30) / 2,
            backButton.bounds.y + backButton.bounds.height / 2 - 15, 30, WHITE);
    }

    void DrawGameplay() {
        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode2D(gamestate.camera);
        {
            // Отрисовка врагов (синие квадраты)
            for (const auto& enemy : enemies) {
                if (!enemy.active) continue;

                if (enemy.frozenTimer > 0) {
                    // Замороженные враги - голубые квадраты
                    DrawRectangle((int)enemy.position.x - 20, (int)enemy.position.y - 20, 40, 40, SKYBLUE);
                }
                else {
                    // Обычные враги - синие квадраты
                    DrawRectangle((int)enemy.position.x - 20, (int)enemy.position.y - 20, 40, 40, BLUE);
                }

                // Отрисовка HP бара врага
                float healthPercent = (float)enemy.health / enemy.maxHealth;
                DrawRectangle((int)enemy.position.x - 20, (int)enemy.position.y - 30, 40, 5, RED);
                DrawRectangle((int)enemy.position.x - 20, (int)enemy.position.y - 30, (int)(40 * healthPercent), 5, GREEN);
            }

            // Отрисовка снарядов (желтые квадраты)
            for (const auto& projectile : projectiles) {
                if (!projectile.active) continue;

                if (projectile.isFreezing) {
                    // Снаряды заморозки - голубые квадраты
                    DrawRectangle((int)projectile.position.x - 10, (int)projectile.position.y - 10, 20, 20, SKYBLUE);
                }
                else {
                    // Обычные снаряды - желтые квадраты
                    DrawRectangle((int)projectile.position.x - 10, (int)projectile.position.y - 10, 20, 20, YELLOW);
                }
            }

            // Отрисовка игрока (красный квадрат)
            DrawRectangle((int)player.position.x - 25, (int)player.position.y - 25, 50, 50, RED);
        }
        EndMode2D();

        // Отрисовка UI поверх игрового мира
        DrawUI();

        // Отрисовка мини-карты в левом нижнем углу
        DrawMinimap();

        EndDrawing();
    }

    void DrawMinimap() {
        // Размер мини-карты
        int minimapSize = 150;
        int minimapX = 10;
        int minimapY = 600 - minimapSize - 10;

        // Темно-зеленый фон мини-карты
        DrawRectangle(minimapX, minimapY, minimapSize, minimapSize, DARKGREEN);
        DrawRectangleLines(minimapX, minimapY, minimapSize, minimapSize, WHITE);

        // Масштаб для мини-карты
        float scaleX = (float)minimapSize / gamestate.mapSize.x;
        float scaleY = (float)minimapSize / gamestate.mapSize.y;

        // Отрисовка врагов на мини-карте
        for (const auto& enemy : enemies) {
            if (!enemy.active) continue;

            int enemyX = minimapX + (int)(enemy.position.x * scaleX);
            int enemyY = minimapY + (int)(enemy.position.y * scaleY);

            if (enemy.frozenTimer > 0) {
                DrawRectangle(enemyX - 2, enemyY - 2, 4, 4, SKYBLUE);
            }
            else {
                DrawRectangle(enemyX - 2, enemyY - 2, 4, 4, BLUE);
            }
        }

        // Отрисовка игрока на мини-карте
        int playerX = minimapX + (int)(player.position.x * scaleX);
        int playerY = minimapY + (int)(player.position.y * scaleY);
        DrawRectangle(playerX - 3, playerY - 3, 6, 6, RED);
    }

    void DrawUI() {
        // Отображение количества врагов
        std::string enemyCountText = "Enemies: " + std::to_string(enemies.size()) + "/90";
        DrawText(enemyCountText.c_str(), 10, 10, 20, WHITE);

        // Отображение HP игрока
        std::string healthText = "HP: " + std::to_string(player.health) + "/100";
        DrawText(healthText.c_str(), 10, 40, 20, GREEN);

        // HP бар игрока
        float playerHealthPercent = (float)player.health / player.maxHealth;
        DrawRectangle(100, 45, 100, 10, RED);
        DrawRectangle(100, 45, (int)(100 * playerHealthPercent), 10, GREEN);

        // Отображение таймера поражения
        if (enemies.size() > 90) {
            std::string timerText = "Time: " + std::to_string((int)gameOverTimer);
            DrawText(timerText.c_str(), 10, 70, 20, RED);
        }

        // Отображение выбранного компаньона
        std::string companionText = "Companion: ";
        companionText += (player.selectedCompanion == 0) ? "Melee Attack (1)" : "Passive Attack (2)";
        DrawText(companionText.c_str(), 10, 100, 20, WHITE);

        // Отображение информации о способностях
        if (player.selectedCompanion == 0) {
            DrawText("RMB: Melee Attack (30 damage)", 10, 130, 20, WHITE);
            std::string cooldownText = "Cooldown: " + std::to_string((int)player.attackCooldown) + "s";
            DrawText(cooldownText.c_str(), 10, 160, 20, WHITE);
        }
        else {
            DrawText("Auto-attack: 3 targets every 1.4s (40 damage + freeze)", 10, 130, 20, WHITE);
            std::string freezeText = "Space: Freeze (CD: " + std::to_string((int)player.freezeCooldown) + "s)";
            DrawText(freezeText.c_str(), 10, 160, 20, WHITE);
        }

        // Отображение поражения
        if (gameOver) {
            DrawRectangle(0, 0, 800, 600, Fade(BLACK, 0.5f));
            DrawText("GAME OVER", 400 - MeasureText("GAME OVER", 40) / 2, 250, 40, RED);
            DrawText("Press ENTER to exit", 400 - MeasureText("Press ENTER to exit", 20) / 2, 300, 20, WHITE);
        }
    }

    void Run() {
        // Создание кнопок для главного меню
        Button playButton = { {300, 300, 200, 50}, "PLAY", false };
        Button settingsButton = { {300, 370, 200, 50}, "SETTINGS", false };
        Button backButton = { {300, 400, 200, 50}, "BACK", false };

        while (!WindowShouldClose()) {
            if (inGame) {
                UpdateGameplay();
                DrawGameplay();
            }
            else if (inSettings) {
                UpdateSettings(backButton);
                BeginDrawing();
                DrawSettings(backButton);
                EndDrawing();
            }
            else {
                UpdateMainMenu(playButton, settingsButton);
                BeginDrawing();
                DrawMainMenu(playButton, settingsButton);
                EndDrawing();
            }
        }
    }
};

int main() {
    // Инициализация окна
    InitWindow(800, 600, "Odium");
    SetTargetFPS(60);

    // Создание и запуск игры
    Game game;
    game.Run();

    // Закрытие окна
    CloseWindow();

    return 0;
}