#include <windows.h>
#include <iostream>
#include <vector>
#include <thread>
#include <conio.h>
#include <random>
#include <mutex>
#include "PoolAllocator.h"


const int GAME_WIDTH = 30;
const int GAME_HEIGHT = 27;

std::random_device rd;
std::mt19937 mt{ rd() };

enum class GameState {
    Title,
    Playing,
    GameOver,
    GameClear
};

class GameObject {
public:
    int prev_x = 0;
    int prev_y = 0;

    virtual void Update() {}

    virtual void Draw() {}

    virtual void ClearPrev() {
        COORD coord;
        coord.X = static_cast<SHORT>(prev_x);
        coord.Y = static_cast<SHORT>(prev_y);
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
        std::cout << " ";
    }
};

class Brick : public GameObject {
public:
    int x, y;

    Brick() : x(0), y(0) {}

    Brick(int init_x, int init_y) : x(init_x), y(init_y) {}

    void Draw() override {
        COORD coord;
        coord.X = static_cast<SHORT>(x);
        coord.Y = static_cast<SHORT>(y);
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
        std::cout << "*";
    }
};

class Paddle : public GameObject {
public:
    int x, y;
    int score = 0;
    std::mutex scoreMutex;

    Paddle() : x(5), y(20), score(0) {}

    void Update() override {
        if (_kbhit()) {
            char c = static_cast<char>(_getch());
            if (c == 'a') {
                if (x > 2) {
                    x -= 2;
                }
                else {
                    x = 1;
                }
            }
            if (c == 'd') {
                if (x < GAME_WIDTH - 5) {
                    x += 2;
                }
                else {
                    x = GAME_WIDTH - 4;
                }
            }
        }
    }

    void Draw() override {
        // 前の座標を空白で上書き
        COORD prev_coord;
        prev_coord.X = static_cast<SHORT>(prev_x);
        prev_coord.Y = static_cast<SHORT>(prev_y) - 3;
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), prev_coord);
        std::cout << "   ";

        // 現在の座標を描画
        COORD coord;
        coord.X = static_cast<SHORT>(x);
        coord.Y = static_cast<SHORT>(y) - 3;
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
        std::cout << "===";

        // 現在の座標を記録
        prev_x = x;
        prev_y = y;
    }
};

class Ball : public GameObject {
public:
    int x, y;
    int dx = 1;
    int dy = 1;
    int frameCount = 0;


    Ball() {
        std::uniform_int_distribution<> xDist(2, 28);
        std::uniform_int_distribution<> yDist(5, 6);

        x = xDist(mt);
        y = yDist(mt);
    }

    void Update() override {
        frameCount++;
        if (frameCount % 1 == 0) {
            prev_x = x;
            prev_y = y;
            x += dx;
            y += dy;
        }
    }

    void Draw() override {
        COORD coord;
        coord.X = static_cast<SHORT>(x);
        coord.Y = static_cast<SHORT>(y);
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
        std::cout << "O";
    }

    // 当たり判定の処理
    void CheckCollision(Paddle& paddle, std::vector<GameObject*>& bricks, int gameWidth, int gameHeight, GameState& gameState, bool& gameOver) {
        // パドルとの衝突
        if (x >= paddle.x && x <= paddle.x + 2 && (y == paddle.y - 4 || y == paddle.y - 3)) {
            std::uniform_int_distribution<> signDist(0, 1);
            dx *= (signDist(mt) == 0 ? -1 : 1);
            dy *= -1;
        }

        // 左の外枠との衝突
        if (x <= 0) {
            dx *= -1;
            x += 1;
        }

        // 右の外枠との衝突
        if (x >= gameWidth - 1) {
            dx *= -1;
            x -= 1;
        }

        // 上の外枠との衝突
        if (y <= 0) {
            dy *= -1;
            y += 1;
        }

        // ブロックとの衝突
        bool check = false;
        auto it = bricks.begin();
        while (it != bricks.end()) {
            if (Brick* brick = dynamic_cast<Brick*>(*it)) {
                if (x == brick->x && y <= brick->y) {
                    paddle.scoreMutex.lock();
                    std::uniform_int_distribution<> signDist(0, 1);
                    dx *= (signDist(mt) == 0 ? -1 : 1);
                    dy *= -1;
                    it = bricks.erase(it);
                    check = true;
                    paddle.scoreMutex.unlock();
                }
                else {
                    ++it;
                }
            }
            else {
                ++it;
            }
        }
        if (check) {
            paddle.score++;
        }

        // 下の外枠との衝突（ゲームオーバー）
        if (x >= gameWidth || y >= gameHeight) {
            gameState = GameState::GameOver;
            gameOver = true;
        }
    }
};

