from __future__ import annotations

import math
import os
import random
import sys
import time
from dataclasses import dataclass

os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import pygame


TABLE_WIDTH = 840
TABLE_HEIGHT = 440
INNER_X = 20
INNER_Y = 20
INNER_WIDTH = 800
INNER_HEIGHT = 400
BALL_RADIUS = 10.0
POCKET_RADIUS = 15.0
PLACE_CENTER = (220.0, 220.0)
PLACE_RADIUS = 100.0
FRICTION = 0.9919999837875366
STOP_THRESHOLD = 0.03400000184774399
PLAYER_SHOT_SPEED = 10.0
ROBOT_SHOT_SPEED = 9.0
STEP_SECONDS = 0.008
MIN_VIEW_WIDTH = 960
MIN_VIEW_HEIGHT = 600

COLOR_WHITE = 0xFFFFFF
COLOR_RED = 0x0000FF
COLOR_YELLOW = 0x00FFFF
COLOR_GREEN = 0x00FF00
COLOR_BROWN = 0x13458B
COLOR_BLUE = 0xFF0000
COLOR_PINK = 0x9314FF
COLOR_BLACK = 0x000000
COLOR_RAIL = 0x1345C8
COLOR_CUE = 0x2D1900

POCKETS = [
    (20.0, 20.0),
    (420.0, 20.0),
    (820.0, 20.0),
    (20.0, 420.0),
    (420.0, 420.0),
    (820.0, 420.0),
]


@dataclass
class Vec2:
    x: float
    y: float

    def copy(self) -> "Vec2":
        return Vec2(self.x, self.y)


@dataclass
class Ball:
    position: Vec2
    velocity: Vec2
    mass: float
    radius: float
    color: int
    pocketed: bool
    score: int


