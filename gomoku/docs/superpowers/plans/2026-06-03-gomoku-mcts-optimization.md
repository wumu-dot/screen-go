# 五子棋 MCTS AI 优化 — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将五子棋 AI 从纯启发式评分升级为 MCTS（蒙特卡洛树搜索）+ 启发式引导 + 形状检测

**Architecture:** 从 `index.html` 中拆分出 5 个独立的 JS 模块文件（`constants.js`、`board.js`、`evaluate.js`、`mcts.js`、`ai.js`），保留现有渲染和 UI 逻辑不变。所有模块通过 `window.Gomoku` 命名空间暴露，用 `<script>` 标签按依赖顺序加载。

**Tech Stack:** 纯 HTML/CSS/JS（无构建工具、无外部依赖），在浏览器中直接打开。

---

## 文件总览

```
gomoku/
├── index.html          # 修改：移除 AI 逻辑，引用 js/*.js，保留渲染/UI/事件
├── js/
│   ├── constants.js    # 新建：棋盘常量、AI 配置、方向数组
│   ├── board.js        # 新建：棋盘操作、胜负判定、棋盘克隆
│   ├── evaluate.js     # 新建：启发式打分 + 形状检测
│   ├── mcts.js         # 新建：MCTS 节点类 + 四步搜索循环
│   └── ai.js           # 新建：AI 入口，调度 MCTS，难度控制
└── README.md           # 不改动
```

### 模块接口约定

所有模块通过 `window.Gomoku` 命名空间暴露函数：

| 模块 | 暴露的函数 |
|------|-----------|
| `constants.js` | `Gomoku.BOARD_SIZE`, `Gomoku.DIRECTIONS`, `Gomoku.DIFFICULTY`, 等 |
| `board.js` | `Gomoku.createBoard()`, `Gomoku.cloneBoard()`, `Gomoku.applyMove()`, `Gomoku.checkWin()`, `Gomoku.isBoardFull()`, `Gomoku.getCandidateMoves()` |
| `evaluate.js` | `Gomoku.evaluatePosition()`, `Gomoku.evaluateBoard()`, `Gomoku.findKillerMove()` |
| `mcts.js` | `Gomoku.MCTSNode` 类, `Gomoku.mctsSearch()` |
| `ai.js` | `Gomoku.findBestMove()` |

---

### Task 1: 创建 constants.js

**Files:**
- Create: `gomoku/js/constants.js`

- [ ] **Step 1: 编写常量模块**

```js
// gomoku/js/constants.js
// 所有游戏常量集中管理

window.Gomoku = window.Gomoku || {};

(function (G) {
  // ---- 棋盘 ----
  G.BOARD_SIZE = 15;
  G.CELL_SIZE  = 36;
  G.PADDING    = 30;
  G.STONE_R    = 15;

  // ---- 棋子 ----
  G.EMPTY  = 0;
  G.BLACK  = 1;   // 黑棋先手（人类）
  G.WHITE  = 2;   // 白棋后手（AI）

  // ---- 方向向量：横、竖、主对角线、副对角线 ----
  G.DIRECTIONS = [[1, 0], [0, 1], [1, 1], [1, -1]];

  // ---- 计时 ----
  G.MOVE_TIME = 30;

  // ---- AI 难度配置 ----
  G.DIFFICULTY = {
    easy:   { timeLimit: 300,   C: 1.8 },
    medium: { timeLimit: 1500,  C: 1.414 },
    hard:   { timeLimit: 4000,  C: 1.2 }
  };

  // ---- 启发式评分权重 ----
  G.WEIGHTS = {
    attack:  1.15,   // 进攻权重
    defense: 0.95    // 防守权重
  };

  // ---- 形状分值 ----
  G.SHAPE_SCORES = {
    FIVE:         100000,
    LIVE_FOUR:     10000,
    RUSH_FOUR:      1000,
    LIVE_THREE:     1000,
    SLEEP_THREE:     100,
    LIVE_TWO:        100,
    SLEEP_TWO:        10
  };

  // ---- 杀招形状加成（evaluateBoard 用） ----
  G.KILLER_BONUS = {
    FOUR_THREE:    50000,  // 四三
    DOUBLE_FOUR:   50000,  // 双四
    FOUR_FOUR:     30000,  // 四四
    DOUBLE_THREE:  10000,  // 双活三
    THREE_SLEEP3:   3000   // 活三 + 眠三
  };

})(window.Gomoku);
```

- [ ] **Step 2: 提交**

```bash
git -C "C:/Users/wumu2/OneDrive/桌面/claude" add gomoku/js/constants.js
git -C "C:/Users/wumu2/OneDrive/桌面/claude" commit -m "feat: 添加 constants.js 常量模块"
```

---

### Task 2: 创建 board.js

**Files:**
- Create: `gomoku/js/board.js`

- [ ] **Step 1: 编写棋盘操作模块**

