#include "raylib.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <random>

// Размеры окна
const int SCREEN_WIDTH = 1024;
const int SCREEN_HEIGHT = 1024;

// Константы для баланса игры
const int MAX_ENEMIES = 70;
const int PLAYER_MAX_HEALTH = 100;
const int ENEMY_MAX_HEALTH = 100;
const int GAME_OVER_TIMER = 5;

// Структура для кнопок
struct Button {
    Rectangle bounds;
    std::string text;
    bool hovered;
};

// Структура для предметов инвентаря
struct InventoryItem {
    int type = 0; // 0 - пусто, 1 - melee companion, 2 - range companion, 3 - mars
    int starLevel = 1; // Уровень звезды (1-6)
    Rectangle slot = { 0, 0, 0, 0 };
    std::string description = "Empty Slot";
};

// Структура для врагов
struct Enemy {
    Vector2 position;
    Vector2 velocity = { 0, 0 };
    bool active;
    float frozenTimer = 0;
    int health;
    int maxHealth;

    Enemy(Vector2 pos) : position(pos), active(true), health(ENEMY_MAX_HEALTH), maxHealth(ENEMY_MAX_HEALTH) {}
};

// Структура для снарядов
struct Projectile {
    Vector2 position;
    Vector2 velocity;
    bool active;
    bool isFreezing;
    bool isMarsSpear;
    bool isMarsWave;
    float size;
    int damage;

    Projectile(Vector2 pos, Vector2 vel, bool freezing = false, int dmg = 0,
        bool marsSpear = false, bool marsWave = false, float projectileSize = 20.0f)
        : position(pos), velocity(vel), active(true), isFreezing(freezing),
        damage(dmg), isMarsSpear(marsSpear), isMarsWave(marsWave), size(projectileSize) {}
};

// Структура для игрока
struct Player {
    Vector2 position;
    Vector2 velocity = { 0, 0 };
    float speed;
    float passiveAttackTimer;
    float freezeCooldown;
    int health;
    int maxHealth;
    float attackCooldown;
    int gold;
    int kills;
    int meleeCompanions;
    int rangeCompanions;
    int marsCompanions;
    int companionLevel; // Уровень улучшения компаньонов (1-6)

    Player() : position({ 0, 0 }), speed(120.0f),
        passiveAttackTimer(0), freezeCooldown(0), health(PLAYER_MAX_HEALTH), maxHealth(PLAYER_MAX_HEALTH),
        attackCooldown(0), gold(0), kills(0), meleeCompanions(0), rangeCompanions(0),
        marsCompanions(0), companionLevel(1) {}
};

// Структура GameState для управления состоянием игры
struct GameState {
    Vector2 mapSize;
    Vector2 cameraOffset;

    GameState() : mapSize({ 5000.0f, 5000.0f }), cameraOffset({ 0, 0 }) {}

    void UpdateCamera(Vector2 playerPosition) {
        // Центрируем камеру на игроке, но ограничиваем смещением фона
        cameraOffset.x = playerPosition.x - SCREEN_WIDTH / 2;
        cameraOffset.y = playerPosition.y - SCREEN_HEIGHT / 2;

        // Ограничиваем смещение камеры границами карты
        float minX = 0;
        float minY = 0;
        float maxX = mapSize.x - SCREEN_WIDTH;
        float maxY = mapSize.y - SCREEN_HEIGHT;

        cameraOffset.x = std::max(minX, std::min(maxX, cameraOffset.x));
        cameraOffset.y = std::max(minY, std::min(maxY, cameraOffset.y));
    }

    Vector2 WorldToScreen(Vector2 worldPos) {
        return { worldPos.x - cameraOffset.x, worldPos.y - cameraOffset.y };
    }
};

// Структура для предметов магазина
struct ShopItem {
    int id = 0;
    std::string name;
    std::string description;
    int goldPrice = 0;
    int killsPrice = 0;
    bool available = true;
    int tier = 1;
};

struct ActiveShopItems {
    ShopItem slot1;
    ShopItem slot2;
    ShopItem slot3;
    float refreshTimer = 0;
    int manualRefreshCost = 20;
    int freeRefreshesLeft = 6;
};

float Vector2Distance(Vector2 v1, Vector2 v2) {
    float dx = v1.x - v2.x;
    float dy = v1.y - v2.y;
    return sqrt(dx * dx + dy * dy);
}

class Game {
private:
    GameState gamestate;
    Player player;
    std::vector<Enemy> enemies;
    std::vector<Projectile> projectiles;
    std::vector<InventoryItem> inventory;

    float enemySpawnTimer;
    float gameOverTimer;
    bool gameOver;
    bool inGame;
    bool inSettings;
    bool choosingWeapon;
    bool inShop;

    float musicVolume;
    float timeSinceLastSpawn;

    int randomCompanionPriceGold;
    int randomCompanionPriceKills;
    int purchaseCount;

    std::vector<float> meleeAttackTimers;
    std::vector<float> rangeAttackTimers;
    std::vector<float> marsAttackTimers;
    float auraAttackTimer;

    std::vector<ShopItem> allShopItems;
    ActiveShopItems currentShop;
    float shopRefreshTimer;

    float attackCooldownReduction;
    float movementSpeedBonus;
    int extraEnemiesPerSpawn;
    float damageBonus;
    int pocketHeroUses;
    int freeRefreshUses;

    // Текстуры
    Texture2D inventoryTexture;
    Texture2D meleeTexture;
    Texture2D rangeTexture;
    Texture2D marsTexture;
    Texture2D backgroundTexture;
    bool texturesLoaded;
    Texture2D menuBackgroundTexture;
    bool menuBackgroundLoaded;

public:
    Game() : enemySpawnTimer(0), gameOverTimer(GAME_OVER_TIMER), gameOver(false),
        inGame(false), inSettings(false), musicVolume(0.5f),
        timeSinceLastSpawn(0), choosingWeapon(false), inShop(false),
        randomCompanionPriceGold(300), randomCompanionPriceKills(30),
        purchaseCount(0), auraAttackTimer(0),
        attackCooldownReduction(0), movementSpeedBonus(0), extraEnemiesPerSpawn(0),
        damageBonus(0), pocketHeroUses(0), freeRefreshUses(0),
        texturesLoaded(false), menuBackgroundLoaded(false) {

        player.position = { gamestate.mapSize.x / 2, gamestate.mapSize.y / 2 };
        LoadTextures();
        InitializeInventory();
        InitializeShopItems();
        RefreshShop();
    }

    ~Game() {
        UnloadTextures();
    }

    void LoadTextures() {
        // Загрузка текстур
        if (FileExists("inventory.png")) {
            inventoryTexture = LoadTexture("inventory.png");
        }
        if (FileExists("melee.png")) {
            meleeTexture = LoadTexture("melee.png");
        }
        if (FileExists("range.png")) {
            rangeTexture = LoadTexture("range.png");
        }
        if (FileExists("mars.png")) {
            marsTexture = LoadTexture("mars.png");
        }
        if (FileExists("background.png")) {
            backgroundTexture = LoadTexture("background.png");
        }
        if (FileExists("menu_background.png")) {
            menuBackgroundTexture = LoadTexture("menu_background.png");
            menuBackgroundLoaded = true;
        }
        texturesLoaded = true;
    }