class SnookerGame:
    def __init__(self) -> None:
        pygame.init()
        pygame.display.set_caption("Snooker")

        info = pygame.display.Info()
        width = max(MIN_VIEW_WIDTH, info.current_w or 1024)
        height = max(MIN_VIEW_HEIGHT, info.current_h or 768)
        self.fullscreen = False
        self.screen = pygame.display.set_mode((width, height), pygame.RESIZABLE)
        self.clock = pygame.time.Clock()
        self.font = pygame.font.SysFont("Arial", 24)

        self.balls: list[Ball] = []
        self.cue_start = Vec2(0.0, 0.0)
        self.cue_end = Vec2(0.0, 0.0)
        self.is_aiming = False
        self.is_placing_white_ball = True
        self.is_robot_turn = False
        self.is_player_turn = True
        self.game_started = False
        self.player_score = 0
        self.robot_score = 0
        self.robot_shot_at: float | None = None
        self.touch_aiming = False

        self.view_width = MIN_VIEW_WIDTH
        self.view_height = MIN_VIEW_HEIGHT
        self.view_scale = 1.0
        self.view_offset = Vec2(0.0, 0.0)
        self.table_offset = Vec2(60.0, 80.0)
        self.world = pygame.Surface((MIN_VIEW_WIDTH, MIN_VIEW_HEIGHT))
        self.update_layout()
        self.initialize_balls()

    def color_ref(self, value: int) -> tuple[int, int, int]:
        return (value & 0xFF, (value >> 8) & 0xFF, (value >> 16) & 0xFF)

    def make_ball(self, x: float, y: float, color: int, score: int) -> Ball:
        return Ball(Vec2(x, y), Vec2(0.0, 0.0), 1.0, BALL_RADIUS, color, False, score)

    def initialize_balls(self) -> None:
        self.balls = [
            self.make_ball(220.0, 240.0, COLOR_WHITE, 0),
            self.make_ball(420.0, 220.0, COLOR_BLUE, 5),
            self.make_ball(220.0, 320.0, COLOR_YELLOW, 2),
            self.make_ball(220.0, 120.0, COLOR_GREEN, 3),
            self.make_ball(220.0, 220.0, COLOR_BROWN, 4),
            self.make_ball(600.0, 220.0, COLOR_PINK, 6),
            self.make_ball(640.0, 220.0, COLOR_BLACK, 7),
        ]

        base_x = 660.0
        base_y = 220.0
        row_gap = 17.32050895690918

        for row in range(5):
            for col in range(row + 1):
                x = base_x + row * row_gap
                y = base_y - row * BALL_RADIUS + col * BALL_RADIUS * 2.0
                self.balls.append(self.make_ball(x, y, COLOR_RED, 1))

        self.is_placing_white_ball = True
        self.game_started = False

    def update_layout(self) -> None:
        window_w, window_h = self.screen.get_size()
        self.view_width = max(window_w, MIN_VIEW_WIDTH)
        self.view_height = max(window_h, MIN_VIEW_HEIGHT)
        self.view_scale = min(window_w / self.view_width, window_h / self.view_height)
        self.view_offset = Vec2(
            (window_w - self.view_width * self.view_scale) / 2.0,
            (window_h - self.view_height * self.view_scale) / 2.0,
        )
        self.table_offset = Vec2(
            math.trunc((self.view_width - TABLE_WIDTH) / 2.0),
            math.trunc((self.view_height - TABLE_HEIGHT) / 2.0),
        )
        self.world = pygame.Surface((self.view_width, self.view_height))

    def screen_to_world(self, point: tuple[float, float]) -> Vec2:
        return Vec2(
            (point[0] - self.view_offset.x) / self.view_scale,
            (point[1] - self.view_offset.y) / self.view_scale,
        )

    def white_ball_screen_position(self) -> Vec2:
        white = self.balls[0]
        return Vec2(
            self.table_offset.x + white.position.x,
            self.table_offset.y + white.position.y,
        )

    def place_white_ball(self, point: Vec2) -> None:
        center = Vec2(
            self.table_offset.x + PLACE_CENTER[0],
            self.table_offset.y + PLACE_CENTER[1],
        )
        inside_table = (
            point.x > self.table_offset.x + INNER_X + BALL_RADIUS
            and point.x <= self.table_offset.x + INNER_X + INNER_WIDTH - BALL_RADIUS - 1
            and point.y > self.table_offset.y + INNER_Y + BALL_RADIUS
            and point.y <= self.table_offset.y + INNER_Y + INNER_HEIGHT - BALL_RADIUS - 1
        )
        inside_placement = math.hypot(point.x - center.x, point.y - center.y) <= PLACE_RADIUS and point.x <= center.x

        if inside_table and inside_placement:
            self.balls[0].position.x = point.x - self.table_offset.x
            self.balls[0].position.y = point.y - self.table_offset.y

    def draw_dotted_line(self, start: Vec2, end: Vec2, color: tuple[int, int, int]) -> None:
        dx = end.x - start.x
        dy = end.y - start.y
        length = math.hypot(dx, dy)
        if length <= 0:
            return

        dash = 3.0
        gap = 3.0
        ux = dx / length
        uy = dy / length
        cursor = 0.0
        while cursor < length:
            next_cursor = min(cursor + dash, length)
            p1 = (round(start.x + ux * cursor), round(start.y + uy * cursor))
            p2 = (round(start.x + ux * next_cursor), round(start.y + uy * next_cursor))
            pygame.draw.line(self.world, color, p1, p2, 1)
            cursor += dash + gap

    def draw_cue(self, start: Vec2, end: Vec2) -> None:
        dx = end.x - start.x
        dy = end.y - start.y
        length = math.hypot(dx, dy)
        if length <= 0:
            return

        extended = Vec2(start.x + dx * 2.0, start.y + dy * 2.0)
        pygame.draw.line(
            self.world,
            self.color_ref(COLOR_CUE),
            (round(start.x), round(start.y)),
            (round(end.x), round(end.y)),
            5,
        )
        self.draw_dotted_line(start, extended, (255, 255, 255))
        pygame.draw.circle(
            self.world,
            self.color_ref(COLOR_GREEN),
            (round(extended.x), round(extended.y)),
            round(BALL_RADIUS),
        )

    def render_background(self) -> None:
        self.world.fill(self.color_ref(COLOR_BROWN))
        for y in range(0, self.view_height, 100):
            for x in range(0, self.view_width, 200):
                rect = pygame.Rect(x, y, 200, 100)
                pygame.draw.rect(self.world, self.color_ref(COLOR_BROWN), rect)
                pygame.draw.rect(self.world, self.color_ref(COLOR_BLACK), rect, 1)

    def render_table(self) -> None:
        tx = self.table_offset.x
        ty = self.table_offset.y
        inner = pygame.Rect(round(tx + INNER_X), round(ty + INNER_Y), INNER_WIDTH, INNER_HEIGHT)
        pygame.draw.rect(self.world, self.color_ref(0x008000), inner)

        arc_rect = pygame.Rect(
            round(tx + PLACE_CENTER[0] - PLACE_RADIUS),
            round(ty + PLACE_CENTER[1] - PLACE_RADIUS),
            round(PLACE_RADIUS * 2),
            round(PLACE_RADIUS * 2),
        )
        pygame.draw.arc(self.world, (255, 255, 255), arc_rect, math.pi / 2.0, math.pi * 3.0 / 2.0, 1)
        pygame.draw.line(
            self.world,
            (255, 255, 255),
            (round(tx + PLACE_CENTER[0]), round(ty + INNER_Y)),
            (round(tx + PLACE_CENTER[0]), round(ty + INNER_Y + INNER_HEIGHT)),
            1,
        )

        rail_color = self.color_ref(COLOR_RAIL)
        pygame.draw.rect(self.world, rail_color, pygame.Rect(round(tx), round(ty), TABLE_WIDTH, INNER_Y))
        pygame.draw.rect(self.world, rail_color, pygame.Rect(round(tx), round(ty), INNER_X, TABLE_HEIGHT))
        pygame.draw.rect(
            self.world,
            rail_color,
            pygame.Rect(round(tx), round(ty + INNER_Y + INNER_HEIGHT), TABLE_WIDTH, INNER_Y),
        )
        pygame.draw.rect(
            self.world,
            rail_color,
            pygame.Rect(round(tx + INNER_X + INNER_WIDTH), round(ty), INNER_X, TABLE_HEIGHT),
        )

        for pocket in POCKETS:
            pygame.draw.circle(
                self.world,
                self.color_ref(COLOR_BLACK),
                (round(tx + pocket[0]), round(ty + pocket[1])),
                round(POCKET_RADIUS),
            )

        for ball in self.balls:
            if ball.pocketed:
                continue
            pygame.draw.circle(
                self.world,
                self.color_ref(ball.color),
                (round(tx + ball.position.x), round(ty + ball.position.y)),
                round(ball.radius),
            )

        if self.is_aiming:
            self.draw_cue(self.cue_end, self.white_ball_screen_position())

    def render_hud(self) -> None:
        score_color = (255, 255, 255)
        turn_color = self.color_ref(COLOR_RED)
        self.world.blit(self.font.render(f"Player Score: {self.player_score}", True, score_color), (self.table_offset.x, self.table_offset.y - 50))
        self.world.blit(self.font.render(f"Robot Score: {self.robot_score}", True, score_color), (self.table_offset.x + 300, self.table_offset.y - 50))
        self.world.blit(
            self.font.render("Player's Turn" if self.is_player_turn else "Robot's Turn", True, turn_color),
            (self.table_offset.x, self.table_offset.y - 80),
        )

    def render(self) -> None:
        self.render_background()
        self.render_table()
        self.render_hud()

        window_w, window_h = self.screen.get_size()
        target_w = max(1, round(self.view_width * self.view_scale))
        target_h = max(1, round(self.view_height * self.view_scale))
        if target_w == self.view_width and target_h == self.view_height:
            frame = self.world
        else:
            frame = pygame.transform.smoothscale(self.world, (target_w, target_h))

        self.screen.fill(self.color_ref(COLOR_BROWN))
        self.screen.blit(frame, (round(self.view_offset.x), round(self.view_offset.y)))
        pygame.display.flip()

    def update_ball_positions(self) -> None:
        left = INNER_X + BALL_RADIUS
        right = INNER_X + INNER_WIDTH - BALL_RADIUS
        top = INNER_Y + BALL_RADIUS
        bottom = INNER_Y + INNER_HEIGHT - BALL_RADIUS

        for ball in self.balls:
            if ball.pocketed:
                continue

            ball.position.x += ball.velocity.x
            ball.position.y += ball.velocity.y
            ball.velocity.x *= FRICTION
            ball.velocity.y *= FRICTION

            if ball.position.x <= left:
                ball.position.x = left
                ball.velocity.x = -ball.velocity.x
            if ball.position.x >= right:
                ball.position.x = right
                ball.velocity.x = -ball.velocity.x
            if ball.position.y <= top:
                ball.position.y = top
                ball.velocity.y = -ball.velocity.y
            if ball.position.y >= bottom:
                ball.position.y = bottom
                ball.velocity.y = -ball.velocity.y

            if math.hypot(ball.velocity.x, ball.velocity.y) < STOP_THRESHOLD:
                ball.velocity.x = 0.0
                ball.velocity.y = 0.0

    def handle_collisions(self) -> None:
        for i, first in enumerate(self.balls):
            if first.pocketed:
                continue
            for second in self.balls[i + 1 :]:
                if second.pocketed:
                    continue

                dx = second.position.x - first.position.x
                dy = second.position.y - first.position.y
                dist = math.hypot(dx, dy)
                min_dist = first.radius + second.radius
                if dist <= 0 or dist > min_dist:
                    continue

                nx = dx / dist
                ny = dy / dist
                first_normal = first.velocity.x * nx + first.velocity.y * ny
                second_normal = second.velocity.x * nx + second.velocity.y * ny
                first_tangent = Vec2(first.velocity.x - first_normal * nx, first.velocity.y - first_normal * ny)
                second_tangent = Vec2(second.velocity.x - second_normal * nx, second.velocity.y - second_normal * ny)
                first_after = (
                    first_normal * (first.mass - second.mass) + 2.0 * second.mass * second_normal
                ) / (first.mass + second.mass)
                second_after = (
                    second_normal * (second.mass - first.mass) + 2.0 * first.mass * first_normal
                ) / (first.mass + second.mass)

                first.velocity.x = first_tangent.x + first_after * nx
                first.velocity.y = first_tangent.y + first_after * ny
                second.velocity.x = second_tangent.x + second_after * nx
                second.velocity.y = second_tangent.y + second_after * ny

                overlap = min_dist - dist
                first.position.x -= nx * overlap * 0.5
                first.position.y -= ny * overlap * 0.5
                second.position.x += nx * overlap * 0.5
                second.position.y += ny * overlap * 0.5

    def reset_after_white_pocketed(self) -> None:
        self.initialize_balls()
        self.is_robot_turn = False
        self.is_player_turn = True
        self.robot_score = 0
        self.player_score = 0
        self.is_placing_white_ball = True
        self.robot_shot_at = None
        self.is_aiming = False

    def handle_pocketing(self) -> None:
        for index, ball in enumerate(self.balls):
            if ball.pocketed:
                continue

            for pocket in POCKETS:
                if math.hypot(ball.position.x - pocket[0], ball.position.y - pocket[1]) >= POCKET_RADIUS:
                    continue

                ball.pocketed = True
                ball.velocity.x = 0.0
                ball.velocity.y = 0.0

                if index == 0:
                    self.reset_after_white_pocketed()
                    return

                if self.is_player_turn:
                    self.robot_score += ball.score
                elif self.is_robot_turn:
                    self.player_score += ball.score
                break

    def are_balls_stopped(self) -> bool:
        return all(ball.velocity.x == 0.0 and ball.velocity.y == 0.0 for ball in self.balls)

    def robot_hit_ball(self) -> None:
        candidates = [ball for ball in self.balls if not ball.pocketed and ball.color != COLOR_WHITE]
        if not candidates:
            return

        target = random.choice(candidates)
        white = self.balls[0]
        dx = target.position.x - white.position.x
        dy = target.position.y - white.position.y
        length = math.hypot(dx, dy)
        if length <= 0:
            return

        white.velocity.x = dx / length * ROBOT_SHOT_SPEED
        white.velocity.y = dy / length * ROBOT_SHOT_SPEED

    def update_robot_turn(self, now: float) -> None:
        if not self.is_robot_turn or not self.are_balls_stopped():
            return

        if self.robot_shot_at is None:
            self.robot_shot_at = now + 0.02
            return

        if now < self.robot_shot_at:
            return

        self.robot_hit_ball()
        self.is_robot_turn = False
        self.is_player_turn = True
        self.robot_shot_at = None

    def step(self, now: float) -> None:
        self.update_ball_positions()
        self.handle_collisions()
        self.handle_pocketing()
        self.update_robot_turn(now)

    def begin_aiming(self, point: Vec2, touch: bool = False) -> None:
        if not self.are_balls_stopped() or not self.is_player_turn or self.is_placing_white_ball:
            return

        self.cue_start = self.white_ball_screen_position()
        self.cue_end = point.copy()
        self.is_aiming = True
        self.touch_aiming = touch

    def shoot_cue_ball(self) -> None:
        if not self.is_aiming or not self.is_player_turn:
            return

        dx = self.cue_start.x - self.cue_end.x
        dy = self.cue_start.y - self.cue_end.y
        length = math.hypot(dx, dy)
        if length <= 0:
            self.is_aiming = False
            return

        self.balls[0].velocity.x = dx / length * PLAYER_SHOT_SPEED
        self.balls[0].velocity.y = dy / length * PLAYER_SHOT_SPEED
        self.is_aiming = False
        self.touch_aiming = False
        self.is_player_turn = False
        self.is_robot_turn = True
        self.robot_shot_at = None

    def toggle_fullscreen(self) -> None:
        self.fullscreen = not self.fullscreen
        flags = pygame.FULLSCREEN if self.fullscreen else pygame.RESIZABLE
        self.screen = pygame.display.set_mode((0, 0), flags)
        self.update_layout()

    def handle_mouse_motion(self, point: Vec2) -> None:
        if self.is_placing_white_ball:
            self.place_white_ball(point)
            return
        if self.is_aiming and self.are_balls_stopped():
            self.cue_end = point.copy()

    def handle_events(self) -> bool:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return False

            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    return False
                if event.key == pygame.K_F11:
                    self.toggle_fullscreen()

            if event.type == pygame.VIDEORESIZE and not self.fullscreen:
                self.screen = pygame.display.set_mode(event.size, pygame.RESIZABLE)
                self.update_layout()

            if event.type == pygame.MOUSEMOTION:
                self.handle_mouse_motion(self.screen_to_world(event.pos))

            if event.type == pygame.MOUSEBUTTONDOWN:
                point = self.screen_to_world(event.pos)
                if event.button == 1 and self.is_placing_white_ball and not self.game_started:
                    self.place_white_ball(point)
                    self.is_placing_white_ball = False
                    self.game_started = True
                elif event.button == 3:
                    self.begin_aiming(point)

            if event.type == pygame.MOUSEBUTTONUP and event.button == 1:
                self.cue_end = self.screen_to_world(event.pos)
                self.shoot_cue_ball()

            if event.type == pygame.FINGERDOWN:
                point = self.screen_to_world((event.x * self.screen.get_width(), event.y * self.screen.get_height()))
                if self.is_placing_white_ball and not self.game_started:
                    self.place_white_ball(point)
                    self.is_placing_white_ball = False
                    self.game_started = True
                else:
                    self.begin_aiming(point, touch=True)

            if event.type == pygame.FINGERMOTION:
                point = self.screen_to_world((event.x * self.screen.get_width(), event.y * self.screen.get_height()))
                self.handle_mouse_motion(point)

            if event.type == pygame.FINGERUP and self.touch_aiming:
                self.cue_end = self.screen_to_world((event.x * self.screen.get_width(), event.y * self.screen.get_height()))
                self.shoot_cue_ball()

        return True

    def run(self) -> int:
        running = True
        previous = time.perf_counter()
        accumulator = 0.0

        while running:
            running = self.handle_events()
            now = time.perf_counter()
            delta = min(0.064, now - previous)
            previous = now
            accumulator += delta

            while accumulator >= STEP_SECONDS:
                self.step(now)
                accumulator -= STEP_SECONDS

            self.render()
            self.clock.tick(120)

        pygame.quit()
        return 0


def main() -> int:
    game = SnookerGame()
    if "--smoke-test" in sys.argv[1:]:
        game.step(time.perf_counter())
        game.render()
        pygame.quit()
        return 0
    return game.run()


if __name__ == "__main__":
    sys.exit(main())