```js
// gomoku/js/board.js
// 棋盘状态操作：创建、克隆、落子、胜负判定、候选位置

window.Gomoku = window.Gomoku || {};

(function (G) {
  const N = G.BOARD_SIZE;

  // ---- 创建空棋盘 ----
  G.createBoard = function () {
    return Array.from({ length: N }, () => Array(N).fill(G.EMPTY));
  };

  // ---- 深拷贝棋盘 ----
  G.cloneBoard = function (board) {
    return board.map(row => row.slice());
  };

  // ---- 落子（不检查合法性，调用方保证合法） ----
  G.applyMove = function (board, row, col, player) {
    board[row][col] = player;
  };

  // ---- 撤销落子 ----
  G.undoMove = function (board, row, col) {
    board[row][col] = G.EMPTY;
  };

  // ---- 检查位置是否合法 ----
  G.isValidMove = function (board, row, col) {
    return row >= 0 && row < N && col >= 0 && col < N && board[row][col] === G.EMPTY;
  };

  // ---- 胜负判定 ----
  // 返回 { won: boolean, winLine: [{row, col}, ...] }
  G.checkWin = function (board, row, col, player) {
    for (const [dr, dc] of G.DIRECTIONS) {
      let count = 1;

      // 正方向
      for (let i = 1; i < 5; i++) {
        const r = row + dr * i, c = col + dc * i;
        if (r < 0 || r >= N || c < 0 || c >= N || board[r][c] !== player) break;
        count++;
      }
      // 反方向
      for (let i = 1; i < 5; i++) {
        const r = row - dr * i, c = col - dc * i;
        if (r < 0 || r >= N || c < 0 || c >= N || board[r][c] !== player) break;
        count++;
      }

      if (count >= 5) {
        // 收集获胜 5 子坐标
        const winLine = [];
        let sr = row, sc = col;
        while (true) {
          const nr = sr - dr, nc = sc - dc;
          if (nr < 0 || nr >= N || nc < 0 || nc >= N || board[nr][nc] !== player) break;
          sr = nr; sc = nc;
        }
        for (let i = 0; i < 5; i++) {
          winLine.push({ row: sr + dr * i, col: sc + dc * i });
        }
        return { won: true, winLine };
      }
    }
    return { won: false, winLine: [] };
  };

  // ---- 棋盘是否已满 ----
  G.isBoardFull = function (board) {
    for (let r = 0; r < N; r++)
      for (let c = 0; c < N; c++)
        if (board[r][c] === G.EMPTY) return false;
    return true;
  };

  // ---- 获取候选落子位置 ----
  // 只返回周围 2 格内有棋子的空位（剪枝优化）
  // 空棋盘时返回天元
  G.getCandidateMoves = function (board) {
    const candidates = [];
    const seen = new Set();

    for (let r = 0; r < N; r++) {
      for (let c = 0; c < N; c++) {
        if (board[r][c] === G.EMPTY) continue;
        // 以每颗棋子为中心，扩展 2 格范围
        for (let dr = -2; dr <= 2; dr++) {
          for (let dc = -2; dc <= 2; dc++) {
            const nr = r + dr, nc = c + dc;
            if (nr < 0 || nr >= N || nc < 0 || nc >= N) continue;
            if (board[nr][nc] !== G.EMPTY) continue;
            const key = nr * N + nc;
            if (seen.has(key)) continue;
            seen.add(key);
            candidates.push({ row: nr, col: nc });
          }
        }
      }
    }

    if (candidates.length === 0) {
      return [{ row: 7, col: 7 }]; // 天元开局
    }
    return candidates;
  };

  // ---- 判断棋盘是否为空 ----
  G.isBoardEmpty = function (board) {
    for (let r = 0; r < N; r++)
      for (let c = 0; c < N; c++)
        if (board[r][c] !== G.EMPTY) return false;
    return true;
  };

})(window.Gomoku);
```

- [ ] **Step 2: 提交**

```bash
git -C "C:/Users/wumu2/OneDrive/桌面/claude" add gomoku/js/board.js
git -C "C:/Users/wumu2/OneDrive/桌面/claude" commit -m "feat: 添加 board.js 棋盘操作模块"
```

---

### Task 3: 创建 evaluate.js

**Files:**
- Create: `gomoku/js/evaluate.js`

- [ ] **Step 1: 编写启发式评估模块（含形状检测）**

```js
// gomoku/js/evaluate.js
// 启发式评估：局部方向打分 + 全局形状检测 + 杀招查找

window.Gomoku = window.Gomoku || {};

(function (G) {
  const N = G.BOARD_SIZE;

  // ================================================================
  //  局部方向打分（从现有 evaluatePosition 迁移，逻辑不变）
  // ================================================================
  G.evaluatePosition = function (board, row, col, player) {
    let total = 0;

    for (const [dr, dc] of G.DIRECTIONS) {
      let count = 1;
      let openEnds = 0;

      // 正方向
      let r = row + dr, c = col + dc;
      while (r >= 0 && r < N && c >= 0 && c < N && board[r][c] === player) {
        count++; r += dr; c += dc;
      }
      if (r >= 0 && r < N && c >= 0 && c < N && board[r][c] === G.EMPTY) openEnds++;

      // 反方向
      r = row - dr; c = col - dc;
      while (r >= 0 && r < N && c >= 0 && c < N && board[r][c] === player) {
        count++; r -= dr; c -= dc;
      }
      if (r >= 0 && r < N && c >= 0 && c < N && board[r][c] === G.EMPTY) openEnds++;

      // 评分
      if (count >= 5) {
        total += 100000;
      } else if (count === 4) {
        total += openEnds === 2 ? 10000 : (openEnds === 1 ? 1000 : 0);
      } else if (count === 3) {
        total += openEnds === 2 ? 1000 : (openEnds === 1 ? 100 : 0);
      } else if (count === 2) {
        total += openEnds === 2 ? 100 : (openEnds === 1 ? 10 : 0);
      } else if (count === 1) {
        total += openEnds === 2 ? 10 : (openEnds === 1 ? 1 : 0);
      }
    }

    return total;
  };

  // ================================================================
  //  全局棋盘评估（对手得分 - 我方得分）
  // ================================================================
  G.evaluateBoard = function (board, player) {
    const opponent = player === G.BLACK ? G.WHITE : G.BLACK;
    let score = 0;

    const candidates = G.getCandidateMoves(board);

    for (const { row, col } of candidates) {
      // 我方在此位置落子的进攻得分
      const myAttack = G.evaluatePosition(board, row, col, player);
      // 对手在此位置落子的防守价值（即对手的进攻得分）
      const oppAttack = G.evaluatePosition(board, row, col, opponent);

      score += myAttack * 0.15 - oppAttack * 0.15;
    }

    return score;
  };

  // ================================================================
  //  单点形状检测
  //  返回在该位置落子后，四个方向的 pattern 分类列表
  // ================================================================
  function classifyCell(board, row, col, player) {
    const patterns = [];

    for (const [dr, dc] of G.DIRECTIONS) {
      let count = 1;    // 包含当前位置
      let openEnds = 0;

      // 正方向
      let r = row + dr, c = col + dc;
      while (r >= 0 && r < N && c >= 0 && c < N && board[r][c] === player) {
        count++; r += dr; c += dc;
      }
      if (r >= 0 && r < N && c >= 0 && c < N && board[r][c] === G.EMPTY) openEnds++;

      // 反方向
      r = row - dr; c = col - dc;
      while (r >= 0 && r < N && c >= 0 && c < N && board[r][c] === player) {
        count++; r -= dr; c -= dc;
      }
      if (r >= 0 && r < N && c >= 0 && c < N && board[r][c] === G.EMPTY) openEnds++;

      patterns.push({ count, openEnds });
    }

    return patterns;
  }

  // ================================================================
  //  检测杀招类型
  //  返回 { type, score } 或 null
  // ================================================================
  function detectKiller(patterns) {
    let hasLiveFour  = false;
    let hasRushFour  = false;
    let hasLiveThree = false;
    let hasSleepThree = false;
    let hasFive      = false;

    for (const p of patterns) {
      if (p.count >= 5) { hasFive = true; continue; }
      if (p.count === 4 && p.openEnds === 2) hasLiveFour = true;
      if (p.count === 4 && p.openEnds === 1) hasRushFour = true;
      if (p.count === 3 && p.openEnds === 2) hasLiveThree = true;
      if (p.count === 3 && p.openEnds === 1) hasSleepThree = true;
    }

    if (hasFive)                                                return { type: 'five', score: 100000 };
    if (hasLiveFour && hasLiveThree)                            return { type: 'four-three', score: 50000 };
    if (hasLiveFour && hasRushFour)                             return { type: 'four-four', score: 50000 };
    if ((hasLiveFour || hasRushFour) && hasLiveThree)           return { type: 'four-three', score: 50000 };
    if (hasRushFour && hasSleepThree && !hasLiveFour)           return { type: 'four-three (weak)', score: 30000 };
    if (hasLiveThree && countPattern(patterns, 3, 2) >= 2)     return { type: 'double-three', score: 10000 };
    if (hasLiveThree && hasSleepThree)                          return { type: 'three-sleep3', score: 3000 };
    if (hasLiveFour)                                            return { type: 'live-four', score: 10000 };
    if (hasLiveThree)                                           return { type: 'live-three', score: 1000 };

    return null;
  }

  function countPattern(patterns, targetCount, targetOpenEnds) {
    return patterns.filter(p => p.count === targetCount && p.openEnds === targetOpenEnds).length;
  }

  // ================================================================
  //  查找杀招落子
  //  扫描所有空位，返回对手或我方的杀招信息
  //  返回 { attack: bestMove, defense: mustBlock } 或 null
  // ================================================================
  G.findKillerMove = function (board, player) {
    const opponent = player === G.BLACK ? G.WHITE : G.BLACK;
    const candidates = G.getCandidateMoves(board);

    let bestAttack = null;
    let bestDefense = null;
    let bestAttackScore = -1;
    let bestDefenseScore = -1;

    for (const { row, col } of candidates) {
      // 检查我方在此落子后的杀招
      const myPatterns = classifyCell(board, row, col, player);
      const myKiller = detectKiller(myPatterns);
      if (myKiller && myKiller.score > bestAttackScore) {
        bestAttackScore = myKiller.score;
        bestAttack = { row, col, type: myKiller.type, score: myKiller.score };
      }

      // 检查对手在此落子后的杀招（需要防守）
      const oppPatterns = classifyCell(board, row, col, opponent);
      const oppKiller = detectKiller(oppPatterns);
      if (oppKiller && oppKiller.score > bestDefenseScore) {
        bestDefenseScore = oppKiller.score;
        bestDefense = { row, col, type: oppKiller.type, score: oppKiller.score };
      }
    }

    return {
      attack: bestAttack,
      defense: bestDefense
    };
  };

})(window.Gomoku);
```

