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
const int MAX_INVENTORY_SLOTS = 6;

// Структура для кнопок
struct Button {
    Rectangle bounds;
    std::string text;
    bool hovered;
};

// Структура для компаньонов
struct Companion {
    int type;           // 1 - melee, 2 - range, 3 - mars, 4 - ice, 5 - fire, 6 - lightning
    int starLevel;      // Уровень звезды (1-6)
    float attackTimer;
    std::string name;

    Companion(int t, int stars) : type(t), starLevel(stars), attackTimer(0) {
        // Устанавливаем имя в зависимости от типа
        switch (type) {
        case 1: name = "Warrior"; break;
        case 2: name = "Archer"; break;
        case 3: name = "Mars"; break;
        case 4: name = "Ice Mage"; break;
        case 5: name = "Fire Mage"; break;
        case 6: name = "Lightning Mage"; break;
        default: name = "Unknown";
        }
    }
};

// Структура для предметов инвентаря
struct InventoryItem {
    int type = 0;
    int starLevel = 1;
    Rectangle slot = { 0, 0, 0, 0 };
    std::string description = "Empty Slot";
};

// Структура для врагов
struct Enemy {
    Vector2 position;
    Vector2 velocity = { 0, 0 };
    bool active;
    float frozenTimer = 0;
    float burnTimer = 0;
    float stunTimer = 0;
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
    bool isBurning;
    bool isElectrifying;
    bool isMarsSpear;
    bool isMarsWave;
    float size;
    int damage;
    int companionType;

    Projectile(Vector2 pos, Vector2 vel, bool freezing = false, bool burning = false,
        bool electrifying = false, int dmg = 0, bool marsSpear = false,
        bool marsWave = false, float projectileSize = 20.0f, int compType = 0)
        : position(pos), velocity(vel), active(true), isFreezing(freezing),
        isBurning(burning), isElectrifying(electrifying), damage(dmg),
        isMarsSpear(marsSpear), isMarsWave(marsWave), size(projectileSize), companionType(compType) {}
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

    Player() : position({ 0, 0 }), speed(120.0f),
        passiveAttackTimer(0), freezeCooldown(0), health(PLAYER_MAX_HEALTH), maxHealth(PLAYER_MAX_HEALTH),
        attackCooldown(0), gold(0), kills(0) {}
};

// Структура GameState для управления состоянием игры
struct GameState {
    Vector2 mapSize;
    Vector2 cameraOffset;

    GameState() : mapSize({ 5000.0f, 5000.0f }), cameraOffset({ 0, 0 }) {}

    void UpdateCamera(Vector2 playerPosition) {
        cameraOffset.x = playerPosition.x - SCREEN_WIDTH / 2;
        cameraOffset.y = playerPosition.y - SCREEN_HEIGHT / 2;

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
    std::vector<Companion> companions; // Все компаньоны хранятся здесь

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
    Texture2D iceTexture;
    Texture2D fireTexture;
    Texture2D lightningTexture;
    Texture2D backgroundTexture;
    bool texturesLoaded;
    Texture2D menuBackgroundTexture;
    bool menuBackgroundLoaded;

public:
    Game() : enemySpawnTimer(0), gameOverTimer(GAME_OVER_TIMER), gameOver(false),
        inGame(false), inSettings(false), musicVolume(0.5f),
        timeSinceLastSpawn(0), choosingWeapon(false), inShop(false),
        randomCompanionPriceGold(300), randomCompanionPriceKills(30),
        purchaseCount(0), attackCooldownReduction(0), movementSpeedBonus(0),
        extraEnemiesPerSpawn(0), damageBonus(0), pocketHeroUses(0), freeRefreshUses(0),
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
        if (FileExists("ice.png")) {
            iceTexture = LoadTexture("ice.png");
        }
        if (FileExists("fire.png")) {
            fireTexture = LoadTexture("fire.png");
        }
        if (FileExists("lightning.png")) {
            lightningTexture = LoadTexture("lightning.png");
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
            if (iceTexture.id != 0) UnloadTexture(iceTexture);
            if (fireTexture.id != 0) UnloadTexture(fireTexture);
            if (lightningTexture.id != 0) UnloadTexture(lightningTexture);
            if (backgroundTexture.id != 0) UnloadTexture(backgroundTexture);
            if (menuBackgroundLoaded) {
                UnloadTexture(menuBackgroundTexture);
            }
        }
    }