    void UnloadTextures() {
        if (texturesLoaded) {
            if (inventoryTexture.id != 0) UnloadTexture(inventoryTexture);
            if (meleeTexture.id != 0) UnloadTexture(meleeTexture);
            if (rangeTexture.id != 0) UnloadTexture(rangeTexture);
            if (marsTexture.id != 0) UnloadTexture(marsTexture);
            if (backgroundTexture.id != 0) UnloadTexture(backgroundTexture);
            if (menuBackgroundLoaded) {
                UnloadTexture(menuBackgroundTexture);
            }
        }
    }

    void InitializeShopItems() {
        allShopItems.clear();

        allShopItems.push_back({ 1, "Moon Shard", "-2% Attack Cooldown\nStacks", 400, 0, true, 1 });
        allShopItems.push_back({ 2, "Boots of Travel", "+3% Movement Speed\nStacks", 400, 0, true, 1 });
        allShopItems.push_back({ 3, "Doom Heart", "+1 Enemy per Spawn\nStacks", 400, 0, true, 1 });
        allShopItems.push_back({ 4, "Power Crystal", "+2% Total Damage\nStacks", 400, 0, true, 1 });
        allShopItems.push_back({ 5, "Pocket Heroes", "Random Companion\nTier 1-2", 0, 40, true, 2 });
        allShopItems.push_back({ 6, "Refresh Token", "Free Shop Refresh\n(0-6 uses)", 400, 0, true, 1 });
    }

    void RefreshShop() {
        std::vector<ShopItem> availableItems = allShopItems;

        for (int i = 0; i < 3 && !availableItems.empty(); i++) {
            int randomIndex = GetRandomValue(0, (int)availableItems.size() - 1);

            if (i == 0) currentShop.slot1 = availableItems[randomIndex];
            else if (i == 1) currentShop.slot2 = availableItems[randomIndex];
            else if (i == 2) currentShop.slot3 = availableItems[randomIndex];

            availableItems.erase(availableItems.begin() + randomIndex);
        }

        currentShop.slot3.killsPrice = 60;
        currentShop.slot3.goldPrice = 0;

        currentShop.refreshTimer = 60.0f;
        currentShop.manualRefreshCost = 20;
        currentShop.freeRefreshesLeft = 6;
    }

    void InitializeInventory() {
        inventory.clear();
        int slotWidth = 102;
        int slotHeight = 52;
        int startX = (SCREEN_WIDTH - slotWidth * 3) / 2;
        int startY = SCREEN_HEIGHT - slotHeight * 2 - 20;

        for (int i = 0; i < 3; i++) {
            InventoryItem item;
            item.type = 0;
            item.starLevel = 1;
            item.slot = { (float)startX + i * slotWidth, (float)startY + slotHeight, (float)slotWidth, (float)slotHeight };
            item.description = "Empty Slot";
            inventory.push_back(item);
        }

        for (int i = 0; i < 3; i++) {
            InventoryItem item;
            item.type = 0;
            item.starLevel = 1;
            item.slot = { (float)startX + i * slotWidth, (float)startY, (float)slotWidth, (float)slotHeight };
            item.description = "Empty Slot";
            inventory.push_back(item);
        }

        UpdateInventoryDisplay();
    }

    void UpdateInventoryDisplay() {
        int totalCompanions = player.meleeCompanions + player.rangeCompanions + player.marsCompanions;

        for (int i = 0; i < inventory.size(); i++) {
            if (i < player.meleeCompanions) {
                inventory[i].type = 1;
                inventory[i].starLevel = player.companionLevel;
                inventory[i].description = "Melee Companion " + GetStarString(player.companionLevel) +
                    "\nAttack: " + std::to_string(5 * player.companionLevel) + " targets" +
                    "\nDamage: " + std::to_string(40 * player.companionLevel) + " per target" +
                    "\nRange: Close combat";
            }
            else if (i < player.meleeCompanions + player.rangeCompanions) {
                inventory[i].type = 2;
                inventory[i].starLevel = player.companionLevel;
                inventory[i].description = "Range Companion " + GetStarString(player.companionLevel) +
                    "\nManual: 1 target x " + std::to_string(40 * player.companionLevel) + " damage" +
                    "\nAuto: " + std::to_string(3 * player.companionLevel) + " targets x " + std::to_string(30 * player.companionLevel) + " damage" +
                    "\nSpecial: Freeze effect";
            }
            else if (i < player.meleeCompanions + player.rangeCompanions + player.marsCompanions) {
                inventory[i].type = 3;
                inventory[i].starLevel = player.companionLevel;
                inventory[i].description = "Mars " + GetStarString(player.companionLevel) +
                    "\nWave Attack: " + std::to_string(60 * player.companionLevel) + " damage" +
                    "\n180 semicircle attack" +
                    "\nAuto-attack every " + std::to_string(3.0f / player.companionLevel) + " seconds" +
                    "\nAURA: " + std::to_string(10 * player.companionLevel) + " damage around player";
            }
            else {
                inventory[i].type = 0;
                inventory[i].starLevel = 1;
                inventory[i].description = "Empty Slot";
            }
        }
    }

    std::string GetStarString(int level) {
        std::string stars = "";
        for (int i = 0; i < level && i < 6; i++) {
            stars += "*";
        }
        return stars;
    }

    void Init() {
        player.position = { gamestate.mapSize.x / 2, gamestate.mapSize.y / 2 };
        player.health = player.maxHealth;
        player.gold = 0;
        player.kills = 0;
        player.meleeCompanions = 0;
        player.rangeCompanions = 0;
        player.marsCompanions = 0;
        player.companionLevel = 1;
        enemies.clear();
        projectiles.clear();
        gameOver = false;
        gameOverTimer = GAME_OVER_TIMER;
        enemySpawnTimer = 0;
        timeSinceLastSpawn = 0;
        player.attackCooldown = 0;
        choosingWeapon = true;
        inShop = false;
        purchaseCount = 0;
        randomCompanionPriceGold = 300;
        randomCompanionPriceKills = 30;

        meleeAttackTimers.clear();
        rangeAttackTimers.clear();
        marsAttackTimers.clear();
        auraAttackTimer = 0;

        attackCooldownReduction = 0;
        movementSpeedBonus = 0;
        extraEnemiesPerSpawn = 0;
        damageBonus = 0;
        pocketHeroUses = 0;
        freeRefreshUses = 0;

        gamestate.UpdateCamera(player.position);
        InitializeInventory();
        RefreshShop();
    }

    int GetSelectedWeaponType() {
        if (player.marsCompanions > 0) return 3;
        if (player.rangeCompanions > 0) return 2;
        if (player.meleeCompanions > 0) return 1;
        return 0;
    }