- [ ] **Step 2: 提交**

```bash
git -C "C:/Users/wumu2/OneDrive/桌面/claude" add gomoku/js/evaluate.js
git -C "C:/Users/wumu2/OneDrive/桌面/claude" commit -m "feat: 添加 evaluate.js 启发式评估模块（含形状检测）"
```

---

### Task 4: 创建 mcts.js

**Files:**
- Create: `gomoku/js/mcts.js`

- [ ] **Step 1: 编写 MCTS 核心模块（含正确的回溯逻辑）**

```js
// gomoku/js/mcts.js
// 蒙特卡洛树搜索核心

window.Gomoku = window.Gomoku || {};

(function (G) {
  const N = G.BOARD_SIZE;

  // ================================================================
  //  MCTS 节点
  // ================================================================
  class MCTSNode {
    /**
     * @param {{ row: number, col: number } | null} move  到达此节点的落子
     * @param {MCTSNode | null} parent  父节点
     * @param {number} player  下这步棋的玩家 (G.BLACK / G.WHITE)
     */
    constructor(move, parent, player) {
      this.move   = move;
      this.parent = parent;
      this.player = player;     // 谁下了这步棋
      this.wins   = 0;
      this.visits = 0;
      this.children = [];
      this.untriedMoves = null;
    }

    getUCB1(C) {
      if (this.visits === 0) return Infinity;
      return (this.wins / this.visits) +
        C * Math.sqrt(Math.log(this.parent.visits) / this.visits);
    }
  }

  G.MCTSNode = MCTSNode;

  // ================================================================
  //  MCTS 搜索
  // ================================================================
  G.mctsSearch = function (board, currentPlayer, difficulty) {
    const config = G.DIFFICULTY[difficulty] || G.DIFFICULTY.medium;
    const C = config.C;

    if (G.isBoardEmpty(board)) {
      return { row: 7, col: 7 };
    }

    const root = new MCTSNode(null, null, null);
    const startTime = performance.now();

    while ((performance.now() - startTime) < config.timeLimit) {
      const simBoard = G.cloneBoard(board);
      let simPlayer = currentPlayer;

      // ===== 1. Selection =====
      let node = root;
      while (node.untriedMoves === null || node.untriedMoves.length === 0) {
        if (node.children.length === 0) break;

        let bestChild = node.children[0];
        let bestUCB = bestChild.getUCB1(C);
        for (let i = 1; i < node.children.length; i++) {
          const ucb = node.children[i].getUCB1(C);
          if (ucb > bestUCB) { bestUCB = ucb; bestChild = node.children[i]; }
        }

        node = bestChild;
        G.applyMove(simBoard, node.move.row, node.move.col, simPlayer);
        simPlayer = simPlayer === G.BLACK ? G.WHITE : G.BLACK;
      }

      // ===== 2. Expansion =====
      if (node.untriedMoves === null) {
        node.untriedMoves = G.getCandidateMoves(simBoard);
      }

      if (node.untriedMoves.length > 0) {
        const idx = Math.floor(Math.random() * node.untriedMoves.length);
        const move = node.untriedMoves[idx];
        node.untriedMoves.splice(idx, 1);

        // 记录谁下了这步棋
        const movePlayer = simPlayer;

        G.applyMove(simBoard, move.row, move.col, simPlayer);
        simPlayer = simPlayer === G.BLACK ? G.WHITE : G.BLACK;

        const child = new MCTSNode(move, node, movePlayer);
        node.children.push(child);
        node = child;
      }

      // ===== 3. Simulation =====
      const result = simulate(simBoard, simPlayer);

      // ===== 4. Backpropagation =====
      let backNode = node;
      while (backNode !== null) {
        backNode.visits++;
        if (backNode.player !== null && backNode.player !== undefined) {
          if (result === backNode.player) {
            backNode.wins++;
          } else if (result === 0) {
            backNode.wins += 0.5; // 平局
          }
        }
        backNode = backNode.parent;
      }
    }

    // ---- 返回访问次数最多的子节点 ----
    if (root.children.length === 0) {
      const fallback = G.getCandidateMoves(board);
      return fallback[Math.floor(Math.random() * fallback.length)];
    }

    let bestChild = root.children[0];
    for (let i = 1; i < root.children.length; i++) {
      if (root.children[i].visits > bestChild.visits) {
        bestChild = root.children[i];
      }
    }
    return bestChild.move;
  };

  // ================================================================
  //  启发式引导模拟（Heavy Playout）
  //  返回胜者：G.BLACK / G.WHITE / 0
  // ================================================================
  function simulate(board, currentPlayer) {
    const simBoard = G.cloneBoard(board);
    let player = currentPlayer;
    let moveCount = 0;
    const maxMoves = N * N;

    while (moveCount < maxMoves) {
      const candidates = G.getCandidateMoves(simBoard);
      if (candidates.length === 0) return 0;

      let selected;

      // 候选很少 → 先检查直接获胜，再随机
      if (candidates.length <= 5) {
        for (const m of candidates) {
          G.applyMove(simBoard, m.row, m.col, player);
          const wc = G.checkWin(simBoard, m.row, m.col, player);
          G.undoMove(simBoard, m.row, m.col);
          if (wc.won) {
            G.applyMove(simBoard, m.row, m.col, player);
            return player;
          }
        }
        selected = candidates[Math.floor(Math.random() * candidates.length)];
      } else {
        // 启发式加权
        const scored = candidates.map(m => ({
          row: m.row, col: m.col,
          score: G.evaluatePosition(simBoard, m.row, m.col, player)
        }));
        scored.sort((a, b) => b.score - a.score);

        if (Math.random() < 0.6 && scored[0].score > 0) {
          const topN = Math.min(5, scored.length);
          const pool = scored.slice(0, topN);
          const totalWeight = pool.reduce((s, m) => s + Math.max(m.score, 1), 0);
          let r = Math.random() * totalWeight;
          selected = pool[pool.length - 1];
          for (const m of pool) {
            r -= Math.max(m.score, 1);
            if (r <= 0) { selected = m; break; }
          }
        } else {
          selected = scored[Math.floor(Math.random() * scored.length)];
        }
      }

      G.applyMove(simBoard, selected.row, selected.col, player);

      const wc = G.checkWin(simBoard, selected.row, selected.col, player);
      if (wc.won) return player;

      player = player === G.BLACK ? G.WHITE : G.BLACK;
      moveCount++;
    }

    return 0;
  }

})(window.Gomoku);
```

