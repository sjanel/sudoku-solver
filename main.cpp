#include <algorithm>
#include <bit>
#include <cstdint>
#include <iostream>
#include <vector>

namespace sudoku {

struct Cell {
  static constexpr uint16_t kAllPossibleBmp = (1U << 9) - 1;

  Cell() noexcept = default;

  explicit Cell(char c) {
    if (c == '.') {
      bmp = kAllPossibleBmp;
    } else {
      set(c - '0');
    }
  }

  explicit Cell(uint16_t bitmap) : bmp(bitmap) {}

  explicit Cell(int i) { set(i); }

  bool contains(int i) const { return bmp & (1U << (i - 1)); }
  bool is(int i) const { return bmp == (1U << (i - 1)); }
  bool isKnown() const { return bmp && (bmp & (bmp - 1)) == 0; }

  int nbPossibleValues() const { return std::popcount(bmp); }

  void set(int i) { bmp |= (1U << (i - 1)); }
  void reset(int i) { bmp ^= ~(1U << (i - 1)); }

  char convertToChar() const {
    for (int i = 1; i <= 9; ++i) {
      if (is(i)) {
        return '0' + i;
      }
    }
    return '.';
  }

  std::pair<Cell, Cell> split() const {
    // Duplicate one unknown cell into 2 distinct sets.
    // For instance, if we have an unknown cell with possible values {1,4,6},
    // We duplicate one instance of the grid with {1,4}, another instance with
    // {6}
    auto lhsNbBits = (nbPossibleValues() + 1) / 2;
    std::pair<Cell, Cell> ret;
    int nLhs = 0;
    for (int i = 1; i <= 9; ++i) {
      if (contains(i)) {
        ret.first.set(i);
        if (++nLhs == lhsNbBits) {
          break;
        }
      }
    }
    ret.second.bmp = (~ret.first.bmp) & bmp;
    return ret;
  }

  bool upgradeValidity(uint16_t& knownBmp, uint16_t& globalBmp) const {
    if (bmp == 0) {
      return false;
    }
    if (isKnown()) {
      if (knownBmp & bmp) {
        return false;
      }
      knownBmp |= bmp;
    }
    globalBmp |= bmp;
    return true;
  }

  uint16_t bmp{};
};

/// 3x3
struct SubGrid {
  bool isSolved() const {
    return std::ranges::all_of(cells, [](Cell c) { return c.isKnown(); });
  }

  bool isValid() const {
    uint16_t globalBmp = 0, knownBmp = 0;
    return std::ranges::all_of(cells,
                               [&](Cell c) {
                                 return c.upgradeValidity(knownBmp, globalBmp);
                               }) &&
           globalBmp == Cell::kAllPossibleBmp;
  }

  uint16_t getBmpKnownValues() const {
    uint16_t ret = 0;
    for (Cell cell : cells) {
      if (cell.isKnown()) {
        ret |= cell.bmp;
      }
    }
    return ret;
  }

  Cell& cellAt(int row, int col) { return cells[col * 3 + row]; }
  Cell cellAt(int row, int col) const { return cells[col * 3 + row]; }

  Cell cells[9]{};
};

struct Grid {
  explicit Grid(const std::vector<std::vector<char>>& board) {
    int row = 0;
    for (const auto& line : board) {
      int col = 0;
      for (char c : line) {
        cellAt(row, col) = Cell(c);
        ++col;
      }
      ++row;
    }
  }

  uint16_t rowKnownBmp(int row) const {
    uint16_t ret = 0;
    for (int col = 0; col < 9; ++col) {
      Cell cell = cellAt(row, col);
      if (cell.isKnown()) {
        ret |= cell.bmp;
      }
    }
    return ret;
  }

  uint16_t colKnownBmp(int col) const {
    uint16_t ret = 0;
    for (int row = 0; row < 9; ++row) {
      Cell cell = cellAt(row, col);
      if (cell.isKnown()) {
        ret |= cell.bmp;
      }
    }
    return ret;
  }

  bool isRowAndColValid(int subPos) const {
    uint16_t globalRowBmp = 0, knownRowBmp = 0;
    uint16_t globalColBmp = 0, knownColBmp = 0;
    for (int otherPos = 0; otherPos < 9; ++otherPos) {
      if (!cellAt(subPos, otherPos)
               .upgradeValidity(knownRowBmp, globalRowBmp) ||
          !cellAt(otherPos, subPos)
               .upgradeValidity(knownColBmp, globalColBmp)) {
        return false;
      }
    }
    return globalRowBmp == Cell::kAllPossibleBmp &&
           globalColBmp == Cell::kAllPossibleBmp;
  }

  bool isSolved() const {
    return std::ranges::all_of(
        subGrids, [](const SubGrid& subGrid) { return subGrid.isSolved(); });
  }

  bool isValid() const {
    for (int subPos = 0; subPos < 9; ++subPos) {
      if (!subGrids[subPos].isValid() || !isRowAndColValid(subPos)) {
        return false;
      }
    }
    return true;
  }

