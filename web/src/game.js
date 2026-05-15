const canvas = document.getElementById("game");
const ctx = canvas.getContext("2d", { alpha: false });

const TABLE_WIDTH = 840;
const TABLE_HEIGHT = 440;
const INNER_X = 20;
const INNER_Y = 20;
const INNER_WIDTH = 800;
const INNER_HEIGHT = 400;
const BALL_RADIUS = 10;
const POCKET_RADIUS = 15;
const PLACE_CENTER = { x: 220, y: 220 };
const PLACE_RADIUS = 100;
const FRICTION = 0.9919999837875366;
const STOP_THRESHOLD = 0.03400000184774399;
const PLAYER_SHOT_SPEED = 10;
const ROBOT_SHOT_SPEED = 9;
const STEP_MS = 8;
const MIN_VIEW_WIDTH = 960;
const MIN_VIEW_HEIGHT = 600;

const COLOR_WHITE = 0xffffff;
const COLOR_RED = 0x0000ff;
const COLOR_YELLOW = 0x00ffff;
const COLOR_GREEN = 0x00ff00;
const COLOR_BROWN = 0x13458b;
const COLOR_BLUE = 0xff0000;
const COLOR_PINK = 0x9314ff;
const COLOR_BLACK = 0x000000;
const COLOR_RAIL = 0x1345c8;
const COLOR_CUE = 0x2d1900;

const pockets = [
  { x: 20, y: 20 },
  { x: 420, y: 20 },
  { x: 820, y: 20 },
  { x: 20, y: 420 },
  { x: 420, y: 420 },
  { x: 820, y: 420 },
];

let balls = [];
let cueStart = { x: 0, y: 0 };
let cueEnd = { x: 0, y: 0 };
let isAiming = false;
let isPlacingWhiteBall = true;
let isRobotTurn = false;
let isPlayerTurn = true;
let gameStarted = false;
let playerScore = 0;
let robotScore = 0;
let robotShotPending = false;
let activePointerId = null;

let dpr = 1;
let viewWidth = MIN_VIEW_WIDTH;
let viewHeight = MIN_VIEW_HEIGHT;
let viewScale = 1;
let viewOffsetX = 0;
let viewOffsetY = 0;
let tableOffsetX = 60;
let tableOffsetY = 80;
let lastFrameTime = performance.now();
let accumulator = 0;

function colorRef(value) {
  const r = value & 0xff;
  const g = (value >> 8) & 0xff;
  const b = (value >> 16) & 0xff;
  return `rgb(${r}, ${g}, ${b})`;
}

function makeBall(x, y, color, score) {
  return {
    position: { x, y },
    velocity: { x: 0, y: 0 },
    mass: 1,
    radius: BALL_RADIUS,
    color,
    pocketed: false,
    score,
  };
}

function initializeBalls() {
  balls = [
    makeBall(220, 240, COLOR_WHITE, 0),
    makeBall(420, 220, COLOR_BLUE, 5),
    makeBall(220, 320, COLOR_YELLOW, 2),
    makeBall(220, 120, COLOR_GREEN, 3),
    makeBall(220, 220, COLOR_BROWN, 4),
    makeBall(600, 220, COLOR_PINK, 6),
    makeBall(640, 220, COLOR_BLACK, 7),
  ];

  const baseX = 660;
  const baseY = 220;
  const rowGap = 17.32050895690918;
  const ballGap = 20;

  for (let row = 0; row < 5; row += 1) {
    for (let col = 0; col <= row; col += 1) {
      balls.push(makeBall(baseX + row * rowGap, baseY - row * BALL_RADIUS + col * ballGap, COLOR_RED, 1));
    }
  }

  isPlacingWhiteBall = true;
  gameStarted = false;
}

function resize() {
  const cssWidth = window.innerWidth;
  const cssHeight = window.innerHeight;
  dpr = Math.max(1, window.devicePixelRatio || 1);
  viewWidth = Math.max(cssWidth, MIN_VIEW_WIDTH);
  viewHeight = Math.max(cssHeight, MIN_VIEW_HEIGHT);
  viewScale = Math.min(cssWidth / viewWidth, cssHeight / viewHeight);
  viewOffsetX = (cssWidth - viewWidth * viewScale) / 2;
  viewOffsetY = (cssHeight - viewHeight * viewScale) / 2;
  tableOffsetX = Math.trunc((viewWidth - TABLE_WIDTH) / 2);
  tableOffsetY = Math.trunc((viewHeight - TABLE_HEIGHT) / 2);

  canvas.width = Math.max(1, Math.round(cssWidth * dpr));
  canvas.height = Math.max(1, Math.round(cssHeight * dpr));
}