- [ ] **Step 2: 提交**

```bash
git -C "C:/Users/wumu2/OneDrive/桌面/claude" add gomoku/js/mcts.js
git -C "C:/Users/wumu2/OneDrive/桌面/claude" commit -m "feat: 添加 mcts.js MCTS 搜索核心模块"
```

---

### Task 5: 创建 ai.js

**Files:**
- Create: `gomoku/js/ai.js`

- [ ] **Step 1: 编写 AI 入口模块**

```js
// gomoku/js/ai.js
// AI 入口：杀招优先检测 + MCTS 调度

window.Gomoku = window.Gomoku || {};

(function (G) {

  /**
   * 查找 AI 的最佳落子
   * @param {number[][]} board  当前棋盘
   * @param {string} difficulty  'easy' | 'medium' | 'hard'
   * @returns {{ row: number, col: number }}
   */
  G.findBestMove = function (board, difficulty) {
    const aiPlayer = G.WHITE;

    // ---- 1. 杀招检测 ----
    const killer = G.findKillerMove(board, aiPlayer);

    // 我能直接赢 → 立即落子
    if (killer.attack && killer.attack.score >= 100000) {
      return { row: killer.attack.row, col: killer.attack.col };
    }

    // 对手即将赢（形成四三、双四等）→ 必须堵
    if (killer.defense && killer.defense.score >= 50000) {
      return { row: killer.defense.row, col: killer.defense.col };
    }

    // 困难模式：对手活四 → 必须堵
    if (difficulty === 'hard' && killer.defense && killer.defense.score >= 10000) {
      return { row: killer.defense.row, col: killer.defense.col };
    }

    // 我能打出杀招（四三、双四等）→ 优先走
    if (killer.attack && killer.attack.score >= 50000) {
      return { row: killer.attack.row, col: killer.attack.col };
    }

    // ---- 2. MCTS 搜索 ----
    return G.mctsSearch(board, aiPlayer, difficulty);
  };

})(window.Gomoku);
```

- [ ] **Step 2: 提交**

```bash
git -C "C:/Users/wumu2/OneDrive/桌面/claude" add gomoku/js/ai.js
git -C "C:/Users/wumu2/OneDrive/桌面/claude" commit -m "feat: 添加 ai.js AI 入口模块"
```

---

### Task 6: 修改 index.html

**Files:**
- Modify: `gomoku/index.html`

- [ ] **Step 1: 在 `</head>` 前添加 JS 模块引用，替换原有 JS 逻辑**

改动涉及两部分：A) 在 `<head>` 中添加脚本引用；B) 精简 `<script>` 中的游戏逻辑，移除已提取到模块的代码，更新 `aiMove` 调用新的 AI。

**Part A: 在 `</head>` 前添加脚本引用**（在 `</style>` 之后，`</head>` 之前）

将：
```html
    </style>
</head>
```

替换为：
```html
    </style>

    <!-- JS 模块（按依赖顺序加载） -->
    <script src="js/constants.js"></script>
    <script src="js/board.js"></script>
    <script src="js/evaluate.js"></script>
    <script src="js/mcts.js"></script>
    <script src="js/ai.js"></script>
</head>
```

**Part B: 精简内联 `<script>` 中的游戏逻辑**

具体修改步骤：

- [ ] **Step 2: 删除常量声明块（行 373-388 的前半部分）**

