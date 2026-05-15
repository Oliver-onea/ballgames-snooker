#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int TABLE_WIDTH = 840;
constexpr int TABLE_HEIGHT = 440;
constexpr int INNER_X = 20;
constexpr int INNER_Y = 20;
constexpr int INNER_WIDTH = 800;
constexpr int INNER_HEIGHT = 400;
constexpr float BALL_RADIUS = 10.0f;
constexpr float POCKET_RADIUS = 15.0f;
constexpr float PLACE_CENTER_X = 220.0f;
constexpr float PLACE_CENTER_Y = 220.0f;
constexpr float PLACE_RADIUS = 100.0f;
constexpr float FRICTION = 0.9919999837875366f;
constexpr float STOP_THRESHOLD = 0.03400000184774399f;
constexpr float PLAYER_SHOT_SPEED = 10.0f;
constexpr float ROBOT_SHOT_SPEED = 9.0f;
constexpr double STEP_SECONDS = 0.008;
constexpr int MIN_VIEW_WIDTH = 960;
constexpr int MIN_VIEW_HEIGHT = 600;
constexpr float PI = 3.14159265358979323846f;

constexpr int COLOR_WHITE = 0xffffff;
constexpr int COLOR_RED = 0x0000ff;
constexpr int COLOR_YELLOW = 0x00ffff;
constexpr int COLOR_GREEN = 0x00ff00;
constexpr int COLOR_BROWN = 0x13458b;
constexpr int COLOR_BLUE = 0xff0000;
constexpr int COLOR_PINK = 0x9314ff;
constexpr int COLOR_BLACK = 0x000000;
constexpr int COLOR_RAIL = 0x1345c8;
constexpr int COLOR_CUE = 0x2d1900;

constexpr std::array<std::pair<float, float>, 6> POCKETS{{
    {20.0f, 20.0f},
    {420.0f, 20.0f},
    {820.0f, 20.0f},
    {20.0f, 420.0f},
    {420.0f, 420.0f},
    {820.0f, 420.0f},
}};

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct Ball {
  Vec2 position;
  Vec2 velocity;
  float mass = 1.0f;
  float radius = BALL_RADIUS;
  int color = COLOR_WHITE;
  bool pocketed = false;
  int score = 0;
};

SDL_Color colorRef(int value) {
  return SDL_Color{
      static_cast<Uint8>(value & 0xff),
      static_cast<Uint8>((value >> 8) & 0xff),
      static_cast<Uint8>((value >> 16) & 0xff),
      255,
  };
}

