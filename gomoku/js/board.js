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