    // БАЗА ДАННЫХ КОМПАНЬОНОВ
    struct CompanionData {
        int type;
        std::string name;
        std::string description;
        Color color;
        float baseCooldown;
        int baseDamage;
        int targets;
        std::string ability;
    };

    std::vector<CompanionData> companionDatabase = {
        {1, "Warrior", "Melee fighter with area attacks", RED, 1.1f, 40, 5, "Cleaves multiple enemies"},
        {2, "Archer", "Ranged attacker with freezing arrows", GREEN, 1.1f, 30, 3, "Freezes enemies on hit"},
        {3, "Mars", "God of war with wave attacks", ORANGE, 3.0f, 60, 7, "Sends shockwaves in semicircle"},
        {4, "Ice Mage", "Master of frost and cold", SKYBLUE, 2.0f, 35, 4, "Slows and damages groups"},
        {5, "Fire Mage", "Wielder of destructive flames", Color{255, 69, 0, 255}, 1.5f, 45, 3, "Burns enemies over time"},
        {6, "Lightning Mage", "Controller of electric energy", YELLOW, 2.5f, 50, 6, "Chains lightning between enemies"}
    };

    CompanionData GetCompanionData(int type) {
        for (const auto& data : companionDatabase) {
            if (data.type == type) return data;
        }
        return companionDatabase[0]; // Fallback
    }

    std::string GetCompanionDescription(int type, int starLevel) {
        CompanionData data = GetCompanionData(type);
        std::string starString = GetStarString(starLevel);
        int damage = data.baseDamage * starLevel;
        int targets = data.targets + (starLevel - 1);
        float cooldown = data.baseCooldown / starLevel;

        return data.name + " " + starString + "\n" +
            "Damage: " + std::to_string(damage) + "\n" +
            "Targets: " + std::to_string(targets) + "\n" +
            "Cooldown: " + std::to_string(cooldown).substr(0, 3) + "s\n" +
            "Ability: " + data.ability;
    }

    void InitializeShopItems() {
        allShopItems.clear();
        allShopItems.push_back({ 1, "Moon Shard", "-2% Attack Cooldown\nStacks", 400, 0, true, 1 });
        allShopItems.push_back({ 2, "Boots of Travel", "+3% Movement Speed\nStacks", 400, 0, true, 1 });
        allShopItems.push_back({ 3, "Doom Heart", "+1 Enemy per Spawn\nStacks", 400, 0, true, 1 });
        allShopItems.push_back({ 4, "Power Crystal", "+2% Total Damage\nStacks", 400, 0, true, 1 });
        allShopItems.push_back({ 5, "Pocket Heroes", "Random Companion\nAny star level", 0, 40, true, 2 });
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

        for (int i = 0; i < MAX_INVENTORY_SLOTS; i++) {
            InventoryItem item;
            item.type = 0;
            item.starLevel = 1;
            int row = i / 3;
            int col = i % 3;
            item.slot = { (float)startX + col * slotWidth, (float)startY + row * slotHeight, (float)slotWidth, (float)slotHeight };
            item.description = "Empty Slot";
            inventory.push_back(item);
        }

        UpdateInventoryDisplay();
    }

    void UpdateInventoryDisplay() {
        // Сбрасываем все слоты
        for (int i = 0; i < inventory.size(); i++) {
            inventory[i].type = 0;
            inventory[i].starLevel = 1;
            inventory[i].description = "Empty Slot";
        }

        // Заполняем слоты компаньонами
        for (int i = 0; i < companions.size() && i < MAX_INVENTORY_SLOTS; i++) {
            inventory[i].type = companions[i].type;
            inventory[i].starLevel = companions[i].starLevel;
            inventory[i].description = GetCompanionDescription(companions[i].type, companions[i].starLevel);
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
        companions.clear();
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
        if (companions.empty()) return 0;

        // Возвращаем тип первого компаньона для основной атаки
        return companions[0].type;
    }

    void UpdateWeaponChoice(Button& meleeButton, Button& rangeButton, Button& magicButton) {
        Vector2 mousePos = GetMousePosition();

        meleeButton.hovered = CheckCollisionPointRec(mousePos, meleeButton.bounds);
        rangeButton.hovered = CheckCollisionPointRec(mousePos, rangeButton.bounds);
        magicButton.hovered = CheckCollisionPointRec(mousePos, magicButton.bounds);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (meleeButton.hovered) {
                companions.push_back(Companion(1, 1)); // Warrior 1★
                UpdateInventoryDisplay();
                choosingWeapon = false;
            }
            if (rangeButton.hovered) {
                companions.push_back(Companion(2, 1)); // Archer 1★
                UpdateInventoryDisplay();
                choosingWeapon = false;
            }
            if (magicButton.hovered) {
                companions.push_back(Companion(4, 1)); // Ice Mage 1★
                UpdateInventoryDisplay();
                choosingWeapon = false;
            }
        }
    }

