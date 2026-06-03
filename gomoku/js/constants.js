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
