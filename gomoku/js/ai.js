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