    void UpdateWeaponChoice(Button& meleeButton, Button& rangeButton) {
        Vector2 mousePos = GetMousePosition();

        meleeButton.hovered = CheckCollisionPointRec(mousePos, meleeButton.bounds);
        rangeButton.hovered = CheckCollisionPointRec(mousePos, rangeButton.bounds);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (meleeButton.hovered) {
                player.meleeCompanions = 1;
                meleeAttackTimers.push_back(0);
                UpdateInventoryDisplay();
                choosingWeapon = false;
            }
            if (rangeButton.hovered) {
                player.rangeCompanions = 1;
                rangeAttackTimers.push_back(0);
                UpdateInventoryDisplay();
                choosingWeapon = false;
            }
        }
    }

    void UpdateShop(Button& randomButton, Button& closeButton, Button& refreshButton, Button& upgradeButton) {
        Vector2 mousePos = GetMousePosition();

        randomButton.hovered = CheckCollisionPointRec(mousePos, randomButton.bounds);
        closeButton.hovered = CheckCollisionPointRec(mousePos, closeButton.bounds);
        refreshButton.hovered = CheckCollisionPointRec(mousePos, refreshButton.bounds);
        upgradeButton.hovered = CheckCollisionPointRec(mousePos, upgradeButton.bounds);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Rectangle item1Bounds = { 200, 250, 200, 120 };
            if (CheckCollisionPointRec(mousePos, item1Bounds)) {
                BuyShopItem(currentShop.slot1);
            }

            Rectangle item2Bounds = { 450, 250, 200, 120 };
            if (CheckCollisionPointRec(mousePos, item2Bounds)) {
                BuyShopItem(currentShop.slot2);
            }

            Rectangle item3Bounds = { 700, 250, 200, 120 };
            if (CheckCollisionPointRec(mousePos, item3Bounds)) {
                BuyShopItem(currentShop.slot3);
            }

            if (randomButton.hovered) {
                if (player.gold >= randomCompanionPriceGold || player.kills >= randomCompanionPriceKills) {
                    if (player.gold >= randomCompanionPriceGold) {
                        player.gold -= randomCompanionPriceGold;
                    }
                    else {
                        player.kills -= randomCompanionPriceKills;
                    }

                    int randomType = GetRandomValue(1, 2);

                    if (randomType == 1) {
                        player.meleeCompanions++;
                        meleeAttackTimers.push_back(0);
                    }
                    else {
                        player.rangeCompanions++;
                        rangeAttackTimers.push_back(0);
                    }

                    purchaseCount++;
                    randomCompanionPriceGold = 300 * (int)pow(2, purchaseCount);
                    randomCompanionPriceKills = 30 + purchaseCount * 10;
                    UpdateInventoryDisplay();
                }
            }

            if (refreshButton.hovered) {
                if (freeRefreshUses > 0) {
                    freeRefreshUses--;
                    RefreshShop();
                }
                else if (player.kills >= currentShop.manualRefreshCost) {
                    player.kills -= currentShop.manualRefreshCost;
                    currentShop.manualRefreshCost += 20;
                    RefreshShop();
                }
            }

            if (upgradeButton.hovered && player.companionLevel < 6) {
                int upgradeCost = player.companionLevel * 500;
                if (player.gold >= upgradeCost) {
                    player.gold -= upgradeCost;
                    player.companionLevel++;
                    UpdateInventoryDisplay();
                }
            }

            if (closeButton.hovered) {
                inShop = false;
            }
        }
    }

    void BuyShopItem(const ShopItem& item) {
        bool canBuy = false;

        if (item.goldPrice > 0 && player.gold >= item.goldPrice) {
            player.gold -= item.goldPrice;
            canBuy = true;
        }
        else if (item.killsPrice > 0 && player.kills >= item.killsPrice) {
            player.kills -= item.killsPrice;
            canBuy = true;
        }

        if (canBuy) {
            ApplyShopItemEffect(item);
            RefreshShop();
        }
    }

    void ApplyShopItemEffect(const ShopItem& item) {
        switch (item.id) {
        case 1:
            attackCooldownReduction += 0.02f;
            break;
        case 2:
            movementSpeedBonus += 0.03f;
            break;
        case 3:
            extraEnemiesPerSpawn += 1;
            break;
        case 4:
            damageBonus += 0.02f;
            break;
        case 5:
        {
            int randomType = GetRandomValue(1, 2);

            if (randomType == 1) {
                player.meleeCompanions++;
                meleeAttackTimers.push_back(0);
            }
            else {
                player.rangeCompanions++;
                rangeAttackTimers.push_back(0);
            }
            UpdateInventoryDisplay();
        }
        break;
        case 6:
            freeRefreshUses += GetRandomValue(0, 6);
            break;
        }
    }

    void MergeCompanions() {
        int totalCompanions = player.meleeCompanions + player.rangeCompanions;

        if (totalCompanions >= 3 && player.companionLevel == 1) {
            int meleeToRemove = std::min(player.meleeCompanions, 2);
            int rangeToRemove = 3 - meleeToRemove;

            if (player.meleeCompanions >= meleeToRemove && player.rangeCompanions >= rangeToRemove) {
                player.meleeCompanions -= meleeToRemove;
                player.rangeCompanions -= rangeToRemove;
                player.marsCompanions += 1;

                if (meleeAttackTimers.size() >= meleeToRemove) {
                    meleeAttackTimers.erase(meleeAttackTimers.begin(), meleeAttackTimers.begin() + meleeToRemove);
                }
                if (rangeAttackTimers.size() >= rangeToRemove) {
                    rangeAttackTimers.erase(rangeAttackTimers.begin(), rangeAttackTimers.begin() + rangeToRemove);
                }
                marsAttackTimers.push_back(0);

                UpdateInventoryDisplay();
            }
        }
        else if (totalCompanions >= 3 && player.companionLevel > 1 && player.companionLevel < 6) {
            int meleeToRemove = std::min(player.meleeCompanions, 2);
            int rangeToRemove = 3 - meleeToRemove;

            if (player.meleeCompanions >= meleeToRemove && player.rangeCompanions >= rangeToRemove) {
                player.meleeCompanions -= meleeToRemove;
                player.rangeCompanions -= rangeToRemove;
                player.companionLevel++;

                if (meleeAttackTimers.size() >= meleeToRemove) {
                    meleeAttackTimers.erase(meleeAttackTimers.begin(), meleeAttackTimers.begin() + meleeToRemove);
                }
                if (rangeAttackTimers.size() >= rangeToRemove) {
                    rangeAttackTimers.erase(rangeAttackTimers.begin(), rangeAttackTimers.begin() + rangeToRemove);
                }

                UpdateInventoryDisplay();
            }
        }
    }

    void UpdateMainMenu(Button& playButton, Button& settingsButton) {
        Vector2 mousePos = GetMousePosition();

        playButton.hovered = CheckCollisionPointRec(mousePos, playButton.bounds);
        settingsButton.hovered = CheckCollisionPointRec(mousePos, settingsButton.bounds);

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

        backButton.hovered = CheckCollisionPointRec(mousePos, backButton.bounds);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (backButton.hovered) {
                inSettings = false;
            }
        }

        if (IsKeyDown(KEY_LEFT)) {
            musicVolume = std::max(0.0f, musicVolume - 0.01f);
        }
        if (IsKeyDown(KEY_RIGHT)) {
            musicVolume = std::min(1.0f, musicVolume + 0.01f);
        }

        SetMasterVolume(musicVolume);
    }

    void UpdateGameplay() {
        if (choosingWeapon) return;
        if (inShop) return;

        if (gameOver) {
            if (IsKeyPressed(KEY_ENTER)) {
                inGame = false;
            }
            return;
        }

        float deltaTime = GetFrameTime();

        shopRefreshTimer += deltaTime;
        if (shopRefreshTimer >= 60.0f) {
            RefreshShop();
            shopRefreshTimer = 0;
        }

        timeSinceLastSpawn += deltaTime;
        if (player.freezeCooldown > 0) {
            player.freezeCooldown -= deltaTime;
        }

        if (player.attackCooldown > 0) {
            player.attackCooldown -= deltaTime * (1.0f + attackCooldownReduction);
        }

        for (auto& timer : meleeAttackTimers) timer += deltaTime;
        for (auto& timer : rangeAttackTimers) timer += deltaTime;
        for (auto& timer : marsAttackTimers) timer += deltaTime;

        if (player.marsCompanions > 0) {
            auraAttackTimer += deltaTime;
        }

        if (IsKeyPressed(KEY_F)) {
            MergeCompanions();
        }

        UpdatePlayerMovement(deltaTime);
        gamestate.UpdateCamera(player.position);
        UpdateEnemySpawning(deltaTime);
        UpdateEnemies(deltaTime);
        UpdateProjectiles(deltaTime);
        CheckPlayerEnemyCollisions();
        CheckGameOverCondition(deltaTime);
        HandleAllCompanionAttacks();
        HandleWeaponAttack();
        HandleMarsAuraAttack();
    }

    void HandleAllCompanionAttacks() {
        float meleeCooldown = 1.1f / player.companionLevel;
        float rangeCooldown = 1.1f / player.companionLevel;
        float marsCooldown = 3.0f / player.companionLevel;

        for (size_t i = 0; i < meleeAttackTimers.size(); i++) {
            if (meleeAttackTimers[i] >= meleeCooldown) {
                PerformMeleeAutoAttack();
                meleeAttackTimers[i] = 0;
            }
        }

        for (size_t i = 0; i < rangeAttackTimers.size(); i++) {
            if (rangeAttackTimers[i] >= rangeCooldown) {
                PerformRangeAutoAttack();
                rangeAttackTimers[i] = 0;
            }
        }

        for (size_t i = 0; i < marsAttackTimers.size(); i++) {
            if (marsAttackTimers[i] >= marsCooldown) {
                Vector2 mouseScreenPos = GetMousePosition();
                Vector2 mouseWorldPos = {
                    mouseScreenPos.x + gamestate.cameraOffset.x,
                    mouseScreenPos.y + gamestate.cameraOffset.y
                };
                Vector2 attackDirection = {
                    mouseWorldPos.x - player.position.x,
                    mouseWorldPos.y - player.position.y
                };
                float length = sqrt(attackDirection.x * attackDirection.x + attackDirection.y * attackDirection.y);
                if (length > 0) {
                    attackDirection.x /= length;
                    attackDirection.y /= length;
                }
                CreateMarsWaveAttack(attackDirection);
                marsAttackTimers[i] = 0;
            }
        }
    }

    void HandleMarsAuraAttack() {
        if (player.marsCompanions > 0 && auraAttackTimer >= (0.3f / player.companionLevel)) {
            int auraDamage = 10 * player.companionLevel;
            float auraRange = 150.0f + (player.companionLevel - 1) * 20.0f;

            for (auto& enemy : enemies) {
                if (!enemy.active) continue;

                float distance = Vector2Distance(player.position, enemy.position);
                if (distance < auraRange) {
                    enemy.health -= auraDamage;
                    if (enemy.health <= 0) {
                        player.kills++;
                        player.gold += GetRandomValue(6, 11);
                    }
                }
            }
            auraAttackTimer = 0;
        }
    }

    void UpdatePlayerMovement(float deltaTime) {
        player.velocity = { 0, 0 };

        if (IsKeyDown(KEY_W)) player.velocity.y = -1;
        if (IsKeyDown(KEY_S)) player.velocity.y = 1;
        if (IsKeyDown(KEY_A)) player.velocity.x = -1;
        if (IsKeyDown(KEY_D)) player.velocity.x = 1;

        float length = sqrt(player.velocity.x * player.velocity.x + player.velocity.y * player.velocity.y);
        if (length > 0) {
            player.velocity.x /= length;
            player.velocity.y /= length;
        }

        float actualSpeed = player.speed * (1.0f + movementSpeedBonus);
        player.position.x += player.velocity.x * actualSpeed * deltaTime;
        player.position.y += player.velocity.y * actualSpeed * deltaTime;

        player.position.x = std::max(0.0f, std::min(gamestate.mapSize.x, player.position.x));
        player.position.y = std::max(0.0f, std::min(gamestate.mapSize.y, player.position.y));
    }

    void UpdateEnemySpawning(float deltaTime) {
        enemySpawnTimer += deltaTime;

        int enemiesToSpawn = 1 + extraEnemiesPerSpawn;

        if (enemySpawnTimer >= 0.6f && enemies.size() < MAX_ENEMIES) {
            for (int i = 0; i < enemiesToSpawn; i++) {
                SpawnEnemy();
            }
            enemySpawnTimer = 0;
        }
    }

    void SpawnEnemy() {
        float angle = GetRandomValue(0, 360) * 3.14159f / 180.0f;
        float distance = GetRandomValue(400, 600);

        Vector2 spawnPos = {
            player.position.x + cos(angle) * distance,
            player.position.y + sin(angle) * distance
        };

        spawnPos.x = std::max(0.0f, std::min(gamestate.mapSize.x, spawnPos.x));
        spawnPos.y = std::max(0.0f, std::min(gamestate.mapSize.y, spawnPos.y));

        enemies.emplace_back(spawnPos);
    }

    void UpdateEnemies(float deltaTime) {
        for (size_t i = 0; i < enemies.size(); i++) {
            if (!enemies[i].active) continue;

            for (size_t j = i + 1; j < enemies.size(); j++) {
                if (!enemies[j].active) continue;

                float distance = Vector2Distance(enemies[i].position, enemies[j].position);
                if (distance < 40.0f) {
                    Vector2 direction = {
                        enemies[i].position.x - enemies[j].position.x,
                        enemies[i].position.y - enemies[j].position.y
                    };

                    float length = sqrt(direction.x * direction.x + direction.y * direction.y);
                    if (length > 0) {
                        direction.x /= length;
                        direction.y /= length;
                    }

                    float pushForce = 5.0f;
                    enemies[i].position.x += direction.x * pushForce;
                    enemies[i].position.y += direction.y * pushForce;
                    enemies[j].position.x -= direction.x * pushForce;
                    enemies[j].position.y -= direction.y * pushForce;
                }
            }
        }

        for (auto& enemy : enemies) {
            if (!enemy.active) continue;

            if (enemy.frozenTimer > 0) {
                enemy.frozenTimer -= deltaTime;
                continue;
            }

            Vector2 direction = {
                player.position.x - enemy.position.x,
                player.position.y - enemy.position.y
            };

            float length = sqrt(direction.x * direction.x + direction.y * direction.y);
            if (length > 0) {
                direction.x /= length;
                direction.y /= length;
            }

            enemy.position.x += direction.x * 110.0f * deltaTime;
            enemy.position.y += direction.y * 110.0f * deltaTime;
        }

        enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
            [](const Enemy& e) { return !e.active || e.health <= 0; }), enemies.end());
    }

    void UpdateProjectiles(float deltaTime) {
        for (auto& projectile : projectiles) {
            if (!projectile.active) continue;

            projectile.position.x += projectile.velocity.x * deltaTime;
            projectile.position.y += projectile.velocity.y * deltaTime;

            for (auto& enemy : enemies) {
                if (!enemy.active) continue;

                float distance = Vector2Distance(projectile.position, enemy.position);
                float collisionDistance = projectile.isMarsWave ? 50.0f : 30.0f;

                if (distance < collisionDistance) {
                    int actualDamage = projectile.damage * (1.0f + damageBonus);
                    enemy.health -= actualDamage;

                    if (enemy.health <= 0) {
                        player.kills++;
                        player.gold += GetRandomValue(6, 11);
                    }

                    if (projectile.isFreezing) {
                        enemy.frozenTimer = 3.0f;
                    }

                    if (!projectile.isMarsSpear && !projectile.isMarsWave) {
                        projectile.active = false;
                        break;
                    }
                }
            }

            if (projectile.position.x < 0 || projectile.position.x > gamestate.mapSize.x ||
                projectile.position.y < 0 || projectile.position.y > gamestate.mapSize.y) {
                projectile.active = false;
            }
        }

        projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile& p) { return !p.active; }), projectiles.end());
    }

    void HandleWeaponAttack() {
        int weaponType = GetSelectedWeaponType();
        if (weaponType == 0) return;

        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && player.attackCooldown <= 0) {
            Vector2 mouseScreenPos = GetMousePosition();
            Vector2 mouseWorldPos = {
                mouseScreenPos.x + gamestate.cameraOffset.x,
                mouseScreenPos.y + gamestate.cameraOffset.y
            };

            if (weaponType == 1) {
                Enemy* target = nullptr;
                float minDist = 100.0f;

                for (auto& enemy : enemies) {
                    if (!enemy.active) continue;

                    float distance = Vector2Distance(mouseWorldPos, enemy.position);
                    if (distance < minDist) {
                        minDist = distance;
                        target = &enemy;
                    }
                }

                if (target) {
                    std::vector<Enemy*> targets = { target };

                    for (auto& enemy : enemies) {
                        if (!enemy.active || &enemy == target) continue;

                        float distance = Vector2Distance(target->position, enemy.position);
                        if (distance < 80.0f && targets.size() < (5 * player.companionLevel)) {
                            targets.push_back(&enemy);
                        }
                    }

                    for (auto* enemyTarget : targets) {
                        int actualDamage = 40 * player.companionLevel * (1.0f + damageBonus);
                        enemyTarget->health -= actualDamage;

                        if (enemyTarget->health <= 0) {
                            player.kills++;
                            player.gold += GetRandomValue(6, 11);
                        }
                    }

                    player.attackCooldown = 0.3f;
                }
            }
            else if (weaponType == 2) {
                Enemy* target = nullptr;
                float minDist = 150.0f;

                for (auto& enemy : enemies) {
                    if (!enemy.active) continue;

                    float distance = Vector2Distance(mouseWorldPos, enemy.position);
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

                    int actualDamage = 40 * player.companionLevel * (1.0f + damageBonus);
                    projectiles.emplace_back(player.position,
                        Vector2{ direction.x * 300.0f, direction.y * 300.0f }, true, actualDamage);

                    player.attackCooldown = 0.3f;
                }
            }
            else if (weaponType == 3) {
                player.attackCooldown = 0.3f;
            }
        }
    }

    void PerformMeleeAutoAttack() {
        std::vector<Enemy*> nearbyEnemies;

        for (auto& enemy : enemies) {
            if (!enemy.active) continue;

            float distance = Vector2Distance(player.position, enemy.position);
            if (distance < 100.0f) {
                nearbyEnemies.push_back(&enemy);
            }
        }

        std::sort(nearbyEnemies.begin(), nearbyEnemies.end(),
            [this](Enemy* a, Enemy* b) {
                return Vector2Distance(player.position, a->position) <
                    Vector2Distance(player.position, b->position);
            });

        int targetsToAttack = std::min(5 * player.companionLevel, (int)nearbyEnemies.size());
        for (int i = 0; i < targetsToAttack; i++) {
            Enemy* target = nearbyEnemies[i];
            int actualDamage = 40 * player.companionLevel * (1.0f + damageBonus);
            target->health -= actualDamage;

            if (target->health <= 0) {
                player.kills++;
                player.gold += GetRandomValue(6, 11);
            }
        }
    }

    void PerformRangeAutoAttack() {
        std::vector<Enemy*> nearbyEnemies;

        for (auto& enemy : enemies) {
            if (!enemy.active) continue;

            float distance = Vector2Distance(player.position, enemy.position);
            if (distance < 250.0f) {
                nearbyEnemies.push_back(&enemy);
            }
        }

        std::sort(nearbyEnemies.begin(), nearbyEnemies.end(),
            [this](Enemy* a, Enemy* b) {
                return Vector2Distance(player.position, a->position) <
                    Vector2Distance(player.position, b->position);
            });

        int targetsToAttack = std::min(3 * player.companionLevel, (int)nearbyEnemies.size());
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

            int actualDamage = 30 * player.companionLevel * (1.0f + damageBonus);
            projectiles.emplace_back(player.position,
                Vector2{ direction.x * 250.0f, direction.y * 250.0f }, true, actualDamage);
        }
    }

    void CreateMarsWaveAttack(Vector2 direction) {
        int numProjectiles = 7;
        float spreadAngle = 180.0f * 3.14159f / 180.0f;

        for (int i = 0; i < numProjectiles; i++) {
            float angle = -spreadAngle / 2 + (spreadAngle / (numProjectiles - 1)) * i;

            float cosA = cos(angle);
            float sinA = sin(angle);

            Vector2 projectileDirection = {
                direction.x * cosA - direction.y * sinA,
                direction.x * sinA + direction.y * cosA
            };

            int actualDamage = 60 * player.companionLevel * (1.0f + damageBonus);
            projectiles.emplace_back(
                player.position,
                Vector2{ projectileDirection.x * 200.0f, projectileDirection.y * 200.0f },
                false, actualDamage, false, true, 40.0f
            );
        }
    }

    void CheckPlayerEnemyCollisions() {
        for (auto& enemy : enemies) {
            if (!enemy.active) continue;

            float distance = Vector2Distance(player.position, enemy.position);
            if (distance < 40.0f) {
                player.health -= 5;

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

        if (player.health <= 0) {
            gameOver = true;
        }
    }

    void CheckGameOverCondition(float deltaTime) {
        if (enemies.size() > MAX_ENEMIES || player.health <= 0) {
            gameOverTimer -= deltaTime;
            if (gameOverTimer <= 0) {
                gameOver = true;
            }
        }
        else {
            gameOverTimer = GAME_OVER_TIMER;
        }
    }

    void DrawWeaponChoice(Button& meleeButton, Button& rangeButton) {
        ClearBackground(BLACK);

        if (backgroundTexture.id != 0) {
            DrawTexture(backgroundTexture, 0, 0, WHITE);
        }

        DrawText("CHOOSE YOUR COMPANION", SCREEN_WIDTH / 2 - MeasureText("CHOOSE YOUR COMPANION", 40) / 2, 200, 40, WHITE);

        DrawRectangleRec(meleeButton.bounds, meleeButton.hovered ? GRAY : DARKGRAY);

        if (meleeTexture.id != 0) {
            DrawTexture(meleeTexture, meleeButton.bounds.x + 10, meleeButton.bounds.y + 10, WHITE);
        }
        else {
            DrawRectangle(meleeButton.bounds.x + 10, meleeButton.bounds.y + 10, 30, 30, VIOLET);
        }

        DrawText(meleeButton.text.c_str(),
            meleeButton.bounds.x + meleeButton.bounds.width / 2 - MeasureText(meleeButton.text.c_str(), 30) / 2,
            meleeButton.bounds.y + meleeButton.bounds.height / 2 - 15, 30, WHITE);

        DrawRectangleRec(rangeButton.bounds, rangeButton.hovered ? GRAY : DARKGRAY);

        if (rangeTexture.id != 0) {
            DrawTexture(rangeTexture, rangeButton.bounds.x + 10, rangeButton.bounds.y + 10, WHITE);
        }
        else {
            DrawRectangle(rangeButton.bounds.x + 10, rangeButton.bounds.y + 10, 30, 30, YELLOW);
        }

        DrawText(rangeButton.text.c_str(),
            rangeButton.bounds.x + rangeButton.bounds.width / 2 - MeasureText(rangeButton.text.c_str(), 30) / 2,
            rangeButton.bounds.y + rangeButton.bounds.height / 2 - 15, 30, WHITE);
    }

    void DrawShop(Button& randomButton, Button& closeButton, Button& refreshButton, Button& upgradeButton) {
        ClearBackground(BLACK);

        if (backgroundTexture.id != 0) {
            DrawTexture(backgroundTexture, 0, 0, WHITE);
        }

        DrawText("SHOP", SCREEN_WIDTH / 2 - MeasureText("SHOP", 60) / 2, 80, 60, WHITE);

        DrawText(("Gold: " + std::to_string(player.gold)).c_str(), 80, 150, 30, YELLOW);
        DrawText(("Kills: " + std::to_string(player.kills)).c_str(), 80, 190, 30, WHITE);

        DrawShopItem(currentShop.slot1, 200, 250);
        DrawShopItem(currentShop.slot2, 450, 250);
        DrawShopItem(currentShop.slot3, 700, 250);

        DrawRectangleRec(randomButton.bounds, randomButton.hovered ? GRAY : DARKGRAY);
        DrawText("RANDOM", randomButton.bounds.x + randomButton.bounds.width / 2 - MeasureText("RANDOM", 25) / 2,
            randomButton.bounds.y + 10, 25, WHITE);
        DrawText("COMPANION", randomButton.bounds.x + randomButton.bounds.width / 2 - MeasureText("COMPANION", 20) / 2,
            randomButton.bounds.y + 40, 20, WHITE);
        DrawText(("Gold: " + std::to_string(randomCompanionPriceGold)).c_str(), randomButton.bounds.x + 10, randomButton.bounds.y + 70, 15,
            player.gold >= randomCompanionPriceGold ? GREEN : RED);
        DrawText(("Kills: " + std::to_string(randomCompanionPriceKills)).c_str(), randomButton.bounds.x + 10, randomButton.bounds.y + 90, 15,
            player.kills >= randomCompanionPriceKills ? GREEN : RED);

        DrawRectangleRec(refreshButton.bounds, refreshButton.hovered ? GRAY : DARKGRAY);
        DrawText("REFRESH", refreshButton.bounds.x + refreshButton.bounds.width / 2 - MeasureText("REFRESH", 25) / 2,
            refreshButton.bounds.y + 10, 25, WHITE);
        DrawText("SHOP", refreshButton.bounds.x + refreshButton.bounds.width / 2 - MeasureText("SHOP", 20) / 2,
            refreshButton.bounds.y + 40, 20, WHITE);

        if (freeRefreshUses > 0) {
            DrawText(("Free: " + std::to_string(freeRefreshUses)).c_str(), refreshButton.bounds.x + 10, refreshButton.bounds.y + 70, 15, GREEN);
        }
        else {
            DrawText(("Kills: " + std::to_string(currentShop.manualRefreshCost)).c_str(), refreshButton.bounds.x + 10, refreshButton.bounds.y + 70, 15,
                player.kills >= currentShop.manualRefreshCost ? GREEN : RED);
        }

        DrawRectangleRec(upgradeButton.bounds, upgradeButton.hovered ? GRAY : DARKGRAY);
        DrawText("UPGRADE", upgradeButton.bounds.x + upgradeButton.bounds.width / 2 - MeasureText("UPGRADE", 25) / 2,
            upgradeButton.bounds.y + 10, 25, WHITE);
        DrawText("COMPANIONS", upgradeButton.bounds.x + upgradeButton.bounds.width / 2 - MeasureText("COMPANIONS", 18) / 2,
            upgradeButton.bounds.y + 40, 18, WHITE);

        if (player.companionLevel < 6) {
            int upgradeCost = player.companionLevel * 500;
            std::string costText = "Cost: " + std::to_string(upgradeCost) + " gold";
            DrawText(costText.c_str(), upgradeButton.bounds.x + 10, upgradeButton.bounds.y + 70, 15,
                player.gold >= upgradeCost ? GREEN : RED);
            std::string levelText = "Level: " + std::to_string(player.companionLevel) + " -> " + std::to_string(player.companionLevel + 1);
            DrawText(levelText.c_str(), upgradeButton.bounds.x + 10, upgradeButton.bounds.y + 90, 15, WHITE);
        }
        else {
            DrawText("MAX LEVEL", upgradeButton.bounds.x + 10, upgradeButton.bounds.y + 70, 15, GOLD);
        }

        DrawRectangleRec(closeButton.bounds, closeButton.hovered ? GRAY : DARKGRAY);
        DrawText(closeButton.text.c_str(),
            closeButton.bounds.x + closeButton.bounds.width / 2 - MeasureText(closeButton.text.c_str(), 30) / 2,
            closeButton.bounds.y + closeButton.bounds.height / 2 - 15, 30, WHITE);

        DrawText(("Melee Companions: " + std::to_string(player.meleeCompanions)).c_str(), 80, 250, 25, VIOLET);
        DrawText(("Range Companions: " + std::to_string(player.rangeCompanions)).c_str(), 80, 285, 25, YELLOW);
        DrawText(("Mars Companions: " + std::to_string(player.marsCompanions)).c_str(), 80, 320, 25, ORANGE);
        std::string levelText = "Companion Level: " + GetStarString(player.companionLevel);
        DrawText(levelText.c_str(), 80, 355, 25, WHITE);

        DrawText("Press F to merge 3 companions", 80, 390, 20, WHITE);
        DrawText("3x 1★ = 1 Mars or Level Up", 80, 415, 20, WHITE);

        DrawText(("Attack CD Reduction: " + std::to_string((int)(attackCooldownReduction * 100)) + "%").c_str(), 800, 320, 20, BLUE);
        DrawText(("Movement Speed: +" + std::to_string((int)(movementSpeedBonus * 100)) + "%").c_str(), 800, 350, 20, BLUE);
        DrawText(("Extra Enemies: " + std::to_string(extraEnemiesPerSpawn)).c_str(), 800, 380, 20, BLUE);
        DrawText(("Damage Bonus: +" + std::to_string((int)(damageBonus * 100)) + "%").c_str(), 800, 410, 20, BLUE);

        if (player.marsCompanions > 0) {
            std::string auraText = "MARS AURA: " + std::to_string(10 * player.companionLevel) + " damage around player";
            DrawText(auraText.c_str(), 800, 440, 20, ORANGE);
        }
    }

    void DrawShopItem(const ShopItem& item, int x, int y) {
        Rectangle itemBounds = { (float)x, (float)y, 200, 120 };
        Color bgColor = item.tier == 1 ? DARKGRAY : (item.tier == 2 ? DARKBLUE : DARKPURPLE);

        DrawRectangleRec(itemBounds, bgColor);
        DrawRectangleLines((int)itemBounds.x, (int)itemBounds.y, (int)itemBounds.width, (int)itemBounds.height, WHITE);

        DrawText(item.name.c_str(), x + 10, y + 10, 20, WHITE);

        std::string desc = item.description;
        size_t pos = desc.find('\n');
        if (pos != std::string::npos) {
            DrawText(desc.substr(0, pos).c_str(), x + 10, y + 35, 16, LIGHTGRAY);
            DrawText(desc.substr(pos + 1).c_str(), x + 10, y + 55, 16, LIGHTGRAY);
        }
        else {
            DrawText(desc.c_str(), x + 10, y + 35, 16, LIGHTGRAY);
        }

        if (item.goldPrice > 0) {
            std::string goldText = "Gold: " + std::to_string(item.goldPrice);
            DrawText(goldText.c_str(), x + 10, y + 85, 18,
                player.gold >= item.goldPrice ? GREEN : RED);
        }
        if (item.killsPrice > 0) {
            std::string killsText = "Kills: " + std::to_string(item.killsPrice);
            DrawText(killsText.c_str(), x + 10, y + 105, 18,
                player.kills >= item.killsPrice ? GREEN : RED);
        }
    }

    void DrawMainMenu(const Button& playButton, const Button& settingsButton) {
        if (menuBackgroundLoaded) {
            DrawTexture(menuBackgroundTexture, 0, 0, WHITE);
        }
        else {
            ClearBackground(BLACK);
        }

        DrawText("ODIUM", SCREEN_WIDTH / 2 - MeasureText("ODIUM", 80) / 2, 200, 80, WHITE);

        DrawRectangleRec(playButton.bounds, playButton.hovered ? GRAY : DARKGRAY);
        DrawText(playButton.text.c_str(),
            playButton.bounds.x + playButton.bounds.width / 2 - MeasureText(playButton.text.c_str(), 30) / 2,
            playButton.bounds.y + playButton.bounds.height / 2 - 15, 30, WHITE);

        DrawRectangleRec(settingsButton.bounds, settingsButton.hovered ? GRAY : DARKGRAY);
        DrawText(settingsButton.text.c_str(),
            settingsButton.bounds.x + settingsButton.bounds.width / 2 - MeasureText(settingsButton.text.c_str(), 30) / 2,
            settingsButton.bounds.y + settingsButton.bounds.height / 2 - 15, 30, WHITE);
    }

    void DrawSettings(const Button& backButton) {
        ClearBackground(BLACK);

        if (backgroundTexture.id != 0) {
            DrawTexture(backgroundTexture, 0, 0, WHITE);
        }

        DrawText("SETTINGS", SCREEN_WIDTH / 2 - MeasureText("SETTINGS", 50) / 2, 150, 50, WHITE);

        DrawText("MUSIC VOLUME:", 400, 250, 30, WHITE);

        Rectangle sliderBar = { 600, 255, 300, 20 };
        Rectangle sliderHandle = { 600 + musicVolume * 300 - 5, 250, 10, 30 };

        DrawRectangleRec(sliderBar, DARKGRAY);
        DrawRectangleRec(sliderHandle, WHITE);

        std::string volumeText = std::to_string((int)(musicVolume * 100)) + "%";
        DrawText(volumeText.c_str(), 910, 255, 20, WHITE);

        DrawRectangleRec(backButton.bounds, backButton.hovered ? GRAY : DARKGRAY);
        DrawText(backButton.text.c_str(),
            backButton.bounds.x + backButton.bounds.width / 2 - MeasureText(backButton.text.c_str(), 30) / 2,
            backButton.bounds.y + backButton.bounds.height / 2 - 15, 30, WHITE);
    }

    void DrawGameplay() {
        BeginDrawing();

        // Очищаем экран
        ClearBackground(BLACK);

        // Рисуем фон со смещением (параллакс эффект)
        if (backgroundTexture.id != 0) {
            // Можно добавить параллакс эффект, смещая фон медленнее чем игрок
            float parallaxFactor = 0.5f;
            DrawTexture(backgroundTexture,
                -gamestate.cameraOffset.x * parallaxFactor,
                -gamestate.cameraOffset.y * parallaxFactor, WHITE);
        }

        // Рисуем все игровые объекты с учетом смещения камеры
        {
            // Аура Mars
            if (player.marsCompanions > 0) {
                Vector2 screenPos = gamestate.WorldToScreen(player.position);
                float auraRange = 150.0f + (player.companionLevel - 1) * 20.0f;
                DrawCircleLines((int)screenPos.x, (int)screenPos.y, auraRange, Fade(ORANGE, 0.3f));
            }

            // Враги
            for (const auto& enemy : enemies) {
                if (!enemy.active) continue;

                Vector2 screenPos = gamestate.WorldToScreen(enemy.position);

                if (enemy.frozenTimer > 0) {
                    DrawRectangle((int)screenPos.x - 20, (int)screenPos.y - 20, 40, 40, SKYBLUE);
                }
                else {
                    DrawRectangle((int)screenPos.x - 20, (int)screenPos.y - 20, 40, 40, BLUE);
                }

                float healthPercent = (float)enemy.health / enemy.maxHealth;
                DrawRectangle((int)screenPos.x - 20, (int)screenPos.y - 30, 40, 5, RED);
                DrawRectangle((int)screenPos.x - 20, (int)screenPos.y - 30, (int)(40 * healthPercent), 5, GREEN);
            }

            // Снаряды
            for (const auto& projectile : projectiles) {
                if (!projectile.active) continue;

                Vector2 screenPos = gamestate.WorldToScreen(projectile.position);

                if (projectile.isMarsWave) {
                    DrawCircle((int)screenPos.x, (int)screenPos.y, projectile.size / 2, ORANGE);
                    DrawCircleLines((int)screenPos.x, (int)screenPos.y, projectile.size / 2, Color{ 255, 140, 0, 255 });
                }
                else if (projectile.isMarsSpear) {
                    DrawRectangle((int)screenPos.x - 8, (int)screenPos.y - 8, 16, 16, ORANGE);
                }
                else if (projectile.isFreezing) {
                    DrawRectangle((int)screenPos.x - 10, (int)screenPos.y - 10, 20, 20, SKYBLUE);
                }
                else {
                    DrawRectangle((int)screenPos.x - 10, (int)screenPos.y - 10, 20, 20, YELLOW);
                }
            }

            // Игрок
            Vector2 playerScreenPos = gamestate.WorldToScreen(player.position);
            DrawRectangle((int)playerScreenPos.x - 25, (int)playerScreenPos.y - 25, 50, 50, RED);
        }

        DrawUI();
        DrawMinimap();
        DrawInventory();

        // Кнопка магазина
        Rectangle shopButton = { SCREEN_WIDTH - 140, 20, 120, 50 };
        bool shopHovered = CheckCollisionPointRec(GetMousePosition(), shopButton);

        DrawRectangleRec(shopButton, shopHovered ? GRAY : DARKGRAY);
        DrawText("SHOP", SCREEN_WIDTH - 130, 35, 20, WHITE);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && shopHovered) {
            inShop = true;
        }

        EndDrawing();
    }

    void DrawMinimap() {
        int minimapSize = 180;
        int minimapX = 20;
        int minimapY = SCREEN_HEIGHT - minimapSize - 52 * 2 - 30;

        DrawRectangle(minimapX, minimapY, minimapSize, minimapSize, DARKGREEN);
        DrawRectangleLines(minimapX, minimapY, minimapSize, minimapSize, WHITE);

        float scaleX = (float)minimapSize / gamestate.mapSize.x;
        float scaleY = (float)minimapSize / gamestate.mapSize.y;

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

        int playerX = minimapX + (int)(player.position.x * scaleX);
        int playerY = minimapY + (int)(player.position.y * scaleY);
        DrawRectangle(playerX - 3, playerY - 3, 6, 6, RED);
    }

    void DrawInventory() {
        Vector2 mousePos = GetMousePosition();
        std::string hoverDescription = "";

        if (inventoryTexture.id != 0) {
            int totalWidth = 102 * 3;
            int totalHeight = 52 * 2;
            int textureX = (SCREEN_WIDTH - totalWidth) / 2;
            int textureY = SCREEN_HEIGHT - totalHeight - 20;
            DrawTexture(inventoryTexture, textureX, textureY, WHITE);
        }

        for (const auto& item : inventory) {
            if (item.type == 1 && meleeTexture.id != 0) {
                DrawTexture(meleeTexture, item.slot.x + 10, item.slot.y + 10, WHITE);
            }
            else if (item.type == 2 && rangeTexture.id != 0) {
                DrawTexture(rangeTexture, item.slot.x + 10, item.slot.y + 10, WHITE);
            }
            else if (item.type == 3 && marsTexture.id != 0) {
                DrawTexture(marsTexture, item.slot.x + 10, item.slot.y + 10, WHITE);
            }
            else if (item.type == 1) {
                DrawRectangle(item.slot.x + 10, item.slot.y + 10, 30, 30, VIOLET);
            }
            else if (item.type == 2) {
                DrawRectangle(item.slot.x + 10, item.slot.y + 10, 30, 30, YELLOW);
            }
            else if (item.type == 3) {
                DrawRectangle(item.slot.x + 10, item.slot.y + 10, 30, 30, ORANGE);
            }

            if (CheckCollisionPointRec(mousePos, item.slot) && item.type != 0) {
                hoverDescription = item.description;
            }
        }

        if (!hoverDescription.empty()) {
            int descWidth = 400;
            int descHeight = 150;
            DrawRectangle(20, 20, descWidth, descHeight, Fade(BLACK, 0.8f));
            DrawText(hoverDescription.c_str(), 30, 30, 18, WHITE);
        }
    }

    void DrawUI() {
        int startY = 20;

        std::string enemyCountText = "Enemies: " + std::to_string(enemies.size()) + "/" + std::to_string(MAX_ENEMIES);
        DrawText(enemyCountText.c_str(), 20, startY, 20, WHITE);

        std::string healthText = "HP: " + std::to_string(player.health) + "/" + std::to_string(PLAYER_MAX_HEALTH);
        DrawText(healthText.c_str(), 20, startY + 30, 20, GREEN);

        float playerHealthPercent = (float)player.health / player.maxHealth;
        DrawRectangle(120, startY + 35, 150, 10, RED);
        DrawRectangle(120, startY + 35, (int)(150 * playerHealthPercent), 10, GREEN);

        std::string goldText = "Gold: " + std::to_string(player.gold);
        DrawText(goldText.c_str(), 20, startY + 60, 20, YELLOW);

        std::string killsText = "Kills: " + std::to_string(player.kills);
        DrawText(killsText.c_str(), 20, startY + 90, 20, WHITE);

        DrawText(("Melee: " + std::to_string(player.meleeCompanions)).c_str(), 20, startY + 120, 20, VIOLET);
        DrawText(("Range: " + std::to_string(player.rangeCompanions)).c_str(), 20, startY + 150, 20, YELLOW);
        DrawText(("Mars: " + std::to_string(player.marsCompanions)).c_str(), 20, startY + 180, 20, ORANGE);
        std::string levelText = "Level: " + GetStarString(player.companionLevel);
        DrawText(levelText.c_str(), 20, startY + 210, 20, WHITE);

        if (enemies.size() > MAX_ENEMIES) {
            std::string timerText = "Time: " + std::to_string((int)gameOverTimer);
            DrawText(timerText.c_str(), 20, startY + 240, 20, RED);
        }

        DrawText("RMB: Attack with companion", 20, startY + 270, 20, WHITE);
        DrawText("F: Merge 3 companions", 20, startY + 300, 20, WHITE);

        DrawText(("CD Reduction: " + std::to_string((int)(attackCooldownReduction * 100)) + "%").c_str(), 20, startY + 330, 18, BLUE);
        DrawText(("Speed: +" + std::to_string((int)(movementSpeedBonus * 100)) + "%").c_str(), 20, startY + 355, 18, BLUE);
        DrawText(("Damage: +" + std::to_string((int)(damageBonus * 100)) + "%").c_str(), 20, startY + 380, 18, BLUE);

        if (player.marsCompanions > 0) {
            std::string auraText = "MARS AURA: " + std::to_string(10 * player.companionLevel) + " damage around player";
            DrawText(auraText.c_str(), 20, startY + 405, 18, ORANGE);
        }

        if (gameOver) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.5f));
            DrawText("GAME OVER", SCREEN_WIDTH / 2 - MeasureText("GAME OVER", 60) / 2, SCREEN_HEIGHT / 2 - 50, 60, RED);
            DrawText("Press ENTER to exit", SCREEN_WIDTH / 2 - MeasureText("Press ENTER to exit", 30) / 2, SCREEN_HEIGHT / 2 + 20, 30, WHITE);
        }
    }

    void Run() {
        Button playButton = { {SCREEN_WIDTH / 2 - 100, 350, 200, 50}, "PLAY", false };
        Button settingsButton = { {SCREEN_WIDTH / 2 - 100, 420, 200, 50}, "SETTINGS", false };
        Button backButton = { {SCREEN_WIDTH / 2 - 100, 500, 200, 50}, "BACK", false };

        Button meleeButton = { {SCREEN_WIDTH / 2 - 150, 300, 300, 80}, "MELEE COMPANION", false };
        Button rangeButton = { {SCREEN_WIDTH / 2 - 150, 400, 300, 80}, "RANGE COMPANION", false };

        Button randomButton = { {100, 500, 200, 120}, "RANDOM COMPANION", false };
        Button refreshButton = { {350, 500, 200, 120}, "REFRESH SHOP", false };
        Button upgradeButton = { {600, 500, 200, 120}, "UPGRADE COMPANIONS", false };
        Button shopCloseButton = { {850, 500, 200, 50}, "CLOSE", false };

        while (!WindowShouldClose()) {
            if (inGame) {
                if (choosingWeapon) {
                    UpdateWeaponChoice(meleeButton, rangeButton);
                    BeginDrawing();
                    DrawWeaponChoice(meleeButton, rangeButton);
                    EndDrawing();
                }
                else if (inShop) {
                    UpdateShop(randomButton, shopCloseButton, refreshButton, upgradeButton);
                    BeginDrawing();
                    DrawShop(randomButton, shopCloseButton, refreshButton, upgradeButton);
                    EndDrawing();
                }
                else {
                    UpdateGameplay();
                    DrawGameplay();
                }
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
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Odium - Survivor Game");
    SetTargetFPS(60);

    Game game;
    game.Run();

    CloseWindow();

    return 0;
}