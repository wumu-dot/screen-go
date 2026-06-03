# 五子棋 AI 优化 — MCTS + 启发式设计规格

**日期**: 2026-06-03
**目标**: 用 MCTS（蒙特卡洛树搜索）+ 启发式引导 + 形状检测提升 AI 棋力

---

## 一、文件结构

```
gomoku/
├── index.html          # 主入口（HTML + CSS + 游戏控制器 + 渲染）
├── js/
│   ├── board.js        # 棋盘状态、胜负判定、落子/悔棋、棋盘克隆
│   ├── evaluate.js     # 启发式评估：方向打分 + 形状检测
│   ├── mcts.js         # MCTS 核心：节点结构、四步循环（纯算法）
│   ├── ai.js           # AI 入口：MCTS 调度、难度控制、findBestMove()
│   └── constants.js    # 常量（棋盘大小、棋子值、AI 配置）
└── README.md
```

模块职责与依赖：

| 模块 | 职责 | 依赖 |
|------|------|------|
| `index.html` | Canvas 渲染、事件处理、UI 更新、全局状态 | `js/*.js` |
| `board.js` | 棋盘读写、落子校验、胜负判定、悔棋、棋盘克隆 | `constants.js` |
| `evaluate.js` | 局部方向打分、全局形状检测、综合评估 | `board.js` |
| `mcts.js` | MCTS 节点、选择/扩展/模拟/回溯 | `board.js`、`evaluate.js`、`constants.js` |
| `ai.js` | 组装 MCTS，控制模拟次数，暴露 `findBestMove()` | `mcts.js`、`constants.js` |
| `constants.js` | `BOARD_SIZE`、`CELL_SIZE`、`AI_PLAYER`、`HUMAN_PLAYER`、难度参数 | 无 |

## 二、MCTS 核心算法

### 节点结构

```
MCTSNode {
  move          // { row, col } 到达此节点的落子
  parent        // 父节点引用
  wins          // 己方获胜次数
  visits        // 访问总次数
  children      // 已展开的子节点列表
  untriedMoves  // 尚未展开的可落子位置列表
  state         // 棋盘快照：{ board, currentPlayer }
}
```

### UCB1 公式

```
UCB(child) = child.wins / child.visits + C × sqrt( ln(parent.visits) / child.visits )
```

- `C = 1.414`（√2，MCTS 经典值）
- 简单模式 `C = 1.8`（增加探索，走法更多样）
- 困难模式 `C = 1.2`（倾向利用已知好手）

### 四步循环

```
Selection    → 从根节点用 UCB1 一路选到叶节点
Expansion    → 从叶节点的 untriedMoves 随机取一个展开为新节点
Simulation   → 从新节点开始，启发式引导随机对局直到终局
Backpropagation → 胜/负/平结果沿路径反向更新所有祖先节点
```

### 模拟策略（Heavy Playout）

每步：对所有合法位置调用 `evaluateBoard()` 打分 → 分数归一化 → 加权随机选点：
- 排名前 5 的位置：60% 概率被选中
- 其余 40% 概率均匀随机

### 特殊处理

- 对手势力诱导检测：模拟中如果在防守方发现对手即将形成杀招（形状检测触发），优先堵关键位
- 空棋盘开局第一手：直接下天元（7, 7）
- 优先级硬检查（困难模式）：如果存在直接五连（攻击胜）或必须堵的四子，直接返回，不跑 MCTS

## 三、难度映射

| 难度 | 每步模拟轮次 | 探索系数 C |
|------|------------|-----------|
| 简单 | 1000 | 1.8 |
| 中等 | 5000 | 1.414 |
| 困难 | 20000 | 1.2 |

## 四、启发式评估增强

### 局部方向打分

复用现有 `evaluatePosition()` 逻辑：

| 连续数 | 两端皆空 | 一端空 | 两端堵死 |
|--------|---------|--------|---------|
| ≥5 | 100000 | 100000 | 100000 |
| 4 | 10000 | 1000 | 0 |
| 3 | 1000 | 100 | 0 |
| 2 | 100 | 10 | 0 |
| 1 | 10 | 1 | 0 |

### 全局形状检测

| 形状 | 说明 | 分值 |
|------|------|------|
| 四三 | 同时一个活四 + 一个活三 | 50000 |
| 双四 | 两个活四 | 50000 |
| 四四 | 一个活四 + 一个冲四 | 30000 |
| 双活三 | 两个活三 | 10000 |
| 活三 + 眠三 | | 3000 |

### evaluateBoard

综合分数 = 局部方向打分 + 形状检测加成。用于 MCTS 模拟引导。

## 五、状态迁移

现有全局状态（`board`, `current`, `gameOver`, `moveno`, `lastMove`, `winLine`, `history`, `hoverPos`）保留在 `index.html` 中。`board.js` 的函数通过参数接收棋盘状态，不做隐式全局依赖，方便 MCTS 模拟时传入克隆棋盘。

`aiMove()` 流程不变：用户落子后触发 → `findBestMove()` 内部改为调用 MCTS → 返回最优落子 → `placeStone()` 执行。

## 六、不允许的改动

- 不修改 CSS 样式和视觉效果
- 不修改计时器逻辑
- 不修改现有模式切换/难度切换 UI
- 不修改键盘快捷键
- 不修改触屏支持