删除以下代码块（常量已移至 constants.js）：
```js
    // ============================================================
    //  常量
    // ============================================================
    const BOARD_SIZE  = 15;
    const CELL_SIZE   = 36;
    const PADDING     = 30;
    const STONE_R     = 15;
    const AI_PLAYER   = 2;   // AI 执白
    const HUMAN_PLAYER = 1;  // 玩家执黑

    const canvas  = document.getElementById('board');
    const ctx     = canvas.getContext('2d');
    const CANVAS_SIZE = PADDING * 2 + (BOARD_SIZE - 1) * CELL_SIZE;
    canvas.width  = CANVAS_SIZE;
    canvas.height = CANVAS_SIZE;
```

替换为（保留 canvas/ctx 初始化，改用 Gomoku 命名空间常量）：
```js
    // ---- 通过 Gomoku 命名空间获取常量 ----
    const BOARD_SIZE  = Gomoku.BOARD_SIZE;
    const CELL_SIZE   = Gomoku.CELL_SIZE;
    const PADDING     = Gomoku.PADDING;
    const STONE_R     = Gomoku.STONE_R;

    const canvas  = document.getElementById('board');
    const ctx     = canvas.getContext('2d');
    const CANVAS_SIZE = PADDING * 2 + (BOARD_SIZE - 1) * CELL_SIZE;
    canvas.width  = CANVAS_SIZE;
    canvas.height = CANVAS_SIZE;
```

- [ ] **Step 3: 删除 MOVE_TIME 常量（行 408）**

删除：
```js
    const MOVE_TIME = 30;
```

替换为：
```js
    const MOVE_TIME = Gomoku.MOVE_TIME;
```

- [ ] **Step 4: 替换 `init()` 函数中的棋盘创建（行 427）**

将：
```js
        board = Array.from({ length: BOARD_SIZE }, () => Array(BOARD_SIZE).fill(0));
```

替换为：
```js
        board = Gomoku.createBoard();
```

- [ ] **Step 5: 简化 `placeStone()` 函数中的胜负判定（行 478-515）**

删除整个胜负判定循环块（从 `// ---- 胜负判定 ----` 到 `if (won) {` 之前的 `}`），替换为调用 `Gomoku.checkWin`：

将：
```js
        // ---- 胜负判定 ----
        const dirs = [[1,0],[0,1],[1,1],[1,-1]];
        let won = false;

        for (const [dr, dc] of dirs) {
            let count = 1;
            for (let i = 1; i < 5; i++) {
                const r = row + dr * i, c = col + dc * i;
                if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) break;
                if (board[r][c] !== player) break;
                count++;
            }
            for (let i = 1; i < 5; i++) {
                const r = row - dr * i, c = col - dc * i;
                if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) break;
                if (board[r][c] !== player) break;
                count++;
            }
            if (count >= 5) {
                won = true;
                // 收集获胜棋子...
                winLine = [];
                let sr = row, sc = col;
                while (true) {
                    const nr = sr - dr, nc = sc - dc;
                    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
                    if (board[nr][nc] !== player) break;
                    sr = nr; sc = nc;
                }
                for (let i = 0; i < 5; i++) {
                    const nr = sr + dr * i, nc = sc + dc * i;
                    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
                    if (board[nr][nc] !== player) break;
                    winLine.push({ row: nr, col: nc });
                }
                break;
            }
        }
```

替换为：
```js
        // ---- 胜负判定 ----
        const winResult = Gomoku.checkWin(board, row, col, player);
        if (winResult.won) {
            winLine = winResult.winLine;
        }
```

后续的 `if (won)` 改为 `if (winResult.won)`：
```js
        if (winResult.won) {
            gameOver = true;
            stopTimer();
            render();
            updateUI();
            return true;
        }
```

- [ ] **Step 6: 删除整个 `evaluatePosition` 函数（行 573-618）**

删除从 `//  AI — 位置评分` 到 `return total; }` 的整个函数。

- [ ] **Step 7: 删除整个 `findBestMove` 函数（行 620-674）**

删除从 `//  AI — 搜索最佳落子` 到 `//  AI 走棋` 之前的整个函数。

- [ ] **Step 8: 更新 `aiMove()` 函数（行 679-700）**

将：
```js
    function aiMove() {
        if (gameOver || aiThinking || current !== AI_PLAYER || mode !== 'pve') return;

        const HUMAN_PLAYER = Gomoku.BLACK;

        aiThinking = true;
        canvas.classList.add('waiting');
        updateUI();

        setTimeout(() => {
            const move = findBestMove();
            if (move) {
                placeStone(move.row, move.col);
            }
            aiThinking = false;
            canvas.classList.remove('waiting');
            updateUI();

            if (!gameOver && current === HUMAN_PLAYER) {
                undoBtn.disabled = history.length === 0;
            }
        }, 280);
    }
```

替换为：
```js
    function aiMove() {
        if (gameOver || aiThinking || current !== Gomoku.WHITE || mode !== 'pve') return;

        aiThinking = true;
        canvas.classList.add('waiting');
        updateUI();

        setTimeout(() => {
            const move = Gomoku.findBestMove(board, difficulty);
            if (move) {
                placeStone(move.row, move.col);
            }
            aiThinking = false;
            canvas.classList.remove('waiting');
            updateUI();

            if (!gameOver && current === Gomoku.BLACK) {
                undoBtn.disabled = history.length === 0;
            }
        }, 50); // 减小延迟，MCTS 内部已有时间控制
    }
```

- [ ] **Step 9: 更新 `init()` 中其他引用常量的位置**

确保 `init()` 和其他函数中不再使用已删除的 `AI_PLAYER`、`HUMAN_PLAYER` 常量。需要在这些位置替换：

- `init()` 中 `current = 1` 保持不变（1 就是黑棋）
- 所有引用 `AI_PLAYER` 的地方（如 `mode === 'pve' && current === AI_PLAYER`）→ 改为 `mode === 'pve' && current === Gomoku.WHITE`
- 所有引用 `HUMAN_PLAYER` 的地方 → 改为 `Gomoku.BLACK`

具体替换：

**在 `undo()` 函数中：** 不需要改动（已经用 `current` 和 `history` 中的 `player`）

**在事件绑定中（画布点击、触摸）：**
将所有的：
```js
if (mode === 'pve' && current === AI_PLAYER) return;
```
替换为：
```js
if (mode === 'pve' && current === Gomoku.WHITE) return;
```