function toWorldPoint(event) {
  return {
    x: (event.clientX - viewOffsetX) / viewScale,
    y: (event.clientY - viewOffsetY) / viewScale,
  };
}

function whiteBallScreenPosition() {
  const white = balls[0];
  return {
    x: tableOffsetX + white.position.x,
    y: tableOffsetY + white.position.y,
  };
}

function distance(a, b) {
  return Math.hypot(a.x - b.x, a.y - b.y);
}

function placeWhiteBall(point) {
  const center = {
    x: tableOffsetX + PLACE_CENTER.x,
    y: tableOffsetY + PLACE_CENTER.y,
  };

  const insideTable =
    point.x > tableOffsetX + INNER_X + BALL_RADIUS &&
    point.x <= tableOffsetX + INNER_X + INNER_WIDTH - BALL_RADIUS - 1 &&
    point.y > tableOffsetY + INNER_Y + BALL_RADIUS &&
    point.y <= tableOffsetY + INNER_Y + INNER_HEIGHT - BALL_RADIUS - 1;

  const insidePlacementArea = distance(point, center) <= PLACE_RADIUS && point.x <= center.x;

  if (insideTable && insidePlacementArea) {
    balls[0].position.x = point.x - tableOffsetX;
    balls[0].position.y = point.y - tableOffsetY;
  }
}

function drawLine(x1, y1, x2, y2) {
  ctx.beginPath();
  ctx.moveTo(x1, y1);
  ctx.lineTo(x2, y2);
  ctx.stroke();
}