void setDrawColor(SDL_Renderer* renderer, int color) {
  const SDL_Color c = colorRef(color);
  SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

float length(Vec2 value) {
  return std::sqrt(value.x * value.x + value.y * value.y);
}

double nowSeconds() {
  using clock = std::chrono::steady_clock;
  return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

std::optional<std::filesystem::path> findFont() {
  std::vector<std::filesystem::path> candidates;
  if (const char* prefix = std::getenv("CONDA_PREFIX")) {
    candidates.emplace_back(std::filesystem::path(prefix) / "fonts" / "DejaVuSans.ttf");
    candidates.emplace_back(std::filesystem::path(prefix) / "fonts" / "Ubuntu-R.ttf");
  }

  candidates.emplace_back("/usr/share/fonts/truetype/msttcorefonts/Arial.ttf");
  candidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
  candidates.emplace_back("/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf");

  for (const auto& candidate : candidates) {
    if (std::filesystem::exists(candidate)) {
      return candidate;
    }
  }
  return std::nullopt;
}

class Game {
 public:
  Game() {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
      throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    if (TTF_Init() != 0) {
      throw std::runtime_error(std::string("TTF_Init failed: ") + TTF_GetError());
    }

    SDL_DisplayMode mode{};
    SDL_GetCurrentDisplayMode(0, &mode);
    const int width = std::max(MIN_VIEW_WIDTH, mode.w > 0 ? mode.w : 1024);
    const int height = std::max(MIN_VIEW_HEIGHT, mode.h > 0 ? mode.h : 768);

    window_ = SDL_CreateWindow(
        "Snooker",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window_) {
      throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    if (!renderer_) {
      renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE);
    }
    if (!renderer_) {
      throw std::runtime_error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
    }

    if (auto fontPath = findFont()) {
      font_ = TTF_OpenFont(fontPath->string().c_str(), 24);
    }
    if (!font_) {
      throw std::runtime_error("Could not find or load a usable TTF font");
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    updateLayout();
    initializeBalls();
  }

  ~Game() {
    if (worldTexture_) {
      SDL_DestroyTexture(worldTexture_);
    }
    if (font_) {
      TTF_CloseFont(font_);
    }
    if (renderer_) {
      SDL_DestroyRenderer(renderer_);
    }
    if (window_) {
      SDL_DestroyWindow(window_);
    }
    TTF_Quit();
    SDL_Quit();
  }

  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;

  int run(bool smokeTest) {
    if (smokeTest) {
      step(nowSeconds());
      render();
      return 0;
    }

    bool running = true;
    double previous = nowSeconds();
    double accumulator = 0.0;

    while (running) {
      running = handleEvents();
      const double current = nowSeconds();
      const double delta = std::min(0.064, current - previous);
      previous = current;
      accumulator += delta;

      while (accumulator >= STEP_SECONDS) {
        step(current);
        accumulator -= STEP_SECONDS;
      }

      render();
      SDL_Delay(1);
    }

    return 0;
  }

 private:
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* worldTexture_ = nullptr;
  TTF_Font* font_ = nullptr;

  std::vector<Ball> balls_;
  Vec2 cueStart_;
  Vec2 cueEnd_;
  bool isAiming_ = false;
  bool isPlacingWhiteBall_ = true;
  bool isRobotTurn_ = false;
  bool isPlayerTurn_ = true;
  bool gameStarted_ = false;
  int playerScore_ = 0;
  int robotScore_ = 0;
  std::optional<double> robotShotAt_;
  bool touchAiming_ = false;
  bool fullscreen_ = false;

  int viewWidth_ = MIN_VIEW_WIDTH;
  int viewHeight_ = MIN_VIEW_HEIGHT;
  float viewScale_ = 1.0f;
  Vec2 viewOffset_;
  Vec2 tableOffset_{60.0f, 80.0f};
  std::mt19937 rng_{std::random_device{}()};

  Ball makeBall(float x, float y, int color, int score) {
    return Ball{Vec2{x, y}, Vec2{0.0f, 0.0f}, 1.0f, BALL_RADIUS, color, false, score};
  }

  void initializeBalls() {
    balls_.clear();
    balls_.push_back(makeBall(220.0f, 240.0f, COLOR_WHITE, 0));
    balls_.push_back(makeBall(420.0f, 220.0f, COLOR_BLUE, 5));
    balls_.push_back(makeBall(220.0f, 320.0f, COLOR_YELLOW, 2));
    balls_.push_back(makeBall(220.0f, 120.0f, COLOR_GREEN, 3));
    balls_.push_back(makeBall(220.0f, 220.0f, COLOR_BROWN, 4));
    balls_.push_back(makeBall(600.0f, 220.0f, COLOR_PINK, 6));
    balls_.push_back(makeBall(640.0f, 220.0f, COLOR_BLACK, 7));

    constexpr float baseX = 660.0f;
    constexpr float baseY = 220.0f;
    constexpr float rowGap = 17.32050895690918f;

    for (int row = 0; row < 5; row += 1) {
      for (int col = 0; col <= row; col += 1) {
        balls_.push_back(makeBall(
            baseX + static_cast<float>(row) * rowGap,
            baseY - static_cast<float>(row) * BALL_RADIUS + static_cast<float>(col) * BALL_RADIUS * 2.0f,
            COLOR_RED,
            1));
      }
    }

    isPlacingWhiteBall_ = true;
    gameStarted_ = false;
  }

  void updateLayout() {
    int windowW = 0;
    int windowH = 0;
    SDL_GetWindowSize(window_, &windowW, &windowH);

    viewWidth_ = std::max(windowW, MIN_VIEW_WIDTH);
    viewHeight_ = std::max(windowH, MIN_VIEW_HEIGHT);
    viewScale_ = std::min(
        static_cast<float>(windowW) / static_cast<float>(viewWidth_),
        static_cast<float>(windowH) / static_cast<float>(viewHeight_));
    viewOffset_ = Vec2{
        (static_cast<float>(windowW) - static_cast<float>(viewWidth_) * viewScale_) / 2.0f,
        (static_cast<float>(windowH) - static_cast<float>(viewHeight_) * viewScale_) / 2.0f,
    };
    tableOffset_ = Vec2{
        static_cast<float>(std::trunc(static_cast<float>(viewWidth_ - TABLE_WIDTH) / 2.0f)),
        static_cast<float>(std::trunc(static_cast<float>(viewHeight_ - TABLE_HEIGHT) / 2.0f)),
    };

    if (worldTexture_) {
      SDL_DestroyTexture(worldTexture_);
      worldTexture_ = nullptr;
    }
    worldTexture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        viewWidth_,
        viewHeight_);
    if (!worldTexture_) {
      throw std::runtime_error(std::string("SDL_CreateTexture failed: ") + SDL_GetError());
    }
  }

  Vec2 screenToWorld(int x, int y) const {
    return Vec2{
        (static_cast<float>(x) - viewOffset_.x) / viewScale_,
        (static_cast<float>(y) - viewOffset_.y) / viewScale_,
    };
  }

  Vec2 whiteBallScreenPosition() const {
    const Ball& white = balls_[0];
    return Vec2{tableOffset_.x + white.position.x, tableOffset_.y + white.position.y};
  }

  void placeWhiteBall(Vec2 point) {
    const Vec2 center{tableOffset_.x + PLACE_CENTER_X, tableOffset_.y + PLACE_CENTER_Y};
    const bool insideTable =
        point.x > tableOffset_.x + INNER_X + BALL_RADIUS &&
        point.x <= tableOffset_.x + INNER_X + INNER_WIDTH - BALL_RADIUS - 1.0f &&
        point.y > tableOffset_.y + INNER_Y + BALL_RADIUS &&
        point.y <= tableOffset_.y + INNER_Y + INNER_HEIGHT - BALL_RADIUS - 1.0f;
    const bool insidePlacement =
        std::hypot(point.x - center.x, point.y - center.y) <= PLACE_RADIUS && point.x <= center.x;

    if (insideTable && insidePlacement) {
      balls_[0].position.x = point.x - tableOffset_.x;
      balls_[0].position.y = point.y - tableOffset_.y;
    }
  }

  void drawLine(Vec2 start, Vec2 end) {
    SDL_RenderDrawLine(
        renderer_,
        static_cast<int>(std::lround(start.x)),
        static_cast<int>(std::lround(start.y)),
        static_cast<int>(std::lround(end.x)),
        static_cast<int>(std::lround(end.y)));
  }

  void fillCircle(Vec2 center, float radius) {
    const int cx = static_cast<int>(std::lround(center.x));
    const int cy = static_cast<int>(std::lround(center.y));
    const int r = static_cast<int>(std::lround(radius));
    for (int dy = -r; dy <= r; dy += 1) {
      const int dx = static_cast<int>(std::floor(std::sqrt(static_cast<float>(r * r - dy * dy))));
      SDL_RenderDrawLine(renderer_, cx - dx, cy + dy, cx + dx, cy + dy);
    }
  }

  void drawArc(Vec2 center, float radius, float startAngle, float endAngle) {
    constexpr int segments = 96;
    Vec2 previous{
        center.x + std::cos(startAngle) * radius,
        center.y + std::sin(startAngle) * radius,
    };
    for (int i = 1; i <= segments; i += 1) {
      const float t = static_cast<float>(i) / static_cast<float>(segments);
      const float angle = startAngle + (endAngle - startAngle) * t;
      const Vec2 current{center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
      drawLine(previous, current);
      previous = current;
    }
  }

  void drawDottedLine(Vec2 start, Vec2 end) {
    const Vec2 delta{end.x - start.x, end.y - start.y};
    const float total = length(delta);
    if (total <= 0.0f) {
      return;
    }

    constexpr float dash = 3.0f;
    constexpr float gap = 3.0f;
    const Vec2 unit{delta.x / total, delta.y / total};

    for (float cursor = 0.0f; cursor < total; cursor += dash + gap) {
      const float next = std::min(cursor + dash, total);
      drawLine(
          Vec2{start.x + unit.x * cursor, start.y + unit.y * cursor},
          Vec2{start.x + unit.x * next, start.y + unit.y * next});
    }
  }

  void drawCue(Vec2 start, Vec2 end) {
    const Vec2 delta{end.x - start.x, end.y - start.y};
    const float cueLength = length(delta);
    if (cueLength <= 0.0f) {
      return;
    }

    const Vec2 extended{start.x + delta.x * 2.0f, start.y + delta.y * 2.0f};
    setDrawColor(renderer_, COLOR_CUE);
    for (int offset = -2; offset <= 2; offset += 1) {
      SDL_RenderDrawLine(
          renderer_,
          static_cast<int>(std::lround(start.x)),
          static_cast<int>(std::lround(start.y + static_cast<float>(offset))),
          static_cast<int>(std::lround(end.x)),
          static_cast<int>(std::lround(end.y + static_cast<float>(offset))));
    }

    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    drawDottedLine(start, extended);
    setDrawColor(renderer_, COLOR_GREEN);
    fillCircle(extended, BALL_RADIUS);
  }

  void drawText(const std::string& text, int x, int y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderText_Blended(font_, text.c_str(), color);
    if (!surface) {
      return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    const SDL_Rect dst{x, y, surface->w, surface->h};
    SDL_FreeSurface(surface);
    if (!texture) {
      return;
    }
    SDL_RenderCopy(renderer_, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
  }

  void renderBackground() {
    setDrawColor(renderer_, COLOR_BROWN);
    SDL_RenderClear(renderer_);

    setDrawColor(renderer_, COLOR_BROWN);
    for (int y = 0; y < viewHeight_; y += 100) {
      for (int x = 0; x < viewWidth_; x += 200) {
        const SDL_Rect rect{x, y, 200, 100};
        SDL_RenderFillRect(renderer_, &rect);
        setDrawColor(renderer_, COLOR_BLACK);
        SDL_RenderDrawRect(renderer_, &rect);
        setDrawColor(renderer_, COLOR_BROWN);
      }
    }
  }

  void renderTable() {
    const float tx = tableOffset_.x;
    const float ty = tableOffset_.y;

    setDrawColor(renderer_, 0x008000);
    SDL_Rect inner{
        static_cast<int>(std::lround(tx + INNER_X)),
        static_cast<int>(std::lround(ty + INNER_Y)),
        INNER_WIDTH,
        INNER_HEIGHT};
    SDL_RenderFillRect(renderer_, &inner);

    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    drawArc(Vec2{tx + PLACE_CENTER_X, ty + PLACE_CENTER_Y}, PLACE_RADIUS, PI / 2.0f, PI * 1.5f);
    drawLine(Vec2{tx + PLACE_CENTER_X, ty + INNER_Y}, Vec2{tx + PLACE_CENTER_X, ty + INNER_Y + INNER_HEIGHT});

    setDrawColor(renderer_, COLOR_RAIL);
    const std::array<SDL_Rect, 4> rails{{
        {static_cast<int>(std::lround(tx)), static_cast<int>(std::lround(ty)), TABLE_WIDTH, INNER_Y},
        {static_cast<int>(std::lround(tx)), static_cast<int>(std::lround(ty)), INNER_X, TABLE_HEIGHT},
        {static_cast<int>(std::lround(tx)), static_cast<int>(std::lround(ty + INNER_Y + INNER_HEIGHT)), TABLE_WIDTH, INNER_Y},
        {static_cast<int>(std::lround(tx + INNER_X + INNER_WIDTH)), static_cast<int>(std::lround(ty)), INNER_X, TABLE_HEIGHT},
    }};
    for (const SDL_Rect& rail : rails) {
      SDL_RenderFillRect(renderer_, &rail);
    }

    setDrawColor(renderer_, COLOR_BLACK);
    for (const auto& pocket : POCKETS) {
      fillCircle(Vec2{tx + pocket.first, ty + pocket.second}, POCKET_RADIUS);
    }

    for (const Ball& ball : balls_) {
      if (ball.pocketed) {
        continue;
      }
      setDrawColor(renderer_, ball.color);
      fillCircle(Vec2{tx + ball.position.x, ty + ball.position.y}, ball.radius);
    }

    if (isAiming_) {
      drawCue(cueEnd_, whiteBallScreenPosition());
    }
  }

  void renderHud() {
    drawText("Player Score: " + std::to_string(playerScore_), static_cast<int>(tableOffset_.x), static_cast<int>(tableOffset_.y) - 50, SDL_Color{255, 255, 255, 255});
    drawText("Robot Score: " + std::to_string(robotScore_), static_cast<int>(tableOffset_.x) + 300, static_cast<int>(tableOffset_.y) - 50, SDL_Color{255, 255, 255, 255});
    const SDL_Color turnColor = colorRef(COLOR_RED);
    drawText(isPlayerTurn_ ? "Player's Turn" : "Robot's Turn", static_cast<int>(tableOffset_.x), static_cast<int>(tableOffset_.y) - 80, turnColor);
  }

  void render() {
    SDL_SetRenderTarget(renderer_, worldTexture_);
    renderBackground();
    renderTable();
    renderHud();

    SDL_SetRenderTarget(renderer_, nullptr);
    setDrawColor(renderer_, COLOR_BROWN);
    SDL_RenderClear(renderer_);

    const SDL_Rect dst{
        static_cast<int>(std::lround(viewOffset_.x)),
        static_cast<int>(std::lround(viewOffset_.y)),
        std::max(1, static_cast<int>(std::lround(static_cast<float>(viewWidth_) * viewScale_))),
        std::max(1, static_cast<int>(std::lround(static_cast<float>(viewHeight_) * viewScale_))),
    };
    SDL_RenderCopy(renderer_, worldTexture_, nullptr, &dst);
    SDL_RenderPresent(renderer_);
  }

  void updateBallPositions() {
    constexpr float left = INNER_X + BALL_RADIUS;
    constexpr float right = INNER_X + INNER_WIDTH - BALL_RADIUS;
    constexpr float top = INNER_Y + BALL_RADIUS;
    constexpr float bottom = INNER_Y + INNER_HEIGHT - BALL_RADIUS;

    for (Ball& ball : balls_) {
      if (ball.pocketed) {
        continue;
      }

      ball.position.x += ball.velocity.x;
      ball.position.y += ball.velocity.y;
      ball.velocity.x *= FRICTION;
      ball.velocity.y *= FRICTION;

      if (ball.position.x <= left) {
        ball.position.x = left;
        ball.velocity.x = -ball.velocity.x;
      }
      if (ball.position.x >= right) {
        ball.position.x = right;
        ball.velocity.x = -ball.velocity.x;
      }
      if (ball.position.y <= top) {
        ball.position.y = top;
        ball.velocity.y = -ball.velocity.y;
      }
      if (ball.position.y >= bottom) {
        ball.position.y = bottom;
        ball.velocity.y = -ball.velocity.y;
      }

      if (std::hypot(ball.velocity.x, ball.velocity.y) < STOP_THRESHOLD) {
        ball.velocity = Vec2{0.0f, 0.0f};
      }
    }
  }

  void handleCollisions() {
    for (std::size_t i = 0; i < balls_.size(); i += 1) {
      Ball& first = balls_[i];
      if (first.pocketed) {
        continue;
      }

      for (std::size_t j = i + 1; j < balls_.size(); j += 1) {
        Ball& second = balls_[j];
        if (second.pocketed) {
          continue;
        }

        const Vec2 delta{second.position.x - first.position.x, second.position.y - first.position.y};
        const float dist = length(delta);
        const float minDist = first.radius + second.radius;
        if (dist <= 0.0f || dist > minDist) {
          continue;
        }

        const Vec2 normal{delta.x / dist, delta.y / dist};
        const float firstNormal = first.velocity.x * normal.x + first.velocity.y * normal.y;
        const float secondNormal = second.velocity.x * normal.x + second.velocity.y * normal.y;
        const Vec2 firstTangent{
            first.velocity.x - firstNormal * normal.x,
            first.velocity.y - firstNormal * normal.y};
        const Vec2 secondTangent{
            second.velocity.x - secondNormal * normal.x,
            second.velocity.y - secondNormal * normal.y};
        const float firstAfter =
            (firstNormal * (first.mass - second.mass) + 2.0f * second.mass * secondNormal) /
            (first.mass + second.mass);
        const float secondAfter =
            (secondNormal * (second.mass - first.mass) + 2.0f * first.mass * firstNormal) /
            (first.mass + second.mass);

        first.velocity.x = firstTangent.x + firstAfter * normal.x;
        first.velocity.y = firstTangent.y + firstAfter * normal.y;
        second.velocity.x = secondTangent.x + secondAfter * normal.x;
        second.velocity.y = secondTangent.y + secondAfter * normal.y;

        const float overlap = minDist - dist;
        first.position.x -= normal.x * overlap * 0.5f;
        first.position.y -= normal.y * overlap * 0.5f;
        second.position.x += normal.x * overlap * 0.5f;
        second.position.y += normal.y * overlap * 0.5f;
      }
    }
  }

  void resetAfterWhitePocketed() {
    initializeBalls();
    isRobotTurn_ = false;
    isPlayerTurn_ = true;
    robotScore_ = 0;
    playerScore_ = 0;
    isPlacingWhiteBall_ = true;
    robotShotAt_.reset();
    isAiming_ = false;
  }

  void handlePocketing() {
    for (std::size_t i = 0; i < balls_.size(); i += 1) {
      Ball& ball = balls_[i];
      if (ball.pocketed) {
        continue;
      }

      for (const auto& pocket : POCKETS) {
        if (std::hypot(ball.position.x - pocket.first, ball.position.y - pocket.second) >= POCKET_RADIUS) {
          continue;
        }

        ball.pocketed = true;
        ball.velocity = Vec2{0.0f, 0.0f};

        if (i == 0) {
          resetAfterWhitePocketed();
          return;
        }

        if (isPlayerTurn_) {
          robotScore_ += ball.score;
        } else if (isRobotTurn_) {
          playerScore_ += ball.score;
        }
        break;
      }
    }
  }

  bool areBallsStopped() const {
    return std::all_of(balls_.begin(), balls_.end(), [](const Ball& ball) {
      return ball.velocity.x == 0.0f && ball.velocity.y == 0.0f;
    });
  }

  void robotHitBall() {
    std::vector<std::size_t> candidates;
    for (std::size_t i = 0; i < balls_.size(); i += 1) {
      if (!balls_[i].pocketed && balls_[i].color != COLOR_WHITE) {
        candidates.push_back(i);
      }
    }
    if (candidates.empty()) {
      return;
    }

    std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1);
    const Ball& target = balls_[candidates[dist(rng_)]];
    Ball& white = balls_[0];
    const Vec2 direction{target.position.x - white.position.x, target.position.y - white.position.y};
    const float directionLength = length(direction);
    if (directionLength <= 0.0f) {
      return;
    }

    white.velocity.x = direction.x / directionLength * ROBOT_SHOT_SPEED;
    white.velocity.y = direction.y / directionLength * ROBOT_SHOT_SPEED;
  }

  void updateRobotTurn(double current) {
    if (!isRobotTurn_ || !areBallsStopped()) {
      return;
    }

    if (!robotShotAt_) {
      robotShotAt_ = current + 0.02;
      return;
    }

    if (current < *robotShotAt_) {
      return;
    }

    robotHitBall();
    isRobotTurn_ = false;
    isPlayerTurn_ = true;
    robotShotAt_.reset();
  }

  void step(double current) {
    updateBallPositions();
    handleCollisions();
    handlePocketing();
    updateRobotTurn(current);
  }

  void beginAiming(Vec2 point, bool touch = false) {
    if (!areBallsStopped() || !isPlayerTurn_ || isPlacingWhiteBall_) {
      return;
    }

    cueStart_ = whiteBallScreenPosition();
    cueEnd_ = point;
    isAiming_ = true;
    touchAiming_ = touch;
  }

  void shootCueBall() {
    if (!isAiming_ || !isPlayerTurn_) {
      return;
    }

    const Vec2 delta{cueStart_.x - cueEnd_.x, cueStart_.y - cueEnd_.y};
    const float shotLength = length(delta);
    if (shotLength <= 0.0f) {
      isAiming_ = false;
      touchAiming_ = false;
      return;
    }

    balls_[0].velocity.x = delta.x / shotLength * PLAYER_SHOT_SPEED;
    balls_[0].velocity.y = delta.y / shotLength * PLAYER_SHOT_SPEED;
    isAiming_ = false;
    touchAiming_ = false;
    isPlayerTurn_ = false;
    isRobotTurn_ = true;
    robotShotAt_.reset();
  }

  void handleMotion(Vec2 point) {
    if (isPlacingWhiteBall_) {
      placeWhiteBall(point);
      return;
    }
    if (isAiming_ && areBallsStopped()) {
      cueEnd_ = point;
    }
  }

  void toggleFullscreen() {
    fullscreen_ = !fullscreen_;
    SDL_SetWindowFullscreen(window_, fullscreen_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    updateLayout();
  }

  bool handleEvents() {
    SDL_Event event{};
    while (SDL_PollEvent(&event) != 0) {
      switch (event.type) {
        case SDL_QUIT:
          return false;
        case SDL_KEYDOWN:
          if (event.key.keysym.sym == SDLK_ESCAPE) {
            return false;
          }
          if (event.key.keysym.sym == SDLK_F11) {
            toggleFullscreen();
          }
          break;
        case SDL_WINDOWEVENT:
          if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
              event.window.event == SDL_WINDOWEVENT_RESIZED) {
            updateLayout();
          }
          break;
        case SDL_MOUSEMOTION:
          handleMotion(screenToWorld(event.motion.x, event.motion.y));
          break;
        case SDL_MOUSEBUTTONDOWN: {
          const Vec2 point = screenToWorld(event.button.x, event.button.y);
          if (event.button.button == SDL_BUTTON_LEFT && isPlacingWhiteBall_ && !gameStarted_) {
            placeWhiteBall(point);
            isPlacingWhiteBall_ = false;
            gameStarted_ = true;
          } else if (event.button.button == SDL_BUTTON_RIGHT) {
            beginAiming(point);
          }
          break;
        }
        case SDL_MOUSEBUTTONUP:
          if (event.button.button == SDL_BUTTON_LEFT) {
            cueEnd_ = screenToWorld(event.button.x, event.button.y);
            shootCueBall();
          }
          break;
        case SDL_FINGERDOWN: {
          int windowW = 0;
          int windowH = 0;
          SDL_GetWindowSize(window_, &windowW, &windowH);
          const Vec2 point = screenToWorld(
              static_cast<int>(event.tfinger.x * static_cast<float>(windowW)),
              static_cast<int>(event.tfinger.y * static_cast<float>(windowH)));
          if (isPlacingWhiteBall_ && !gameStarted_) {
            placeWhiteBall(point);
            isPlacingWhiteBall_ = false;
            gameStarted_ = true;
          } else {
            beginAiming(point, true);
          }
          break;
        }
        case SDL_FINGERMOTION: {
          int windowW = 0;
          int windowH = 0;
          SDL_GetWindowSize(window_, &windowW, &windowH);
          handleMotion(screenToWorld(
              static_cast<int>(event.tfinger.x * static_cast<float>(windowW)),
              static_cast<int>(event.tfinger.y * static_cast<float>(windowH))));
          break;
        }
        case SDL_FINGERUP:
          if (touchAiming_) {
            int windowW = 0;
            int windowH = 0;
            SDL_GetWindowSize(window_, &windowW, &windowH);
            cueEnd_ = screenToWorld(
                static_cast<int>(event.tfinger.x * static_cast<float>(windowW)),
                static_cast<int>(event.tfinger.y * static_cast<float>(windowH)));
            shootCueBall();
          }
          break;
        default:
          break;
      }
    }
    return true;
  }
};

}  // namespace

int main(int argc, char** argv) {
  const bool smokeTest = std::any_of(argv + 1, argv + argc, [](const char* arg) {
    return std::string(arg) == "--smoke-test";
  });

  try {
    Game game;
    return game.run(smokeTest);
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