这出现在 3 处：
1. `canvas.addEventListener('click', ...)` 中 (~行952)
2. `canvas.addEventListener('mousemove', ...)` 中 (~行965)
3. `canvas.addEventListener('touchstart', ...)` 中 (~行987)

**在 `aiMove()` 调用触发处：**
将：
```js
if (mode === 'pve' && !gameOver && current === AI_PLAYER) {
    aiMove();
}
```
替换为：
```js
if (mode === 'pve' && !gameOver && current === Gomoku.WHITE) {
    aiMove();
}
```

这出现在 2 处：
1. canvas click 事件中 (~行957)
2. touchstart 事件中 (~行989)

**在 `updateUI()` 中：**
将：
```js
if (mode === 'pve' && current === AI_PLAYER) {
```
替换为：
```js
if (mode === 'pve' && current === Gomoku.WHITE) {
```
（出现在 ~行898）

**在 `resetTimer()` 中：**
将：
```js
if (mode === 'pve' && current === AI_PLAYER) {
```
替换为：
```js
if (mode === 'pve' && current === Gomoku.WHITE) {
```
（出现在 ~行731）

- [ ] **Step 10: 提交**

```bash
git -C "C:/Users/wumu2/OneDrive/桌面/claude" add gomoku/index.html
git -C "C:/Users/wumu2/OneDrive/桌面/claude" commit -m "refactor: index.html 引用 JS 模块，精简内联代码"
```

---

### Task 7: 验证与测试

**Files:** 无新建

- [ ] **Step 1: 启动本地服务器并打开浏览器测试**

```bash
cd "C:/Users/wumu2/OneDrive/桌面/claude/gomoku"
# 用任意 HTTP 服务器打开，例如：
# python -m http.server 8080
# 或用浏览器直接打开 index.html（file:// 协议下 script src 可能因 CORS 被阻）
```

由于 `<script src="js/...">` 在 `file://` 协议下可能被浏览器阻止，需要使用本地 HTTP 服务器：
```bash
cd "C:/Users/wumu2/OneDrive/桌面/claude/gomoku" && python -m http.server 8080
```
然后访问 `http://localhost:8080`

- [ ] **Step 2: 手动测试清单**
  1. 打开页面，确认棋盘正常渲染，木质纹理显示
  2. 双人模式（PVP）：点击落子，胜负判定正常，悔棋正常
  3. 人机模式（PVE）：
     - 简单难度：AI 在 ~0.3 秒内落子，走法有变化
     - 中等难度：AI 在 ~1.5 秒内落子
     - 困难难度：AI 在 ~4 秒内落子，棋力明显强于简单
  4. 切换到人机模式时难度选择可见，切换到双人模式时隐藏
  5. 键盘快捷键（R 重开、Ctrl+Z 悔棋）正常
  6. 计时器正常工作
  7. 触屏操作正常
  8. 响应式布局正常（手机/平板/桌面）
  9. AI 不会在明显能赢时错过杀招
  10. AI 不会忽略对手的明显威胁（如活四）

- [ ] **Step 3: 提交最终验证结果**

```bash
git -C "C:/Users/wumu2/OneDrive/桌面/claude" status
```

---

## 附录：index.html 完整内联 `<script>` 修改对照

以下是 index.html 中需要改动的完整内联脚本代码（替换原有的 `<script>...</script>` 全部内容）：