class Game {
public:
    Game() {
        gameState = GameState::Title;
        hasShownTitle = false;
        gameOver = false;
        checkCount = 0;
        nextAddCount = 350;
    }

    void Run() {
        // ブロック初期化
        int brick_rows = 3;
        int brick_cols = (GAME_WIDTH - 2);

        for (int row = 0; row < brick_rows; ++row) {
            for (int col = 1; col <= brick_cols; ++col) {
                Brick* newBrick = brickAllocator.Alloc();
                if (newBrick == nullptr) {
                    std::cerr << "Memory allocation failed for Brick.\n";
                    exit(EXIT_FAILURE);
                }
                if (newBrick) {
                    newBrick->x = col;
                    newBrick->y = row + 1;
                    gameObjects.push_back(newBrick);
                }
            }
        }

        // パドル初期化
        paddle = paddleAllocator.Alloc();
        if (paddle) {
            paddle->y = GAME_HEIGHT - 2;
            gameObjects.push_back(static_cast<GameObject*>(paddle));
        }

        // ボール初期化
        Ball* ball = ballAllocator.Alloc();
        if (ball) {
            gameObjects.push_back(static_cast<GameObject*>(ball));
        }

        // ゲームのメイン処理
        while (true) {
            if (gameState == GameState::Title) {
                ShowTitle();
            }
            else if (gameState == GameState::Playing) {
                Play();
            }
            else if (gameState == GameState::GameOver) {
                ShowGameOver();
                break;
            }
            else if (gameState == GameState::GameClear) {
                ShowGameClear();
            }
        }
    }

private:
    PoolAllocator<Ball, 10> ballAllocator;
    PoolAllocator<Brick, 120> brickAllocator;
    PoolAllocator<Paddle, 1> paddleAllocator;

    GameState gameState;

    std::vector<GameObject*> gameObjects;

    Paddle* paddle;

    bool hasShownTitle;
    bool gameOver;

    int checkCount;
    int nextAddCount;

