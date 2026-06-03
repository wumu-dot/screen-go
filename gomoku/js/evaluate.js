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
  //  全局棋盘评估（用于 MCTS 模拟的宏观判断）
  // ================================================================
  G.evaluateBoard = function (board, player) {
    const opponent = player === G.BLACK ? G.WHITE : G.BLACK;
    let score = 0;

    const candidates = G.getCandidateMoves(board);

    for (const { row, col } of candidates) {
      score += G.evaluatePosition(board, row, col, player) * 0.15;
      score -= G.evaluatePosition(board, row, col, opponent) * 0.15;
    }

    return score;
  };

  // ================================================================
  //  单点形状分类
  //  模拟在 (row, col) 放置 player 棋子后，四个方向的 pattern
  // ================================================================
  function classifyCell(board, row, col, player) {
    const patterns = [];

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

      patterns.push({ count, openEnds });
    }

    return patterns;
  }

  // ================================================================
  //  检测杀招类型
  //  返回 { type, score } 或 null
  // ================================================================
  function detectKiller(patterns) {
    let hasLiveFour   = false;
    let hasRushFour   = false;
    let hasLiveThree  = false;
    let hasSleepThree = false;
    let hasFive       = false;

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
    if (countLiveThrees(patterns) >= 2)                         return { type: 'double-three', score: 10000 };
    if (hasLiveThree && hasSleepThree)                          return { type: 'three-sleep3', score: 3000 };
    if (hasLiveFour)                                            return { type: 'live-four', score: 10000 };
    if (hasLiveThree)                                           return { type: 'live-three', score: 1000 };

    return null;
  }

  function countLiveThrees(patterns) {
    return patterns.filter(p => p.count === 3 && p.openEnds === 2).length;
  }

  // ================================================================
  //  查找杀招落子
  //  扫描所有空位，返回我方进攻杀招和对手防守杀招
  //  返回 { attack: Move | null, defense: Move | null }
  // ================================================================
  G.findKillerMove = function (board, player) {
    const opponent = player === G.BLACK ? G.WHITE : G.BLACK;
    const candidates = G.getCandidateMoves(board);

    let bestAttack = null;
    let bestDefense = null;
    let bestAttackScore = -1;
    let bestDefenseScore = -1;

    for (const { row, col } of candidates) {
      // 我方在此落子后的杀招
      const myPatterns = classifyCell(board, row, col, player);
      const myKiller = detectKiller(myPatterns);
      if (myKiller && myKiller.score > bestAttackScore) {
        bestAttackScore = myKiller.score;
        bestAttack = { row, col, type: myKiller.type, score: myKiller.score };
      }

      // 对手在此落子后的杀招（需要防守）
      const oppPatterns = classifyCell(board, row, col, opponent);
      const oppKiller = detectKiller(oppPatterns);
      if (oppKiller && oppKiller.score > bestDefenseScore) {
        bestDefenseScore = oppKiller.score;
        bestDefense = { row, col, type: oppKiller.type, score: oppKiller.score };
      }
    }

    return { attack: bestAttack, defense: bestDefense };
  };

})(window.Gomoku);