```js
<script>
    // ============================================================
    //  从 Gomoku 命名空间获取常量
    // ============================================================
    const BOARD_SIZE  = Gomoku.BOARD_SIZE;
    const CELL_SIZE   = Gomoku.CELL_SIZE;
    const PADDING     = Gomoku.PADDING;
    const STONE_R     = Gomoku.STONE_R;
    const MOVE_TIME   = Gomoku.MOVE_TIME;

    const canvas  = document.getElementById('board');
    const ctx     = canvas.getContext('2d');
    const CANVAS_SIZE = PADDING * 2 + (BOARD_SIZE - 1) * CELL_SIZE;
    canvas.width  = CANVAS_SIZE;
    canvas.height = CANVAS_SIZE;

    // ============================================================
    //  状态
    // ============================================================
    let board      = [];
    let current    = 1;
    let gameOver   = false;
    let moveno     = 0;
    let lastMove   = null;
    let winLine    = [];
    let history    = [];
    let hoverPos   = null;

    let mode       = 'pvp';
    let difficulty = 'easy';
    let aiThinking = false;

    let timeLeft      = MOVE_TIME;
    let timerID       = null;
    let timeoutPlayer = 0;

    const turnStone   = document.getElementById('turnStone');
    const turnLabel   = document.getElementById('turnLabel');
    const statusText  = document.getElementById('statusText');
    const moveCounter = document.getElementById('moveCounter');
    const undoBtn     = document.getElementById('undoBtn');
    const resetBtn    = document.getElementById('resetBtn');
    const modeHint    = document.getElementById('modeHint');
    const diffRow     = document.getElementById('diffRow');

    // ============================================================
    //  初始化
    // ============================================================
    function init() {
        board = Gomoku.createBoard();
        current    = 1;
        gameOver   = false;
        moveno     = 0;
        lastMove   = null;
        winLine    = [];
        history    = [];
        hoverPos   = null;
        aiThinking = false;
        timeoutPlayer = 0;
        timeLeft = MOVE_TIME;
        updateTimerDisplay();
        stopTimer();
        render();
        updateUI();
        undoBtn.disabled = true;
        canvas.classList.remove('waiting');
    }

    // ============================================================
    //  坐标转换
    // ============================================================
    function getBoardPos(e) {
        const rect = canvas.getBoundingClientRect();
        const sx = canvas.width  / rect.width;
        const sy = canvas.height / rect.height;
        const x = (e.clientX - rect.left) * sx;
        const y = (e.clientY - rect.top)  * sy;

        const col = Math.round((x - PADDING) / CELL_SIZE);
        const row = Math.round((y - PADDING) / CELL_SIZE);

        if (row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE) {
            return { row, col };
        }
        return null;
    }

    // ============================================================
    //  落子
    // ============================================================
    function placeStone(row, col) {
        if (gameOver)               return false;
        if (board[row][col] !== 0)  return false;

        const player = current;
        board[row][col] = player;
        moveno++;
        lastMove = { row, col };
        history.push({ row, col, player });

        const winResult = Gomoku.checkWin(board, row, col, player);
        if (winResult.won) {
            winLine = winResult.winLine;
        }

        if (winResult.won) {
            gameOver = true;
            stopTimer();
            render();
            updateUI();
            return true;
        }

        if (moveno >= BOARD_SIZE * BOARD_SIZE) {
            gameOver = true;
            stopTimer();
            render();
            updateUI();
            return true;
        }

        current = current === 1 ? 2 : 1;
        resetTimer();
        render();
        updateUI();
        undoBtn.disabled = false;
        return true;
    }

    // ============================================================
    //  悔棋
    // ============================================================
    function undo() {
        if (gameOver || aiThinking) return;
        if (history.length === 0)   return;

        if (mode === 'pve') {
            const steps = Math.min(history.length, 2);
            for (let i = 0; i < steps; i++) {
                const move = history.pop();
                board[move.row][move.col] = 0;
                moveno--;
                current = move.player;
            }
        } else {
            const move = history.pop();
            board[move.row][move.col] = 0;
            moveno--;
            current = move.player;
        }

        lastMove = history.length > 0 ? history[history.length - 1] : null;
        winLine  = [];
        resetTimer();
        render();
        updateUI();
        undoBtn.disabled = history.length === 0;
    }

    // ============================================================
    //  AI 走棋
    // ============================================================
    function aiMove() {
        if (gameOver || aiThinking || current !== Gomoku.WHITE || mode !== 'pve') return;

        aiThinking = true;
        canvas.classList.add('waiting');
        updateUI();

        setTimeout(() => {
            const move = Gomoku.findBestMove(board, difficulty);
            if (move) {
                placeStone(move.row, move.col);
            }
            aiThinking = false;
            canvas.classList.remove('waiting');
            updateUI();

            if (!gameOver && current === Gomoku.BLACK) {
                undoBtn.disabled = history.length === 0;
            }
        }, 50);
    }

    // ============================================================
    //  计时器
    // ============================================================
    function startTimer() {
        stopTimer();
        timeLeft = MOVE_TIME;
        updateTimerDisplay();

        timerID = setInterval(() => {
            timeLeft--;
            updateTimerDisplay();
            if (timeLeft <= 0) {
                stopTimer();
                handleTimeout();
            }
        }, 1000);
    }

    function stopTimer() {
        if (timerID !== null) {
            clearInterval(timerID);
            timerID = null;
        }
    }

    function resetTimer() {
        stopTimer();
        if (gameOver) return;
        if (mode === 'pve' && current === Gomoku.WHITE) {
            updateTimerDisplay();
            return;
        }
        startTimer();
    }

    function updateTimerDisplay() {
        const el = document.getElementById('timerDisplay');
        if (!el) return;
        el.textContent = '⏱ ' + timeLeft;
        el.className = 'timer';
        if (timeLeft <= 5) el.classList.add('danger');
        else if (timeLeft <= 10) el.classList.add('warning');
    }

    function handleTimeout() {
        if (gameOver) return;
        timeoutPlayer = current;
        gameOver = true;
        winLine = [];
        stopTimer();
        render();
        updateUI();
        undoBtn.disabled = true;
    }

    // ============================================================
    //  渲染（保持不变）
    // ============================================================
    function render() {
        ctx.clearRect(0, 0, CANVAS_SIZE, CANVAS_SIZE);

        ctx.fillStyle = '#d4a86a';
        ctx.fillRect(0, 0, CANVAS_SIZE, CANVAS_SIZE);

        ctx.strokeStyle = 'rgba(139, 90, 43, 0.08)';
        ctx.lineWidth = 1;
        for (let y = 0; y < CANVAS_SIZE; y += 4) {
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(CANVAS_SIZE, y);
            ctx.stroke();
        }

        ctx.strokeStyle = '#5a3e28';
        ctx.lineWidth = 1;
        for (let i = 0; i < BOARD_SIZE; i++) {
            const p = PADDING + i * CELL_SIZE;
            ctx.beginPath(); ctx.moveTo(p, PADDING); ctx.lineTo(p, PADDING + (BOARD_SIZE - 1) * CELL_SIZE); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(PADDING, p); ctx.lineTo(PADDING + (BOARD_SIZE - 1) * CELL_SIZE, p); ctx.stroke();
        }

        ctx.fillStyle = '#5a3e28';
        for (const [r, c] of [[3,3],[3,11],[7,7],[11,3],[11,11]]) {
            const x = PADDING + c * CELL_SIZE, y = PADDING + r * CELL_SIZE;
            ctx.beginPath(); ctx.arc(x, y, 3.5, 0, Math.PI * 2); ctx.fill();
        }

        for (let r = 0; r < BOARD_SIZE; r++)
            for (let c = 0; c < BOARD_SIZE; c++)
                if (board[r][c] !== 0) drawStone(r, c, board[r][c], false);

        if (lastMove && winLine.length === 0) {
            const x = PADDING + lastMove.col * CELL_SIZE;
            const y = PADDING + lastMove.row * CELL_SIZE;
            ctx.fillStyle = board[lastMove.row][lastMove.col] === 1 ? '#ff6b6b' : '#e74c3c';
            ctx.beginPath(); ctx.arc(x, y, 4, 0, Math.PI * 2); ctx.fill();
        }

        if (winLine.length >= 5) {
            for (const { row, col } of winLine) {
                const x = PADDING + col * CELL_SIZE, y = PADDING + row * CELL_SIZE;
                ctx.shadowColor = '#ffd700'; ctx.shadowBlur = 16;
                ctx.strokeStyle = '#ffd700'; ctx.lineWidth = 3;
                ctx.beginPath(); ctx.arc(x, y, STONE_R + 3, 0, Math.PI * 2); ctx.stroke();
                ctx.shadowBlur = 0;
            }
            for (const { row, col } of winLine) {
                const x = PADDING + col * CELL_SIZE, y = PADDING + row * CELL_SIZE;
                ctx.fillStyle = '#ffd700';
                ctx.beginPath(); ctx.arc(x, y, 3, 0, Math.PI * 2); ctx.fill();
            }
        }

        if (hoverPos && !gameOver && !aiThinking && board[hoverPos.row][hoverPos.col] === 0) {
            ctx.globalAlpha = 0.45;
            drawStone(hoverPos.row, hoverPos.col, current, true);
            ctx.globalAlpha = 1;
        }
    }

    function drawStone(row, col, player, isHover) {
        const x = PADDING + col * CELL_SIZE, y = PADDING + row * CELL_SIZE;
        const r = STONE_R + (isHover ? 2 : 0);

        if (!isHover) {
            ctx.shadowColor = 'rgba(0,0,0,0.35)';
            ctx.shadowBlur = 6; ctx.shadowOffsetX = 2; ctx.shadowOffsetY = 2;
        }

        const grad = ctx.createRadialGradient(x - 6, y - 6, 2, x, y, r);
        if (player === 1) {
            grad.addColorStop(0, isHover ? '#888' : '#666');
            grad.addColorStop(0.6, '#222');
            grad.addColorStop(1, '#000');
        } else {
            grad.addColorStop(0, '#fff');
            grad.addColorStop(0.6, '#eee');
            grad.addColorStop(1, '#bbb');
        }

        ctx.beginPath(); ctx.arc(x, y, r, 0, Math.PI * 2);
        ctx.fillStyle = grad; ctx.fill();

        if (!isHover) {
            ctx.shadowBlur = 0;
            ctx.strokeStyle = player === 1 ? '#000' : '#999';
            ctx.lineWidth = 0.6; ctx.stroke();

            const hl = ctx.createRadialGradient(x - 6, y - 6, 0.5, x - 6, y - 6, 6);
            hl.addColorStop(0, 'rgba(255,255,255,0.3)');
            hl.addColorStop(1, 'rgba(255,255,255,0)');
            ctx.beginPath(); ctx.arc(x, y, r, 0, Math.PI * 2);
            ctx.fillStyle = hl; ctx.fill();
        }

        ctx.shadowBlur = 0; ctx.shadowOffsetX = 0; ctx.shadowOffsetY = 0;
    }

    // ============================================================
    //  UI 更新（保持不变，仅将 AI_PLAYER 替换为 Gomoku.WHITE）
    // ============================================================
    function updateUI() {
        turnStone.className = 'turn-stone ' + (current === 1 ? 'black' : 'white');

        if (gameOver) {
            if (timeoutPlayer) {
                const name = timeoutPlayer === 1 ? '黑棋' : '白棋';
                turnLabel.textContent = '⏰ ' + name + '超时！';
                statusText.textContent = '⚡ 超时判负';
                statusText.className = 'status-text win';
            } else if (winLine.length >= 5) {
                const name = board[winLine[0].row][winLine[0].col] === 1 ? '黑棋' : '白棋';
                turnLabel.textContent = '🏆 ' + name + '获胜！';
                statusText.textContent = '🎉 五子连珠！';
                statusText.className = 'status-text win';
            } else {
                turnLabel.textContent = '平局';
                statusText.textContent = '🤝 棋盘已满';
                statusText.className = 'status-text draw';
            }
            undoBtn.disabled = true;
        } else if (aiThinking) {
            turnLabel.textContent = '🤔 AI 思考中…';
            statusText.textContent = '⏳ 请稍候';
            statusText.className = 'status-text thinking';
            undoBtn.disabled = true;
        } else {
            if (mode === 'pve' && current === Gomoku.WHITE) {
                turnLabel.textContent = '🤖 AI 思考中…';
                statusText.textContent = '';
                statusText.className = 'status-text';
            } else {
                turnLabel.textContent = current === 1 ? '⚫ 黑棋落子' : '⚪ 白棋落子';
                statusText.textContent = '';
                statusText.className = 'status-text';
            }
            undoBtn.disabled = history.length === 0;
        }

        moveCounter.textContent = '第 ' + moveno + ' 手';
    }

    // ============================================================
    //  模式 / 难度切换（保持不变）
    // ============================================================
    function setMode(newMode) {
        if (newMode === mode) return;
        mode = newMode;

        document.querySelectorAll('#modeGroup .group-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.value === mode);
        });

        diffRow.classList.toggle('hidden', mode === 'pvp');
        modeHint.textContent = mode === 'pvp' ? '轮流落子' : ('🤖 ' + (difficulty === 'easy' ? '简单' : difficulty === 'medium' ? '中等' : '困难'));
        init();
    }

    function setDifficulty(newDiff) {
        if (newDiff === difficulty) return;
        difficulty = newDiff;

        document.querySelectorAll('#diffGroup .group-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.value === difficulty);
        });

        if (mode === 'pve') {
            modeHint.textContent = '🤖 ' + (difficulty === 'easy' ? '简单' : difficulty === 'medium' ? '中等' : '困难');
        }
        init();
    }

    // ============================================================
    //  事件绑定
    // ============================================================
    canvas.addEventListener('click', (e) => {
        if (aiThinking) return;
        const pos = getBoardPos(e);
        if (!pos) return;

        if (mode === 'pve' && current === Gomoku.WHITE) return;

        if (placeStone(pos.row, pos.col)) {
            if (mode === 'pve' && !gameOver && current === Gomoku.WHITE) {
                aiMove();
            }
        }
    });

    canvas.addEventListener('mousemove', (e) => {
        if (aiThinking) { hoverPos = null; render(); return; }
        const pos = getBoardPos(e);
        if (pos && !(mode === 'pve' && current === Gomoku.WHITE)) {
            hoverPos = pos;
        } else {
            hoverPos = null;
        }
        render();
    });

    canvas.addEventListener('mouseleave', () => {
        hoverPos = null;
        render();
    });

    canvas.addEventListener('touchstart', (e) => {
        e.preventDefault();
        if (aiThinking) return;
        const touch = e.touches[0];
        const fake = { clientX: touch.clientX, clientY: touch.clientY };
        const pos = getBoardPos(fake);
        if (!pos) return;
        if (mode === 'pve' && current === Gomoku.WHITE) return;
        if (placeStone(pos.row, pos.col)) {
            if (mode === 'pve' && !gameOver && current === Gomoku.WHITE) {
                aiMove();
            }
        }
    }, { passive: false });

    resetBtn.addEventListener('click', init);
    undoBtn.addEventListener('click', undo);

    document.querySelectorAll('#modeGroup .group-btn').forEach(btn => {
        btn.addEventListener('click', () => setMode(btn.dataset.value));
    });

    document.querySelectorAll('#diffGroup .group-btn').forEach(btn => {
        btn.addEventListener('click', () => setDifficulty(btn.dataset.value));
    });

    document.addEventListener('keydown', (e) => {
        if (e.key === 'r' || e.key === 'R') init();
        if (e.key === 'z' && (e.ctrlKey || e.metaKey)) { e.preventDefault(); undo(); }
    });

    init();
</script>
```

> **注意：** 附录中的代码是 Task 6 修改完成后的**目标状态**，用于参考。实际执行时按 Step 2-9 逐步修改。