function drawCue(start, end) {
  const dx = end.x - start.x;
  const dy = end.y - start.y;
  const length = Math.hypot(dx, dy);
  if (length <= 0) {
    return;
  }

  const extended = {
    x: start.x + dx * 2,
    y: start.y + dy * 2,
  };

  ctx.save();
  ctx.lineCap = "round";
  ctx.lineWidth = 5;
  ctx.strokeStyle = colorRef(COLOR_CUE);
  drawLine(start.x, start.y, end.x, end.y);

  ctx.lineWidth = 1;
  ctx.setLineDash([3, 3]);
  ctx.strokeStyle = "white";
  drawLine(start.x, start.y, extended.x, extended.y);
  ctx.setLineDash([]);

  ctx.fillStyle = colorRef(COLOR_GREEN);
  ctx.beginPath();
  ctx.arc(extended.x, extended.y, BALL_RADIUS, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();
}

function renderTable() {
  const tx = tableOffsetX;
  const ty = tableOffsetY;

  ctx.fillStyle = colorRef(0x008000);
  ctx.fillRect(tx + INNER_X, ty + INNER_Y, INNER_WIDTH, INNER_HEIGHT);

  ctx.save();
  ctx.strokeStyle = "white";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.arc(tx + PLACE_CENTER.x, ty + PLACE_CENTER.y, PLACE_RADIUS, -Math.PI / 2, Math.PI / 2, true);
  ctx.stroke();
  drawLine(tx + PLACE_CENTER.x, ty + INNER_Y, tx + PLACE_CENTER.x, ty + INNER_Y + INNER_HEIGHT);
  ctx.restore();

  ctx.fillStyle = colorRef(COLOR_RAIL);
  ctx.fillRect(tx, ty, TABLE_WIDTH, INNER_Y);
  ctx.fillRect(tx, ty, INNER_X, TABLE_HEIGHT);
  ctx.fillRect(tx, ty + INNER_Y + INNER_HEIGHT, TABLE_WIDTH, INNER_Y);
  ctx.fillRect(tx + INNER_X + INNER_WIDTH, ty, INNER_X, TABLE_HEIGHT);

  ctx.fillStyle = colorRef(COLOR_BLACK);
  for (const pocket of pockets) {
    ctx.beginPath();
    ctx.arc(tx + pocket.x, ty + pocket.y, POCKET_RADIUS, 0, Math.PI * 2);
    ctx.fill();
  }

  for (const ball of balls) {
    if (ball.pocketed) {
      continue;
    }

    ctx.fillStyle = colorRef(ball.color);
    ctx.beginPath();
    ctx.arc(tx + ball.position.x, ty + ball.position.y, ball.radius, 0, Math.PI * 2);
    ctx.fill();
  }

  if (isAiming) {
    drawCue(cueEnd, whiteBallScreenPosition());
  }
}

function renderBackground() {
  ctx.fillStyle = colorRef(COLOR_BROWN);
  ctx.strokeStyle = colorRef(COLOR_BLACK);
  ctx.lineWidth = 1;

  for (let y = 0; y < viewHeight; y += 100) {
    for (let x = 0; x < viewWidth; x += 200) {
      ctx.fillRect(x, y, 200, 100);
      ctx.strokeRect(x, y, 200, 100);
    }
  }
}

function renderHud() {
  ctx.font = "24px Arial";
  ctx.textBaseline = "top";
  ctx.fillStyle = "white";
  ctx.fillText(`Player Score: ${playerScore}`, tableOffsetX, tableOffsetY - 50);
  ctx.fillText(`Robot Score: ${robotScore}`, tableOffsetX + 300, tableOffsetY - 50);

  ctx.fillStyle = colorRef(COLOR_RED);
  ctx.fillText(isPlayerTurn ? "Player's Turn" : "Robot's Turn", tableOffsetX, tableOffsetY - 80);
}

function render() {
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.setTransform(dpr * viewScale, 0, 0, dpr * viewScale, dpr * viewOffsetX, dpr * viewOffsetY);

  renderBackground();
  renderTable();
  renderHud();
}

function updateBallPositions() {
  const left = INNER_X + BALL_RADIUS;
  const right = INNER_X + INNER_WIDTH - BALL_RADIUS;
  const top = INNER_Y + BALL_RADIUS;
  const bottom = INNER_Y + INNER_HEIGHT - BALL_RADIUS;

  for (const ball of balls) {
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

    if (Math.hypot(ball.velocity.x, ball.velocity.y) < STOP_THRESHOLD) {
      ball.velocity.x = 0;
      ball.velocity.y = 0;
    }
  }
}

function handleCollisions() {
  for (let i = 0; i < balls.length; i += 1) {
    const first = balls[i];
    if (first.pocketed) {
      continue;
    }

    for (let j = i + 1; j < balls.length; j += 1) {
      const second = balls[j];
      if (second.pocketed) {
        continue;
      }

      const dx = second.position.x - first.position.x;
      const dy = second.position.y - first.position.y;
      const dist = Math.hypot(dx, dy);
      const minDist = first.radius + second.radius;

      if (dist <= 0 || dist > minDist) {
        continue;
      }

      const nx = dx / dist;
      const ny = dy / dist;
      const firstNormal = first.velocity.x * nx + first.velocity.y * ny;
      const secondNormal = second.velocity.x * nx + second.velocity.y * ny;
      const firstTangentX = first.velocity.x - firstNormal * nx;
      const firstTangentY = first.velocity.y - firstNormal * ny;
      const secondTangentX = second.velocity.x - secondNormal * nx;
      const secondTangentY = second.velocity.y - secondNormal * ny;
      const firstAfter =
        (firstNormal * (first.mass - second.mass) + 2 * second.mass * secondNormal) /
        (first.mass + second.mass);
      const secondAfter =
        (secondNormal * (second.mass - first.mass) + 2 * first.mass * firstNormal) /
        (first.mass + second.mass);

      first.velocity.x = firstTangentX + firstAfter * nx;
      first.velocity.y = firstTangentY + firstAfter * ny;
      second.velocity.x = secondTangentX + secondAfter * nx;
      second.velocity.y = secondTangentY + secondAfter * ny;

      const overlap = minDist - dist;
      first.position.x -= nx * overlap * 0.5;
      first.position.y -= ny * overlap * 0.5;
      second.position.x += nx * overlap * 0.5;
      second.position.y += ny * overlap * 0.5;
    }
  }
}

function resetAfterWhitePocketed() {
  initializeBalls();
  isRobotTurn = false;
  isPlayerTurn = true;
  robotScore = 0;
  playerScore = 0;
  isPlacingWhiteBall = true;
  robotShotPending = false;
  isAiming = false;
}

function handlePocketing() {
  for (let i = 0; i < balls.length; i += 1) {
    const ball = balls[i];
    if (ball.pocketed) {
      continue;
    }

    for (const pocket of pockets) {
      if (Math.hypot(ball.position.x - pocket.x, ball.position.y - pocket.y) >= POCKET_RADIUS) {
        continue;
      }

      ball.pocketed = true;
      ball.velocity.x = 0;
      ball.velocity.y = 0;

      if (i === 0) {
        resetAfterWhitePocketed();
        return;
      }

      if (isPlayerTurn) {
        robotScore += ball.score;
      } else if (isRobotTurn) {
        playerScore += ball.score;
      }

      break;
    }
  }
}

function areBallsStopped() {
  return balls.every((ball) => ball.velocity.x === 0 && ball.velocity.y === 0);
}

function robotHitBall() {
  const candidates = balls.filter((ball) => !ball.pocketed && ball.color !== COLOR_WHITE);
  if (candidates.length === 0) {
    return;
  }

  const target = candidates[Math.floor(Math.random() * candidates.length)];
  const white = balls[0];
  const dx = target.position.x - white.position.x;
  const dy = target.position.y - white.position.y;
  const length = Math.hypot(dx, dy);
  if (length <= 0) {
    return;
  }

  white.velocity.x = (dx / length) * ROBOT_SHOT_SPEED;
  white.velocity.y = (dy / length) * ROBOT_SHOT_SPEED;
}

function updateRobotTurn() {
  if (!isRobotTurn || robotShotPending || !areBallsStopped()) {
    return;
  }

  robotShotPending = true;
  window.setTimeout(() => {
    if (isRobotTurn && areBallsStopped()) {
      robotHitBall();
      isRobotTurn = false;
      isPlayerTurn = true;
    }
    robotShotPending = false;
  }, 20);
}

function step() {
  updateBallPositions();
  handleCollisions();
  handlePocketing();
  updateRobotTurn();
}

function shootCueBall() {
  if (!isAiming || !isPlayerTurn) {
    return;
  }

  const dx = cueStart.x - cueEnd.x;
  const dy = cueStart.y - cueEnd.y;
  const length = Math.hypot(dx, dy);
  if (length <= 0) {
    isAiming = false;
    return;
  }

  balls[0].velocity.x = (dx / length) * PLAYER_SHOT_SPEED;
  balls[0].velocity.y = (dy / length) * PLAYER_SHOT_SPEED;
  isAiming = false;
  isPlayerTurn = false;
  isRobotTurn = true;
}

function beginAiming(point, pointerId = null) {
  if (!areBallsStopped() || !isPlayerTurn || isPlacingWhiteBall) {
    return;
  }

  cueStart = whiteBallScreenPosition();
  cueEnd = point;
  isAiming = true;
  activePointerId = pointerId;
}

canvas.addEventListener("contextmenu", (event) => {
  event.preventDefault();
});

canvas.addEventListener("pointerdown", (event) => {
  event.preventDefault();
  const point = toWorldPoint(event);

  if (event.pointerType !== "mouse" && canvas.setPointerCapture) {
    canvas.setPointerCapture(event.pointerId);
  }

  if (isPlacingWhiteBall && !gameStarted && event.button === 0) {
    placeWhiteBall(point);
    isPlacingWhiteBall = false;
    gameStarted = true;
    return;
  }

  if (event.pointerType === "mouse" && event.button === 2) {
    beginAiming(point, null);
    return;
  }

  if (event.pointerType !== "mouse" && event.button === 0) {
    beginAiming(point, event.pointerId);
  }
});

canvas.addEventListener("pointermove", (event) => {
  event.preventDefault();
  const point = toWorldPoint(event);

  if (isPlacingWhiteBall) {
    placeWhiteBall(point);
    return;
  }

  if (isAiming && (activePointerId === null || activePointerId === event.pointerId) && areBallsStopped()) {
    cueEnd = point;
  }
});

canvas.addEventListener("pointerup", (event) => {
  event.preventDefault();
  const point = toWorldPoint(event);

  if (event.pointerType !== "mouse" && activePointerId === event.pointerId) {
    cueEnd = point;
    shootCueBall();
    activePointerId = null;
    return;
  }

  if (event.pointerType === "mouse" && event.button === 0) {
    cueEnd = point;
    shootCueBall();
  }
});

canvas.addEventListener("pointercancel", (event) => {
  if (activePointerId === event.pointerId) {
    activePointerId = null;
    isAiming = false;
  }
});

window.addEventListener("resize", resize);

function frame(now) {
  const delta = Math.min(64, now - lastFrameTime);
  lastFrameTime = now;
  accumulator += delta;

  while (accumulator >= STEP_MS) {
    step();
    accumulator -= STEP_MS;
  }

  render();
  window.requestAnimationFrame(frame);
}

initializeBalls();
resize();
window.requestAnimationFrame(frame);