    void ShowTitle() {
        std::cout << std::endl;
        std::cout << "　■■■■■　　　　　　　　　　　　　　　　　　　　　　■　　　　■　■■　　■■　　　　■　　　　■■■■　" << std::endl;
        std::cout << "　■　　　■■　　　　　　　　　　　　　　　　　　　　　■　　　■　　■■　　■■　　　　■　　　■　　　　　" << std::endl;
        std::cout << "　■　　　　■　　　　　　　　　　　　　　　　　　　　　■　　■　　　■■　　■　■　　　■　　■　　　　　　" << std::endl;
        std::cout << "　■　　　■■　　■　■■　　■■■　　　　■■■　　　■　■　　　　■■　　■　■■　　■　　■　　　　　　" << std::endl;
        std::cout << "　■■■■■　　　■■　　　■　　　■　　　　　　■　　■■■　　　　■■　　■　　■　　■　　■　　　■■■" << std::endl;
        std::cout << "　■　　　　■　　■　　　　■■■■■　　　■■■■　　■　　■　　　■■　　■　　■■　■　　■　　　　　■" << std::endl;
        std::cout << "　■　　　　■　　■　　　　■　　　　　　■　　　■　　■　　■■　　■■　　■　　　■　■　　■　　　　　■" << std::endl;
        std::cout << "　■　　　■■　　■　　　　■　　　　　　■　　■■　　■　　　■　　■■　　■　　　　■■　　　■　　　　■" << std::endl;
        std::cout << "　■■■■■　　　■　　　　　■■■■　　■■■■■　　■　　　　■　■■　　■　　　　■■　　　　■■■■■" << std::endl;
        std::cout << std::endl;
        std::cout << "　パドルを操作して、ブロックを消していこう！！" << std::endl;
        std::cout << "　時間が経過するごとにボールが増えていくよ！！" << std::endl;
        std::cout << "　ブロックをたくさん消して、ハイスコアを目指せ！！" << std::endl;
        std::cout << "　スペースキーを押してゲームスタート！！" << std::endl;
        std::cout << std::endl;
        std::cout << "　操作方法" << std::endl;
        std::cout << "　Aキー：パドルを左に移動" << std::endl;
        std::cout << "　Dキー：パドルを右に移動" << std::endl;
        std::cout << std::endl;
        std::cout << "　オブジェクト解説" << std::endl;
        std::cout << "　 * ：ブロック" << std::endl;
        std::cout << "　 O ：ボール" << std::endl;
        std::cout << "　===：パドル" << std::endl;
        std::cout << "　 H ：壁" << std::endl;

        GameState nextState = GameState::Title;

        while (true) {
            if (_kbhit()) {
                char c = static_cast<char>(_getch());
                if (c == ' ') {
                    gameState = GameState::Playing;
                    system("CLS");
                    break;
                }
            }
        }
    }