  void intersectConstraints() {
    bool updateDoneStep;
    do {
      uint16_t colBmps[9]{};
      for (int col = 0; col < 9; ++col) {
        colBmps[col] = ~colKnownBmp(col);
      }
      updateDoneStep = false;
      for (int row = 0; row < 9; ++row) {
        uint16_t rowBmp = ~rowKnownBmp(row);
        for (int col = 0; col < 9; ++col) {
          SubGrid& subGrid = subGridAt(row / 3, col / 3);
          Cell& cell = subGrid.cellAt(row % 3, col % 3);
          if (!cell.isKnown()) {
            // 3-way constraints narrowing: line, column, and SubGrid
            uint16_t oldBmp = cell.bmp;
            cell.bmp &= ~subGrid.getBmpKnownValues();
            cell.bmp &= rowBmp;
            cell.bmp &= colBmps[col];
            if (oldBmp != cell.bmp) {
              updateDoneStep = true;
            }
          }
        }
      }
    } while (updateDoneStep);
  }

  using GridVector = std::vector<Grid>;

  std::pair<int8_t, int8_t> getCellWithLowestConstraints() const {
    std::pair<int8_t, int8_t> ret;
    int lowestNbPossibilities = 10;
    for (int8_t r = 0; r < 9; ++r) {
      for (int8_t c = 0; c < 9; ++c) {
        auto nbPossibleValues = cellAt(r, c).nbPossibleValues();
        if (nbPossibleValues > 1 && nbPossibleValues < lowestNbPossibilities) {
          ret = {r, c};
          lowestNbPossibilities = nbPossibleValues;
          if (nbPossibleValues == 2) {
            return ret;
          }
        }
      }
    }
    return ret;
  }

  void generate(GridVector& gridVector) const {
    auto [row, col] = getCellWithLowestConstraints();
    // There should be at least one unknown cell at this point
    auto [lhsCell, rhsCell] = cellAt(row, col).split();

    gridVector.emplace_back(*this).cellAt(row, col) = lhsCell;
    gridVector.emplace_back(*this).cellAt(row, col) = rhsCell;
  }

  GridVector solve(int maxNbSolutions = -1) const {
    GridVector grids(1, *this);
    GridVector solutions;
    int invalidSolutions = 0;
    do {
      Grid grid = std::move(grids.back());
      grids.pop_back();
      grid.intersectConstraints();
      if (grid.isSolved()) {
        solutions.push_back(std::move(grid));
        if (--maxNbSolutions == 0) {
          break;
        }
        continue;
      }
      if (!grid.isValid()) {
        ++invalidSolutions;
        continue;
      }
      grid.generate(grids);
    } while (!grids.empty());
    return solutions;
  }

  Cell& cellAt(int row, int col) {
    return subGridAt(row / 3, col / 3).cellAt(row % 3, col % 3);
  }
  Cell cellAt(int row, int col) const {
    return subGridAt(row / 3, col / 3).cellAt(row % 3, col % 3);
  }

  SubGrid& subGridAt(int row, int col) { return subGrids[col * 3 + row]; }
  SubGrid subGridAt(int row, int col) const { return subGrids[col * 3 + row]; }

  friend std::ostream& operator<<(std::ostream& o, const Grid& grid) {
    static constexpr const char* const sep = "-------------";
    o << sep << std::endl;
    for (int row = 0; row < 9; ++row) {
      for (int col = 0; col < 9; ++col) {
        if (col % 3 == 0) {
          o << '|';
        }
        o << grid.cellAt(row, col).convertToChar();
      }
      o << '|' << std::endl;
      if (row % 3 == 2) {
        o << sep << std::endl;
      }
    }
    return o;
  }

  SubGrid subGrids[9];
};
}  // namespace sudoku

int main() {
  // Define a grid below. '.' marks an unknown point.
  std::vector<std::vector<char>> board = {
      {'.', '.', '.', '7', '.', '4', '.', '.', '5'},
      {'.', '2', '.', '.', '1', '.', '.', '.', '.'},
      {'.', '.', '.', '.', '.', '.', '.', '.', '2'},
      {'.', '9', '.', '.', '.', '6', '.', '5', '.'},
      {'.', '.', '.', '.', '7', '.', '.', '.', '8'},
      {'.', '5', '3', '2', '.', '.', '.', '1', '.'},
      {'4', '.', '.', '.', '.', '.', '.', '.', '.'},
      {'.', '.', '.', '.', '6', '.', '.', '.', '.'},
      {'.', '.', '.', '4', '.', '7', '.', '.', '.'}};

  sudoku::Grid grid(board);

  std::cout << "Input: " << std::endl << grid << std::endl;

  auto solutions = grid.solve();

  static constexpr int kMaxNbSolutionsToPrint = 3;

  std::cout << solutions.size() << " solutions, printing the first "
            << kMaxNbSolutionsToPrint << std::endl;

  int nbSolutionsPrinted = 0;
  for (const auto& sol : solutions) {
    std::cout << sol << std::endl;
    if (++nbSolutionsPrinted == kMaxNbSolutionsToPrint) {
      break;
    }
  }
}