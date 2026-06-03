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
      this.player = player;
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
            backNode.wins += 0.5;
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