    void Play() {
        checkCount++;

        // 2個目以降のボールの生成
        if (checkCount == nextAddCount) {
            Ball* newBall = ballAllocator.Alloc();
            if (newBall) {
                std::uniform_int_distribution<> xDist(2, GAME_WIDTH - 2);
                std::uniform_int_distribution<> yDist(5, 6);
                newBall->x = xDist(mt);
                newBall->y = yDist(mt);

                gameObjects.push_back(static_cast<GameObject*>(newBall));
            }
            checkCount = 0;
            std::uniform_int_distribution<> ballDist(400, 600);
            nextAddCount = ballDist(mt);
        }

        std::thread updateThread([&]() {
            for (auto obj : gameObjects) {
                obj->Update();
            }
            });
        updateThread.join();

        for (auto obj : gameObjects) {
            if (Ball* b = dynamic_cast<Ball*>(obj)) {
                b->CheckCollision(*paddle, gameObjects, GAME_WIDTH, GAME_HEIGHT, gameState, gameOver);
            }
        }

        // 全オブジェクトの前の位置をクリア
        for (auto obj : gameObjects) {
            obj->ClearPrev();
        }

        // 外枠を描画
        for (int y = 0; y < GAME_HEIGHT; ++y) {
            for (int x = 0; x < GAME_WIDTH; ++x) {
                if (x == 0 || x == GAME_WIDTH - 1 || y == 0 || y == GAME_HEIGHT - 1) {
                    COORD coord;
                    coord.X = static_cast<SHORT>(x);
                    coord.Y = static_cast<SHORT>(y);
                    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
                    std::cout << "H";
                }
            }
        }

        // オブジェクトを描画
        for (auto obj : gameObjects) {
            obj->Draw();
        }

        // スコアを画面下部に表示
        COORD scoreCoord;
        scoreCoord.X = 0;
        scoreCoord.Y = GAME_HEIGHT + 2;
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), scoreCoord);
        std::cout << "スコア：" << paddle->score;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // ゲームオーバーチェック
        if (gameOver) {
            gameState = GameState::GameOver;
        }

        // ゲームクリアチェック
        bool allBricksGone = true;
        for (auto obj : gameObjects) {
            if (dynamic_cast<Brick*>(obj)) {
                allBricksGone = false;
                break;
            }
        }
        if (allBricksGone) {
            gameState = GameState::GameClear;
        }
    }

    void ShowGameOver() {
        system("CLS");
        std::cout << std::endl;
        std::cout << "　　　■■■■　　　　　　　　　　　　　　　　　　　　　　　　　■■■■　　　　　　　　　　　　　　　　　　　　" << std::endl;
        std::cout << "　■■　　　　　　　　　　　　　　　　　　　　　　　　　　　　■■　　　■　　　　　　　　　　　　　　　　　　　" << std::endl;
        std::cout << "　■　　　　　　　■■■■　　■■■■　■■■　　　■■■　　■　　　　　■　■　　　■　　　■■■　　■■■　" << std::endl;
        std::cout << "　■　　　　　　　　　　■　　■　　■■　　■　　■　　■■　■　　　　　■　■■　　■　　■　　■■　■■　　" << std::endl;
        std::cout << "　■　　■■■　　■■■■　　■　　　■　　■　　■■■■■　■　　　　　■　　■　　■　　■■■■■　■　　　" << std::endl;
        std::cout << "　■　　　　■　　■　　■　　■　　　■　　■　　■　　　　　■　　　　　■　　■　■　　　■　　　　　■　　　" << std::endl;
        std::cout << "　■■　　　■　　■　　■　　■　　　■　　■　　■　　　　　■■　　　■　　　　■■　　　■　　　　　■　　　" << std::endl;
        std::cout << "　　■■■■■　　■■■■　　■　　　■　　■　　　■■■　　　■■■■　　　　　■■　　　　■■■　　■　　　" << std::endl;
        std::cout << std::endl;
        std::cout << "　スコア：" << paddle->score << std::endl;
        std::cout << std::endl;
        std::cout << "　Qキーを押してゲーム終了" << std::endl;
        std::cout << "　Rキーを押してリトライ" << std::endl;
    }

    void ShowGameClear() {
        system("CLS");
        std::cout << std::endl;
        std::cout << "　　　■■■■　　　　　　　　　　　　　　　　　　　　　　　　　■■■■　　■　　　　　　　　　　　　　　　　　　" << std::endl;
        std::cout << "　■■　　　　　　　　　　　　　　　　　　　　　　　　　　　　■■　　　　　■　　　　　　　　　　　　　　　　　　" << std::endl;
        std::cout << "　■　　　　　　　■■■■　　■■■■　■■■　　　■■■　　■　　　　　　■　　　■■■　　■■■■　　■■■　" << std::endl;
        std::cout << "　■　　　　　　　　　　■　　■　　■■　　■　　■　　■■　■　　　　　　■　　■　　■■　　　　■　　■■　　" << std::endl;
        std::cout << "　■　　■■■　　■■■■　　■　　　■　　■　　■■■■■　■　　　　　　■　　■■■■■　■■■■　　■　　　" << std::endl;
        std::cout << "　■　　　　■　　■　　■　　■　　　■　　■　　■　　　　　■　　　　　　■　　■　　　　　■　　■　　■　　　" << std::endl;
        std::cout << "　■■　　　■　　■　　■　　■　　　■　　■　　■　　　　　■■　　　　　■　　■　　　　　■　　■　　■　　　" << std::endl;
        std::cout << "　　■■■■■　　■■■■　　■　　　■　　■　　　■■■　　　■■■■　　■　　　■■■　　■■■■　　■　　　" << std::endl;
        std::cout << std::endl;
        std::cout << "　スコア：" << paddle->score << std::endl;
        std::cout << std::endl;
        std::cout << "　Qキーを押してゲーム終了" << std::endl;
        std::cout << "　Rキーを押してリトライ" << std::endl;
    }
};


int main() {
    // エスケープシーケンスを有効に
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hStdOut, &mode);
    SetConsoleMode(hStdOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    // カーソルを消す
    printf("\x1b[?25l");

    // ゲームスタート
    while (true) {
        Game game;
        game.Run();

        // キー入力待ち
        while (true) {
            if (_kbhit()) {
                char c = static_cast<char>(_getch());
                if (c == 'q' || c == 'Q') {
                    system("CLS");
                    return EXIT_SUCCESS;
                }
                else if (c == 'r' || c == 'R') {
                    system("CLS");
                    break;
                }
            }
        }
    }

    return EXIT_SUCCESS;
}