    void UpdateShop(Button& randomButton, Button& closeButton, Button& refreshButton) {
        Vector2 mousePos = GetMousePosition();

        randomButton.hovered = CheckCollisionPointRec(mousePos, randomButton.bounds);
        closeButton.hovered = CheckCollisionPointRec(mousePos, closeButton.bounds);
        refreshButton.hovered = CheckCollisionPointRec(mousePos, refreshButton.bounds);

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

                    // Случайный компаньон 1-2 звезды
                    int randomType = GetRandomValue(1, 6);
                    int randomStars = GetRandomValue(1, 2);
                    companions.push_back(Companion(randomType, randomStars));

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
            int randomType = GetRandomValue(1, 6);
            int randomStars = GetRandomValue(1, 3);
            companions.push_back(Companion(randomType, randomStars));
            UpdateInventoryDisplay();
        }
        break;
        case 6:
            freeRefreshUses += GetRandomValue(0, 6);
            break;
        }
    }

    void MergeCompanions() {
        if (companions.size() >= 3) {
            // Проверяем, есть ли 3 компаньона одинакового уровня звезд
            std::vector<int> sameLevelIndices;
            int targetLevel = companions[0].starLevel;

            for (int i = 0; i < companions.size(); i++) {
                if (companions[i].starLevel == targetLevel) {
                    sameLevelIndices.push_back(i);
                    if (sameLevelIndices.size() == 3) break;
                }
            }

            if (sameLevelIndices.size() == 3) {
                // Удаляем трех компаньонов
                for (int i = 2; i >= 0; i--) {
                    companions.erase(companions.begin() + sameLevelIndices[i]);
                }

                // Добавляем нового компаньона более высокого уровня
                int newStarLevel = targetLevel + 1;
                if (newStarLevel > 6) newStarLevel = 6;

                int randomType = GetRandomValue(1, 6);
                companions.push_back(Companion(randomType, newStarLevel));

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

        // Обновляем таймеры компаньонов
        for (auto& companion : companions) {
            companion.attackTimer += deltaTime;
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
    }

    void HandleAllCompanionAttacks() {
        for (auto& companion : companions) {
            CompanionData data = GetCompanionData(companion.type);
            float cooldown = data.baseCooldown / companion.starLevel;

            if (companion.attackTimer >= cooldown) {
                PerformCompanionAttack(companion);
                companion.attackTimer = 0;
            }
        }
    }

    void PerformCompanionAttack(const Companion& companion) {
        CompanionData data = GetCompanionData(companion.type);
        int damage = data.baseDamage * companion.starLevel * (1.0f + damageBonus);
        int targets = data.targets + (companion.starLevel - 1);

        switch (companion.type) {
        case 1: // Warrior - ближняя атака по нескольким целям
            PerformWarriorAttack(damage, targets);
            break;
        case 2: // Archer - замораживающие стрелы
            PerformArcherAttack(damage, targets);
            break;
        case 3: // Mars - волновые атаки
            PerformMarsAttack(damage);
            break;
        case 4: // Ice Mage - ледяные сферы
            PerformIceMageAttack(damage, targets);
            break;
        case 5: // Fire Mage - огненные шары
            PerformFireMageAttack(damage, targets);
            break;
        case 6: // Lightning Mage - цепная молния
            PerformLightningMageAttack(damage, targets);
            break;
        }
    }

    void PerformWarriorAttack(int damage, int targets) {
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

        int targetsToAttack = std::min(targets, (int)nearbyEnemies.size());
        for (int i = 0; i < targetsToAttack; i++) {
            Enemy* target = nearbyEnemies[i];
            target->health -= damage;
            if (target->health <= 0) {
                player.kills++;
                player.gold += GetRandomValue(6, 11);
            }
        }
    }

    void PerformArcherAttack(int damage, int targets) {
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

        int targetsToAttack = std::min(targets, (int)nearbyEnemies.size());
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

            projectiles.emplace_back(player.position,
                Vector2{ direction.x * 250.0f, direction.y * 250.0f },
                true, false, false, damage, false, false, 20.0f, 2);
        }
    }

    void PerformMarsAttack(int damage) {
        // Волновая атака Mars
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

        CreateMarsWaveAttack(attackDirection, damage);
    }

    void PerformIceMageAttack(int damage, int targets) {
        // Ледяные сферы
        std::vector<Enemy*> nearbyEnemies;
        for (auto& enemy : enemies) {
            if (!enemy.active) continue;
            float distance = Vector2Distance(player.position, enemy.position);
            if (distance < 200.0f) {
                nearbyEnemies.push_back(&enemy);
            }
        }

        std::sort(nearbyEnemies.begin(), nearbyEnemies.end(),
            [this](Enemy* a, Enemy* b) {
                return Vector2Distance(player.position, a->position) <
                    Vector2Distance(player.position, b->position);
            });

        int targetsToAttack = std::min(targets, (int)nearbyEnemies.size());
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

            projectiles.emplace_back(player.position,
                Vector2{ direction.x * 200.0f, direction.y * 200.0f },
                true, false, false, damage, false, false, 25.0f, 4);
        }
    }

    void PerformFireMageAttack(int damage, int targets) {
        // Огненные шары
        std::vector<Enemy*> nearbyEnemies;
        for (auto& enemy : enemies) {
            if (!enemy.active) continue;
            float distance = Vector2Distance(player.position, enemy.position);
            if (distance < 300.0f) {
                nearbyEnemies.push_back(&enemy);
            }
        }

        std::sort(nearbyEnemies.begin(), nearbyEnemies.end(),
            [this](Enemy* a, Enemy* b) {
                return Vector2Distance(player.position, a->position) <
                    Vector2Distance(player.position, b->position);
            });

        int targetsToAttack = std::min(targets, (int)nearbyEnemies.size());
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

            projectiles.emplace_back(player.position,
                Vector2{ direction.x * 180.0f, direction.y * 180.0f },
                false, true, false, damage, false, false, 30.0f, 5);
        }
    }

    void PerformLightningMageAttack(int damage, int targets) {
        // Цепная молния
        if (!enemies.empty()) {
            Enemy* firstTarget = &enemies[GetRandomValue(0, enemies.size() - 1)];
            std::vector<Enemy*> chainedTargets = { firstTarget };

            // Находим дополнительные цели для цепной молнии
            for (int i = 1; i < targets && i < enemies.size(); i++) {
                Enemy* lastTarget = chainedTargets.back();
                Enemy* closest = nullptr;
                float minDist = 150.0f; // Максимальное расстояние для цепи

                for (auto& enemy : enemies) {
                    if (!enemy.active) continue;
                    if (std::find(chainedTargets.begin(), chainedTargets.end(), &enemy) != chainedTargets.end()) continue;

                    float distance = Vector2Distance(lastTarget->position, enemy.position);
                    if (distance < minDist) {
                        minDist = distance;
                        closest = &enemy;
                    }
                }

                if (closest) {
                    chainedTargets.push_back(closest);
                }
                else {
                    break;
                }
            }

            // Наносим урон всем целям
            for (auto* target : chainedTargets) {
                target->health -= damage;
                if (target->health <= 0) {
                    player.kills++;
                    player.gold += GetRandomValue(6, 11);
                }
                target->stunTimer = 1.0f; // Оглушение
            }
        }
    }

    void CreateMarsWaveAttack(Vector2 direction, int damage) {
        int numProjectiles = 7 + companions.size(); // Больше волн с большим количеством компаньонов
        float spreadAngle = 180.0f * 3.14159f / 180.0f;

        for (int i = 0; i < numProjectiles; i++) {
            float angle = -spreadAngle / 2 + (spreadAngle / (numProjectiles - 1)) * i;

            float cosA = cos(angle);
            float sinA = sin(angle);

            Vector2 projectileDirection = {
                direction.x * cosA - direction.y * sinA,
                direction.x * sinA + direction.y * cosA
            };

            projectiles.emplace_back(
                player.position,
                Vector2{ projectileDirection.x * 200.0f, projectileDirection.y * 200.0f },
                false, false, false, damage, false, true, 40.0f, 3
            );
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
        for (auto& enemy : enemies) {
            if (!enemy.active) continue;

            // Обработка статусных эффектов
            if (enemy.frozenTimer > 0) {
                enemy.frozenTimer -= deltaTime;
                continue; // Замороженные враги не двигаются
            }

            if (enemy.burnTimer > 0) {
                enemy.burnTimer -= deltaTime;
                enemy.health -= 5; // Урон от горения
                if (enemy.health <= 0) {
                    player.kills++;
                    player.gold += GetRandomValue(6, 11);
                }
            }

            if (enemy.stunTimer > 0) {
                enemy.stunTimer -= deltaTime;
                continue; // Оглушенные враги не двигаются
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
                    enemy.health -= projectile.damage;

                    if (enemy.health <= 0) {
                        player.kills++;
                        player.gold += GetRandomValue(6, 11);
                    }

                    // Применяем статусные эффекты
                    if (projectile.isFreezing) {
                        enemy.frozenTimer = 3.0f;
                    }
                    if (projectile.isBurning) {
                        enemy.burnTimer = 5.0f;
                    }
                    if (projectile.isElectrifying) {
                        enemy.stunTimer = 2.0f;
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
        if (companions.empty()) return;

        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && player.attackCooldown <= 0) {
            Vector2 mouseScreenPos = GetMousePosition();
            Vector2 mouseWorldPos = {
                mouseScreenPos.x + gamestate.cameraOffset.x,
                mouseScreenPos.y + gamestate.cameraOffset.y
            };

            // Используем способность основного компаньона
            Companion& mainCompanion = companions[0];
            PerformCompanionAttack(mainCompanion);
            player.attackCooldown = 0.3f;
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

    void DrawWeaponChoice(Button& meleeButton, Button& rangeButton, Button& magicButton) {
        ClearBackground(BLACK);

        if (backgroundTexture.id != 0) {
            DrawTexture(backgroundTexture, 0, 0, WHITE);
        }

        DrawText("CHOOSE YOUR COMPANION", SCREEN_WIDTH / 2 - MeasureText("CHOOSE YOUR COMPANION", 40) / 2, 200, 40, WHITE);

        // Кнопка Warrior
        DrawRectangleRec(meleeButton.bounds, meleeButton.hovered ? GRAY : DARKGRAY);
        if (meleeTexture.id != 0) {
            DrawTexture(meleeTexture, meleeButton.bounds.x + 10, meleeButton.bounds.y + 10, WHITE);
        }
        else {
            DrawRectangle(meleeButton.bounds.x + 10, meleeButton.bounds.y + 10, 30, 30, RED);
        }
        DrawText(meleeButton.text.c_str(),
            meleeButton.bounds.x + meleeButton.bounds.width / 2 - MeasureText(meleeButton.text.c_str(), 30) / 2,
            meleeButton.bounds.y + meleeButton.bounds.height / 2 - 15, 30, WHITE);

        // Кнопка Archer
        DrawRectangleRec(rangeButton.bounds, rangeButton.hovered ? GRAY : DARKGRAY);
        if (rangeTexture.id != 0) {
            DrawTexture(rangeTexture, rangeButton.bounds.x + 10, rangeButton.bounds.y + 10, WHITE);
        }
        else {
            DrawRectangle(rangeButton.bounds.x + 10, rangeButton.bounds.y + 10, 30, 30, GREEN);
        }
        DrawText(rangeButton.text.c_str(),
            rangeButton.bounds.x + rangeButton.bounds.width / 2 - MeasureText(rangeButton.text.c_str(), 30) / 2,
            rangeButton.bounds.y + rangeButton.bounds.height / 2 - 15, 30, WHITE);

        // Кнопка Ice Mage
        DrawRectangleRec(magicButton.bounds, magicButton.hovered ? GRAY : DARKGRAY);
        if (iceTexture.id != 0) {
            DrawTexture(iceTexture, magicButton.bounds.x + 10, magicButton.bounds.y + 10, WHITE);
        }
        else {
            DrawRectangle(magicButton.bounds.x + 10, magicButton.bounds.y + 10, 30, 30, SKYBLUE);
        }
        DrawText(magicButton.text.c_str(),
            magicButton.bounds.x + magicButton.bounds.width / 2 - MeasureText(magicButton.text.c_str(), 30) / 2,
            magicButton.bounds.y + magicButton.bounds.height / 2 - 15, 30, WHITE);
    }

    void DrawShop(Button& randomButton, Button& closeButton, Button& refreshButton) {
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

        DrawRectangleRec(closeButton.bounds, closeButton.hovered ? GRAY : DARKGRAY);
        DrawText(closeButton.text.c_str(),
            closeButton.bounds.x + closeButton.bounds.width / 2 - MeasureText(closeButton.text.c_str(), 30) / 2,
            closeButton.bounds.y + closeButton.bounds.height / 2 - 15, 30, WHITE);

        DrawText("Press F to merge 3 same-star companions", 80, 390, 20, WHITE);
        // ИСПРАВЛЕННАЯ СТРОКА:
        DrawText(("Companions: " + std::to_string(companions.size()) + "/" + std::to_string(MAX_INVENTORY_SLOTS)).c_str(), 80, 420, 20, WHITE);

        DrawText(("Attack CD Reduction: " + std::to_string((int)(attackCooldownReduction * 100)) + "%").c_str(), 800, 320, 20, BLUE);
        DrawText(("Movement Speed: +" + std::to_string((int)(movementSpeedBonus * 100)) + "%").c_str(), 800, 350, 20, BLUE);
        DrawText(("Extra Enemies: " + std::to_string(extraEnemiesPerSpawn)).c_str(), 800, 380, 20, BLUE);
        DrawText(("Damage Bonus: +" + std::to_string((int)(damageBonus * 100)) + "%").c_str(), 800, 410, 20, BLUE);
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

        ClearBackground(BLACK);

        if (backgroundTexture.id != 0) {
            float parallaxFactor = 0.5f;
            DrawTexture(backgroundTexture,
                -gamestate.cameraOffset.x * parallaxFactor,
                -gamestate.cameraOffset.y * parallaxFactor, WHITE);
        }

        {
            // Враги с эффектами
            for (const auto& enemy : enemies) {
                if (!enemy.active) continue;

                Vector2 screenPos = gamestate.WorldToScreen(enemy.position);

                Color enemyColor = BLUE;
                if (enemy.frozenTimer > 0) enemyColor = SKYBLUE;
                else if (enemy.burnTimer > 0) enemyColor = Color{ 255, 69, 0, 255 };
                else if (enemy.stunTimer > 0) enemyColor = YELLOW;

                DrawRectangle((int)screenPos.x - 20, (int)screenPos.y - 20, 40, 40, enemyColor);

                float healthPercent = (float)enemy.health / enemy.maxHealth;
                DrawRectangle((int)screenPos.x - 20, (int)screenPos.y - 30, 40, 5, RED);
                DrawRectangle((int)screenPos.x - 20, (int)screenPos.y - 30, (int)(40 * healthPercent), 5, GREEN);
            }

            // Снаряды с разными цветами
            for (const auto& projectile : projectiles) {
                if (!projectile.active) continue;

                Vector2 screenPos = gamestate.WorldToScreen(projectile.position);

                Color projColor = WHITE;
                if (projectile.isFreezing) projColor = SKYBLUE;
                else if (projectile.isBurning) projColor = Color{ 255, 69, 0, 255 };
                else if (projectile.isElectrifying) projColor = YELLOW;
                else if (projectile.isMarsWave) projColor = ORANGE;

                if (projectile.isMarsWave) {
                    DrawCircle((int)screenPos.x, (int)screenPos.y, projectile.size / 2, projColor);
                }
                else {
                    DrawRectangle((int)screenPos.x - 10, (int)screenPos.y - 10, 20, 20, projColor);
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

            DrawRectangle(enemyX - 2, enemyY - 2, 4, 4, BLUE);
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
            // Рисуем иконки компаньонов
            if (item.type == 1 && meleeTexture.id != 0) {
                DrawTexture(meleeTexture, item.slot.x + 10, item.slot.y + 10, WHITE);
            }
            else if (item.type == 2 && rangeTexture.id != 0) {
                DrawTexture(rangeTexture, item.slot.x + 10, item.slot.y + 10, WHITE);
            }
            else if (item.type == 3 && marsTexture.id != 0) {
                DrawTexture(marsTexture, item.slot.x + 10, item.slot.y + 10, WHITE);
            }
            else if (item.type == 4 && iceTexture.id != 0) {
                DrawTexture(iceTexture, item.slot.x + 10, item.slot.y + 10, WHITE);
            }
            else if (item.type == 5 && fireTexture.id != 0) {
                DrawTexture(fireTexture, item.slot.x + 10, item.slot.y + 10, WHITE);
            }
            else if (item.type == 6 && lightningTexture.id != 0) {
                DrawTexture(lightningTexture, item.slot.x + 10, item.slot.y + 10, WHITE);
            }
            else if (item.type == 1) {
                DrawRectangle(item.slot.x + 10, item.slot.y + 10, 30, 30, RED);
            }
            else if (item.type == 2) {
                DrawRectangle(item.slot.x + 10, item.slot.y + 10, 30, 30, GREEN);
            }
            else if (item.type == 3) {
                DrawRectangle(item.slot.x + 10, item.slot.y + 10, 30, 30, ORANGE);
            }
            else if (item.type == 4) {
                DrawRectangle(item.slot.x + 10, item.slot.y + 10, 30, 30, SKYBLUE);
            }
            else if (item.type == 5) {
                DrawRectangle(item.slot.x + 10, item.slot.y + 10, 30, 30, Color{ 255, 69, 0, 255 });
            }
            else if (item.type == 6) {
                DrawRectangle(item.slot.x + 10, item.slot.y + 10, 30, 30, YELLOW);
            }

            // Рисуем уровень звезд
            if (item.type != 0) {
                std::string starText = GetStarString(item.starLevel);
                DrawText(starText.c_str(), item.slot.x + 50, item.slot.y + 15, 20, GOLD);
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

        // Убрано отображение количества компаньонов слева сверху

        if (enemies.size() > MAX_ENEMIES) {
            std::string timerText = "Time: " + std::to_string((int)gameOverTimer);
            DrawText(timerText.c_str(), 20, startY + 120, 20, RED);
        }

        DrawText("RMB: Companion ability", 20, startY + 150, 20, WHITE);
        DrawText("F: Merge 3 same-star companions", 20, startY + 180, 20, WHITE);

        DrawText(("CD Reduction: " + std::to_string((int)(attackCooldownReduction * 100)) + "%").c_str(), 20, startY + 210, 18, BLUE);
        DrawText(("Speed: +" + std::to_string((int)(movementSpeedBonus * 100)) + "%").c_str(), 20, startY + 235, 18, BLUE);
        DrawText(("Damage: +" + std::to_string((int)(damageBonus * 100)) + "%").c_str(), 20, startY + 260, 18, BLUE);

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

        Button meleeButton = { {SCREEN_WIDTH / 2 - 150, 300, 300, 80}, "WARRIOR", false };
        Button rangeButton = { {SCREEN_WIDTH / 2 - 150, 400, 300, 80}, "ARCHER", false };
        Button magicButton = { {SCREEN_WIDTH / 2 - 150, 500, 300, 80}, "ICE MAGE", false };

        Button randomButton = { {100, 500, 200, 120}, "RANDOM COMPANION", false };
        Button refreshButton = { {350, 500, 200, 120}, "REFRESH SHOP", false };
        Button shopCloseButton = { {850, 500, 200, 50}, "CLOSE", false };

        while (!WindowShouldClose()) {
            if (inGame) {
                if (choosingWeapon) {
                    UpdateWeaponChoice(meleeButton, rangeButton, magicButton);
                    BeginDrawing();
                    DrawWeaponChoice(meleeButton, rangeButton, magicButton);
                    EndDrawing();
                }
                else if (inShop) {
                    UpdateShop(randomButton, shopCloseButton, refreshButton);
                    BeginDrawing();
                    DrawShop(randomButton, shopCloseButton, refreshButton);
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