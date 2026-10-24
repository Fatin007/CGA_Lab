// board.cpp -- Implementation of the chess rules engine.
//
// The structure follows the standard chess-programming approach:
//
//   1. generate_pseudo_moves()  -- where could each piece legally go,
//                                  ignoring whether our own king would be
//                                  left in check?
//   2. filter_legal_moves()     -- try each one on the board and keep only
//                                  those where our king is still safe.
//
// This two-step approach means the "is my king in check" question is asked in
// exactly ONE place, which is what makes castling and en passant correct.

#include "board.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>

// ---------------------------------------------------------------------------
// Helpers.
// ---------------------------------------------------------------------------
static bool is_inside(int row, int col)
{
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}

// A piece "belongs" to the given side if it is that colour's piece.
static bool belongs_to(const Piece& piece, Color color)
{
    return piece.type != EMPTY && piece.color == color;
}

// The direction a pawn of the given colour moves in.
// White (row 0 is rank 1) moves towards increasing row numbers.
static int pawn_direction(Color color)
{
    return (color == WHITE) ? 1 : -1;
}

// The row a pawn of the given colour starts on.
static int pawn_start_row(Color color)
{
    return (color == WHITE) ? 1 : 6;
}

// The row a pawn of the given colour promotes on.
static int pawn_promotion_row(Color color)
{
    return (color == WHITE) ? 7 : 0;
}

// ---------------------------------------------------------------------------
// Construction and reset.
// ---------------------------------------------------------------------------
ChessBoard::ChessBoard()
{
    reset();
}

void ChessBoard::reset()
{
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
            board_[row][col] = Piece();

    // Back rank: R N B Q K B N R
    const PieceType back_rank[8] = { ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK };

    // Remember: row 0 is rank 1 (White's home row) and row 7 is rank 8.
    for (int col = 0; col < 8; ++col)
    {
        board_[0][col] = Piece{ back_rank[col], WHITE };
        board_[1][col] = Piece{ PAWN,               WHITE };
        board_[6][col] = Piece{ PAWN,               BLACK };
        board_[7][col] = Piece{ back_rank[col], BLACK };
    }

    side_to_move_       = WHITE;
    castling_rights_    = CASTLE_WHITE_KING | CASTLE_WHITE_QUEEN |
                          CASTLE_BLACK_KING | CASTLE_BLACK_QUEEN;
    en_passant_target_  = -1;
    halfmove_clock_     = 0;
    fullmove_number_    = 1;
    history_.clear();

    // A new game starts with only one position: the starting one.
    position_keys_.clear();
    if (track_repetitions_)
        position_keys_.push_back(position_key());
}

Piece ChessBoard::at(int row, int col) const
{
    if (!is_inside(row, col)) return Piece();
    return board_[row][col];
}

// ---------------------------------------------------------------------------
// Move generation.
// ---------------------------------------------------------------------------

// Sliding pieces (rook, bishop, queen) move any number of squares along a
// straight line or diagonal, stopping at the first piece they meet.
void ChessBoard::add_moves_in_direction(int row, int col, int dr, int dc,
                                        std::vector<Move>& out) const
{
    const Color us = side_to_move_;

    int r = row + dr;
    int c = col + dc;

    while (is_inside(r, c))
    {
        const Piece& target = board_[r][c];

        if (target.is_empty())
        {
            Move move;
            move.fromRow = row; move.fromCol = col;
            move.toRow   = r;    move.toCol   = c;
            out.push_back(move);
        }
        else
        {
            // We can capture an enemy piece, but then we must stop.
            if (target.color != us)
            {
                Move move;
                move.fromRow = row; move.fromCol = col;
                move.toRow   = r;    move.toCol   = c;
                out.push_back(move);
            }
            break;
        }

        r += dr;
        c += dc;
    }
}

// The knight moves in an "L" shape: 2 squares in one direction, then 1
// perpendicular. It jumps, so pieces in between do not matter.
void ChessBoard::add_knight_moves(int row, int col, std::vector<Move>& out) const
{
    static const int offsets[8][2] = {
        {  2,  1 }, {  2, -1 }, { -2,  1 }, { -2, -1 },
        {  1,  2 }, {  1, -2 }, { -1,  2 }, { -1, -2 }
    };

    const Color us = side_to_move_;

    for (const auto& offset : offsets)
    {
        int r = row + offset[0];
        int c = col + offset[1];

        if (!is_inside(r, c)) continue;

        const Piece& target = board_[r][c];

        // Either the square is empty, or it holds an enemy piece we may capture.
        if (target.is_empty() || target.color != us)
        {
            Move move;
            move.fromRow = row; move.fromCol = col;
            move.toRow   = r;    move.toCol   = c;
            out.push_back(move);
        }
    }
}

// Castling: the king moves two squares sideways, and the rook jumps to the
// other side of the king.
//
// Requirements:
//   * king and rook have not moved before (tracked as castling rights)
//   * all squares between king and rook are empty
//   * the king is not currently in check
//   * the king does not pass through an attacked square
//   * the king does not land on an attacked square
void ChessBoard::add_castling_moves(int row, int col, std::vector<Move>& out) const
{
    if (row != 7 && row != 0) return;                 // king must be on its home rank
    if (col != 4) return;                            // king must be on the e-file

    const Color us = side_to_move_;
    const Color them = (us == WHITE) ? BLACK : WHITE;

    // --- King side (h-file rook) ---
    const int king_side_right = (us == WHITE) ? CASTLE_WHITE_KING : CASTLE_BLACK_KING;
    if (has_castling_right(king_side_right))
    {
        if (board_[row][5].is_empty() && board_[row][6].is_empty() &&
            board_[row][7].type == ROOK && board_[row][7].color == us)
        {
            // The king may not start in, pass through, or land on an attacked square.
            bool safe = !is_square_attacked(row, 4, them) &&
                        !is_square_attacked(row, 5, them) &&
                        !is_square_attacked(row, 6, them);

            if (safe)
            {
                Move move;
                move.fromRow = row; move.fromCol = 4;
                move.toRow   = row; move.toCol   = 6;
                move.castleKingSide = true;
                out.push_back(move);
            }
        }
    }

    // --- Queen side (a-file rook) ---
    const int queen_side_right = (us == WHITE) ? CASTLE_WHITE_QUEEN : CASTLE_BLACK_QUEEN;
    if (has_castling_right(queen_side_right))
    {
        if (board_[row][1].is_empty() && board_[row][2].is_empty() && board_[row][3].is_empty() &&
            board_[row][0].type == ROOK && board_[row][0].color == us)
        {
            bool safe = !is_square_attacked(row, 4, them) &&
                        !is_square_attacked(row, 3, them) &&
                        !is_square_attacked(row, 2, them);

            if (safe)
            {
                Move move;
                move.fromRow = row; move.fromCol = 4;
                move.toRow   = row; move.toCol   = 2;
                move.castleQueenSide = true;
                out.push_back(move);
            }
        }
    }
}

// Generates every move the piece on (row, col) could make, ignoring whether
// our own king would be left in check.
void ChessBoard::generate_pseudo_moves(int row, int col, std::vector<Move>& out) const
{
    const Piece piece = board_[row][col];
    if (piece.is_empty()) return;

    const Color us    = side_to_move_;
    const Color them  = (us == WHITE) ? BLACK : WHITE;
    const int   dr    = pawn_direction(us);

    switch (piece.type)
    {
    case PAWN:
    {
        // One square forward, if empty.
        int r = row + dr;
        if (is_inside(r, col) && board_[r][col].is_empty())
        {
            Move move;
            move.fromRow = row; move.fromCol = col;
            move.toRow   = r;    move.toCol   = col;
            if (r == pawn_promotion_row(us))
            {
                // A pawn reaching the far side promotes. We generate one move
                // per promotion piece so the caller can choose.
                move.promotion = QUEEN;
                out.push_back(move);
                move.promotion = ROOK;
                out.push_back(move);
                move.promotion = BISHOP;
                out.push_back(move);
                move.promotion = KNIGHT;
                out.push_back(move);
            }
            else
            {
                out.push_back(move);
            }
        }

        // Two squares forward from the starting rank, if both squares are empty.
        int r2 = row + 2 * dr;
        if (row == pawn_start_row(us) && is_inside(r2, col) &&
            board_[row + dr][col].is_empty() && board_[r2][col].is_empty())
        {
            Move move;
            move.fromRow = row; move.fromCol = col;
            move.toRow   = r2;  move.toCol   = col;
            out.push_back(move);
        }

        // Captures diagonally.
        for (int dc : { -1, 1 })
        {
            int c = col + dc;
            if (!is_inside(r, c)) continue;

            const Piece& target = board_[r][c];

            if (!target.is_empty() && target.color == them)
            {
                Move move;
                move.fromRow = row; move.fromCol = col;
                move.toRow   = r;    move.toCol   = c;
                if (r == pawn_promotion_row(us))
                {
                    move.promotion = QUEEN; out.push_back(move);
                    move.promotion = ROOK;  out.push_back(move);
                    move.promotion = BISHOP;out.push_back(move);
                    move.promotion = KNIGHT;out.push_back(move);
                }
                else
                {
                    out.push_back(move);
                }
            }
            else if (target.is_empty() && r == en_passant_target_)
            {
                // En passant: capture the pawn that just moved past us.
                Move move;
                move.fromRow = row; move.fromCol = col;
                move.toRow   = r;    move.toCol   = c;
                move.enPassant = true;
                out.push_back(move);
            }
        }
        break;
    }

    case KNIGHT:
        add_knight_moves(row, col, out);
        break;

    case BISHOP:
        add_moves_in_direction(row, col,  1,  1, out);
        add_moves_in_direction(row, col,  1, -1, out);
        add_moves_in_direction(row, col, -1,  1, out);
        add_moves_in_direction(row, col, -1, -1, out);
        break;

    case ROOK:
        add_moves_in_direction(row, col,  1,  0, out);
        add_moves_in_direction(row, col, -1,  0, out);
        add_moves_in_direction(row, col,  0,  1, out);
        add_moves_in_direction(row, col,  0, -1, out);
        break;

    case QUEEN:
        add_moves_in_direction(row, col,  1,  1, out);
        add_moves_in_direction(row, col,  1, -1, out);
        add_moves_in_direction(row, col, -1,  1, out);
        add_moves_in_direction(row, col, -1, -1, out);
        add_moves_in_direction(row, col,  1,  0, out);
        add_moves_in_direction(row, col, -1,  0, out);
        add_moves_in_direction(row, col,  0,  1, out);
        add_moves_in_direction(row, col,  0, -1, out);
        break;

    case KING:
    {
        // The king moves ONE square in any direction. It must not use the
        // sliding helper (add_moves_in_direction), because that helper lets a
        // piece travel any number of squares, which is wrong for a king.
        for (int ddr = -1; ddr <= 1; ++ddr)
        {
            for (int ddc = -1; ddc <= 1; ++ddc)
            {
                if (ddr == 0 && ddc == 0) continue;

                int r = row + ddr;
                int c = col + ddc;

                if (!is_inside(r, c)) continue;

                const Piece& target = board_[r][c];

                // Either the square is empty, or it holds an enemy piece.
                if (target.is_empty() || target.color != us)
                {
                    Move move;
                    move.fromRow = row; move.fromCol = col;
                    move.toRow   = r;    move.toCol   = c;
                    out.push_back(move);
                }
            }
        }
        add_castling_moves(row, col, out);
        break;
    }

    default:
        break;
    }

    // Fill in the "captured piece" hint for every move we generated. The AI
    // uses it to try captures first, which makes its search much faster.
    //
    // For an en passant capture the captured pawn is not on the destination
    // square, so we point at the square it really sits on.
    for (Move& move : out)
    {
        if (move.enPassant)
            move.captured_piece_hint = board_[move.fromRow][move.toCol];
        else
            move.captured_piece_hint = board_[move.toRow][move.toCol];
    }
}

// Keeps only the moves that do not leave our own king in check.
//
// This is the single place where king safety is enforced, so special moves
// like en passant (where a pawn can be captured "from behind") are handled
// correctly without any extra special cases.
std::vector<Move> ChessBoard::filter_legal_moves(std::vector<Move> candidates) const
{
    std::vector<Move> legal;
    const Color us = side_to_move_;

    for (const Move& move : candidates)
    {
        // ChessBoard::make_move and undo_move are not const, so we work on a
        // copy of the board for this test. The board is small (a few hundred
        // bytes), so copying it is cheap and keeps the code simple.
        ChessBoard test = *this;

        // make_move refuses moves that capture a king (that must never
        // happen in a real game). If it refuses, the move is not legal and
        // we must NOT call undo_move, because nothing was pushed onto the
        // undo stack in that case.
        if (!test.make_move(move))
            continue;

        if (!test.in_check(us))
            legal.push_back(move);

        test.undo_move(move);
    }

    return legal;
}

std::vector<Move> ChessBoard::legal_moves() const
{
    std::vector<Move> candidates;

    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
            if (belongs_to(board_[row][col], side_to_move_))
                generate_pseudo_moves(row, col, candidates);

    return filter_legal_moves(candidates);
}

std::vector<Move> ChessBoard::legal_moves_from(int row, int col) const
{
    if (!is_inside(row, col)) return {};

    if (!belongs_to(board_[row][col], side_to_move_))
        return {};

    std::vector<Move> candidates;
    generate_pseudo_moves(row, col, candidates);

    return filter_legal_moves(candidates);
}

// ---------------------------------------------------------------------------
// Checking whether a square is attacked.
//
// Instead of generating moves for every enemy piece (slow), we look outward
// from the square along each direction and see what we run into.
// ---------------------------------------------------------------------------
bool ChessBoard::is_square_attacked(int row, int col, Color by) const
{
    if (!is_inside(row, col)) return false;

    // --- Pawns ---
    // A pawn of colour `by` attacks the square if it sits one row "behind" it
    // (diagonally) and one file to either side.
    int pawn_source_row = row - pawn_direction(by);
    for (int dc : { -1, 1 })
    {
        int c = col + dc;
        if (!is_inside(pawn_source_row, c)) continue;

        const Piece& piece = board_[pawn_source_row][c];
        if (piece.type == PAWN && piece.color == by)
            return true;
    }

    // --- Knights ---
    static const int knight_offsets[8][2] = {
        {  2,  1 }, {  2, -1 }, { -2,  1 }, { -2, -1 },
        {  1,  2 }, {  1, -2 }, { -1,  2 }, { -1, -2 }
    };

    for (const auto& offset : knight_offsets)
    {
        int r = row + offset[0];
        int c = col + offset[1];
        if (!is_inside(r, c)) continue;

        const Piece& piece = board_[r][c];
        if (piece.type == KNIGHT && piece.color == by)
            return true;
    }

    // --- King ---
    for (int dr = -1; dr <= 1; ++dr)
    {
        for (int dc = -1; dc <= 1; ++dc)
        {
            if (dr == 0 && dc == 0) continue;

            int r = row + dr;
            int c = col + dc;
            if (!is_inside(r, c)) continue;

            const Piece& piece = board_[r][c];
            if (piece.type == KING && piece.color == by)
                return true;
        }
    }

    // --- Rooks, bishops and queens (sliding pieces) ---
    // Diagonals: bishop and queen.
    static const int diagonal_dirs[4][2] = { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
    for (const auto& dir : diagonal_dirs)
    {
        int r = row + dir[0];
        int c = col + dir[1];

        while (is_inside(r, c))
        {
            const Piece& piece = board_[r][c];

            if (!piece.is_empty())
            {
                if (piece.color == by && (piece.type == BISHOP || piece.type == QUEEN))
                    return true;
                break;   // blocked by the first piece we meet
            }

            r += dir[0];
            c += dir[1];
        }
    }

    // Straight lines: rook and queen.
    static const int straight_dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for (const auto& dir : straight_dirs)
    {
        int r = row + dir[0];
        int c = col + dir[1];

        while (is_inside(r, c))
        {
            const Piece& piece = board_[r][c];

            if (!piece.is_empty())
            {
                if (piece.color == by && (piece.type == ROOK || piece.type == QUEEN))
                    return true;
                break;
            }

            r += dir[0];
            c += dir[1];
        }
    }

    return false;
}

void ChessBoard::find_king(Color color, int& row, int& col) const
{
    for (int r = 0; r < 8; ++r)
    {
        for (int c = 0; c < 8; ++c)
        {
            if (board_[r][c].type == KING && board_[r][c].color == color)
            {
                row = r;
                col = c;
                return;
            }
        }
    }

    row = -1;
    col = -1;
}

bool ChessBoard::in_check(Color color) const
{
    int king_row = -1;
    int king_col = -1;
    find_king(color, king_row, king_col);

    if (king_row == -1) return false;   // no king (should not happen in a real game)

    return is_square_attacked(king_row, king_col, (color == WHITE) ? BLACK : WHITE);
}

// ---------------------------------------------------------------------------
// Making and undoing moves.
// ---------------------------------------------------------------------------

// Removes castling rights that are no longer valid:
//   * if the king moves, that colour loses both rights
//   * if a rook moves from its home square, that side loses that right
//   * if a rook is captured on its home square, the opponent loses that right
void ChessBoard::update_castling_rights(const Move& move)
{
    // The king moving removes both of that colour's rights.
    if (board_[move.fromRow][move.fromCol].type == KING)
    {
        castling_rights_ &= ~(CASTLE_WHITE_KING  | CASTLE_WHITE_QUEEN);
        castling_rights_ &= ~(CASTLE_BLACK_KING  | CASTLE_BLACK_QUEEN);
    }

    // A rook moving from the a-file or h-file removes that rook's right.
    if (board_[move.fromRow][move.fromCol].type == ROOK)
    {
        if (move.fromRow == 7 && move.fromCol == 0) castling_rights_ &= ~CASTLE_WHITE_QUEEN;
        if (move.fromRow == 7 && move.fromCol == 7) castling_rights_ &= ~CASTLE_WHITE_KING;
        if (move.fromRow == 0 && move.fromCol == 0) castling_rights_ &= ~CASTLE_BLACK_QUEEN;
        if (move.fromRow == 0 && move.fromCol == 7) castling_rights_ &= ~CASTLE_BLACK_KING;
    }

    // A rook captured on its home square removes the opponent's right.
    if (board_[move.toRow][move.toCol].type == ROOK)
    {
        if (move.toRow == 7 && move.toCol == 0) castling_rights_ &= ~CASTLE_WHITE_QUEEN;
        if (move.toRow == 7 && move.toCol == 7) castling_rights_ &= ~CASTLE_WHITE_KING;
        if (move.toRow == 0 && move.toCol == 0) castling_rights_ &= ~CASTLE_BLACK_QUEEN;
        if (move.toRow == 0 && move.toCol == 7) castling_rights_ &= ~CASTLE_BLACK_KING;
    }
}

bool ChessBoard::make_move(const Move& move)
{
    if (!is_inside(move.fromRow, move.fromCol) || !is_inside(move.toRow, move.toCol))
        return false;

    const Piece moving = board_[move.fromRow][move.fromCol];
    if (moving.is_empty()) return false;
    if (moving.color != side_to_move_) return false;

    // --- A king may never be captured ---
    //
    // In a real game the game ends the moment a king is checkmated, so a king
    // is never actually taken. But our move generator does not know that: it
    // happily generates a move that captures the enemy king, and if we allowed
    // it the position would end up with only one king on the board. That
    // would break in_check() (there is no king to be in check) and with it
    // the checkmate and stalemate detection.
    //
    // So we refuse the move here. The AI therefore searches for checkmate
    // and stalemate rather than for a king capture, which is what we want.
    if (board_[move.toRow][move.toCol].type == KING)
        return false;

    // Save everything we need to undo this move.
    MoveUndo undo;
    undo.move                 = move;
    undo.capturedPiece        = board_[move.toRow][move.toCol];
    undo.capturedPawn         = Piece();
    undo.capturedPawnRow      = -1;
    undo.capturedPawnCol      = -1;
    undo.previousSideToMove   = side_to_move_;
    undo.previousCastlingRights   = castling_rights_;
    undo.previousEnPassantTarget  = en_passant_target_;
    undo.previousHalfmoveClock    = halfmove_clock_;
    undo.previousFullmoveNumber  = fullmove_number_;

    // --- En passant: remove the pawn that sits beside the moving pawn ---
    if (move.enPassant)
    {
        int captured_row = move.fromRow;
        undo.capturedPawn     = board_[captured_row][move.toCol];
        undo.capturedPawnRow  = captured_row;
        undo.capturedPawnCol  = move.toCol;
        board_[captured_row][move.toCol] = Piece();
    }

    // --- Move the piece ---
    board_[move.toRow][move.toCol] = moving;
    board_[move.fromRow][move.fromCol] = Piece();

    // --- Promotion ---
    if (move.promotion != EMPTY)
        board_[move.toRow][move.toCol].type = move.promotion;

    // --- Castling: also move the rook ---
    //
    // King side: the king moves e->g and the rook moves h->f.
    // Queen side: the king moves e->c and the rook moves a->d.
    if (move.castleKingSide)
    {
        board_[move.toRow][5] = board_[move.toRow][7];
        board_[move.toRow][7] = Piece();
    }
    else if (move.castleQueenSide)
    {
        board_[move.toRow][3] = board_[move.toRow][0];
        board_[move.toRow][0] = Piece();
    }

    // --- Update castling rights ---
    update_castling_rights(move);

    // --- Update the en passant target ---
    // It is only set when a pawn moves two squares, and it is cleared after
    // every other move.
    en_passant_target_ = -1;
    if (moving.type == PAWN && std::abs(move.toRow - move.fromRow) == 2)
        en_passant_target_ = (move.fromRow + move.toRow) / 2;

    // --- Update the clocks ---
    bool is_capture = !undo.capturedPiece.is_empty() || move.enPassant;
    if (moving.type == PAWN || is_capture)
        halfmove_clock_ = 0;
    else
        ++halfmove_clock_;

    if (side_to_move_ == BLACK)
        ++fullmove_number_;

    // --- Switch side ---
    side_to_move_ = (side_to_move_ == WHITE) ? BLACK : WHITE;

    history_.push_back(undo);

    // Remember the position we have just reached, so we can spot a
    // repetition later.
    if (track_repetitions_)
        position_keys_.push_back(position_key());

    return true;
}

void ChessBoard::undo_move(const Move& move)
{
    // We do not actually need the move: everything needed to undo it was
    // saved on the undo stack by make_move.
    (void)move;

    if (history_.empty()) return;

    MoveUndo undo = history_.back();
    history_.pop_back();

    // Switch the side back.
    side_to_move_ = undo.previousSideToMove;

    // Restore the counters and rights.
    castling_rights_        = undo.previousCastlingRights;
    en_passant_target_      = undo.previousEnPassantTarget;
    halfmove_clock_         = undo.previousHalfmoveClock;
    fullmove_number_        = undo.previousFullmoveNumber;

    // Move the piece back.
    Piece piece = board_[undo.move.toRow][undo.move.toCol];
    board_[undo.move.fromRow][undo.move.fromCol] = piece;
    board_[undo.move.toRow][undo.move.toCol] = Piece();

    // Put the captured piece back.
    if (!undo.capturedPiece.is_empty())
        board_[undo.move.toRow][undo.move.toCol] = undo.capturedPiece;

    // Put an en passant pawn back.
    if (undo.capturedPawnRow >= 0)
        board_[undo.capturedPawnRow][undo.capturedPawnCol] = undo.capturedPawn;

    // Move the rook back after castling.
    //
    // King side: the rook was moved h->f, so move it back f->h.
    // Queen side: the rook was moved a->d, so move it back d->a.
    if (undo.move.castleKingSide)
    {
        board_[undo.move.toRow][7] = board_[undo.move.toRow][5];
        board_[undo.move.toRow][5] = Piece();
    }
    else if (undo.move.castleQueenSide)
    {
        board_[undo.move.toRow][0] = board_[undo.move.toRow][3];
        board_[undo.move.toRow][3] = Piece();
    }

    // Forget the position we have just left, so the repetition count goes
    // back to what it was before the move.
    if (track_repetitions_ && !position_keys_.empty())
        position_keys_.pop_back();
}

// ---------------------------------------------------------------------------
// Game end conditions.
// ---------------------------------------------------------------------------
bool ChessBoard::is_checkmate() const
{
    if (!in_check(side_to_move_)) return false;
    return legal_moves().empty();
}

bool ChessBoard::is_stalemate() const
{
    if (in_check(side_to_move_)) return false;
    return legal_moves().empty();
}

bool ChessBoard::is_fifty_move_draw() const
{
    return halfmove_clock_ >= 100;   // 100 half-moves = 50 moves by each side
}

// ---------------------------------------------------------------------------
// Threefold repetition.
//
// The rules of chess say the game is drawn if the same position occurs three
// times. This matters a lot for a computer opponent: the AI picks randomly
// among the moves that score exactly the same, and in an even position there
// are often several such moves. Without a repetition rule, two computers can
// shuffle the pieces back and forth for ever without the game ever ending.
//
// The way we detect it is simple. Every position is reduced to a single
// number -- a "key". Two positions are the same if and only if they have the
// same key. We keep a list of the keys of all the positions that have been
// reached, and the game is drawn when the current key has appeared three
// times.
// ---------------------------------------------------------------------------

std::uint64_t ChessBoard::position_key() const
{
    // A mixing constant (it is the golden ratio, which has good bit
    // distribution properties).
    const std::uint64_t MAGIC = 0x9E3779B97F4A7C15ULL;

    std::uint64_t key = MAGIC;

    // The pieces. Each square contributes its type and its colour.
    for (int row = 0; row < 8; ++row)
    {
        for (int col = 0; col < 8; ++col)
        {
            const Piece piece = board_[row][col];
            const std::uint64_t value =
                (static_cast<std::uint64_t>(piece.type) << 1) |
                (piece.is_empty() ? 0ULL : static_cast<std::uint64_t>(piece.color));

            // Multiply and add: this spreads the value across the whole key
            // instead of letting one square's value overwrite another's.
            key ^= value + MAGIC + (key << 6) + (key >> 2);
        }
    }

    // The side to move.
    key ^= (static_cast<std::uint64_t>(side_to_move_) + MAGIC) + (key << 6) + (key >> 2);

    // The castling rights.
    key ^= (static_cast<std::uint64_t>(castling_rights_) + MAGIC) + (key << 6) + (key >> 2);

    // The en passant target.
    key ^= (static_cast<std::uint64_t>(en_passant_target_ + 1) + MAGIC) + (key << 6) + (key >> 2);

    return key;
}

bool ChessBoard::is_threefold_repetition() const
{
    // If the tracking is off (the AI's search copy), we have no history to
    // look at, so there is no repetition to report.
    if (!track_repetitions_) return false;

    // Count how many times the current position's key appears in the list.
    // We include the current position itself, so a position that has been
    // seen twice before counts as the third occurrence.
    const std::uint64_t current = position_key();

    int occurrences = 0;
    for (const std::uint64_t key : position_keys_)
    {
        if (key == current)
        {
            ++occurrences;
            if (occurrences >= 3) return true;
        }
    }

    return false;
}

int ChessBoard::count_material(Color color) const
{
    int total = 0;
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
            if (board_[row][col].color == color && !board_[row][col].is_empty())
                ++total;
    return total;
}

// ---------------------------------------------------------------------------
// Test helpers.
// ---------------------------------------------------------------------------
void ChessBoard::clear_board()
{
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
            board_[row][col] = Piece();

    en_passant_target_ = -1;
    halfmove_clock_    = 0;
    fullmove_number_   = 1;
    history_.clear();

    // The board has been wiped, so the old line of play no longer applies.
    position_keys_.clear();
    if (track_repetitions_)
        position_keys_.push_back(position_key());
}

void ChessBoard::place(const std::string& square, PieceType type, Color color)
{
    if (square.size() != 2) return;

    int col = square[0] - 'a';
    int row = square[1] - '1';

    if (!is_inside(row, col)) return;

    board_[row][col] = Piece{ type, color };
}

void ChessBoard::clear_castling_rights()
{
    castling_rights_ = 0;
}

// ---------------------------------------------------------------------------
// Text output.
// ---------------------------------------------------------------------------
std::string ChessBoard::square_name(int row, int col)
{
    if (!is_inside(row, col)) return "??";
    std::string name;
    name += static_cast<char>('a' + col);
    name += static_cast<char>('1' + row);
    return name;
}

std::string ChessBoard::to_ascii() const
{
    std::ostringstream out;

    out << "\n    +---+---+---+---+---+---+---+---+\n";
    for (int row = 7; row >= 0; --row)
    {
        out << "  " << (row + 1) << " |";
        for (int col = 0; col < 8; ++col)
        {
            const Piece& piece = board_[row][col];
            char symbol = ' ';

            if (!piece.is_empty())
            {
                switch (piece.type)
                {
                case PAWN:   symbol = 'P'; break;
                case KNIGHT: symbol = 'N'; break;
                case BISHOP: symbol = 'B'; break;
                case ROOK:   symbol = 'R'; break;
                case QUEEN:  symbol = 'Q'; break;
                case KING:   symbol = 'K'; break;
                default:     symbol = '?'; break;
                }

                // Lower case for Black, upper case for White.
                if (piece.color == BLACK)
                    symbol = static_cast<char>(std::tolower(symbol));
            }

            out << ' ' << symbol << " |";
        }
        out << "\n    +---+---+---+---+---+---+---+---+\n";
    }

    out << "      a   b   c   d   e   f   g   h\n";
    out << "  Side to move: " << (side_to_move_ == WHITE ? "White" : "Black") << "\n";

    return out.str();
}

// ---------------------------------------------------------------------------
// Self-test: a set of assertions that prove the rules engine works.
//
// This is a separate program-level function so it can be called from main()
// with a command line flag, and it is also something we can show to faculty
// as proof that the chess logic is correct and completely independent of the
// graphics code.
// ---------------------------------------------------------------------------

// Counts the legal moves in the current position (helper for the tests).
static int count_legal_moves(const ChessBoard& board)
{
    return static_cast<int>(board.legal_moves().size());
}

// Finds a legal move matching a "from-to" pair given as squares.
static bool has_move(const ChessBoard& board, const std::string& from, const std::string& to)
{
    for (const Move& move : board.legal_moves())
    {
        if (ChessBoard::square_name(move.fromRow, move.fromCol) == from &&
            ChessBoard::square_name(move.toRow,   move.toCol)   == to)
            return true;
    }
    return false;
}

// Plays a sequence of moves given as "e2e4" style strings.
static bool play_sequence(ChessBoard& board, const std::vector<std::string>& moves)
{
    for (const std::string& text : moves)
    {
        if (text.size() != 4) return false;

        int from_col = text[0] - 'a';
        int from_row = text[1] - '1';
        int to_col   = text[2] - 'a';
        int to_row   = text[3] - '1';

        bool found = false;
        for (const Move& move : board.legal_moves())
        {
            if (move.fromRow == from_row && move.fromCol == from_col &&
                move.toRow   == to_row   && move.toCol   == to_col)
            {
                if (!board.make_move(move)) return false;
                found = true;
                break;
            }
        }

        if (!found) return false;
    }
    return true;
}

bool run_rules_self_test()
{
    int passed = 0;
    int failed = 0;

    auto check = [&](const char* description, bool condition)
    {
        if (condition)
        {
            ++passed;
            std::cout << "  [PASS] " << description << "\n";
        }
        else
        {
            ++failed;
            std::cout << "  [FAIL] " << description << "\n";
        }
    };

    std::cout << "\n=== Chess Rules Engine Self-Test ===\n\n";

    // --- Test 1: the starting position has 20 legal moves ---
    {
        ChessBoard board;
        check("Starting position has exactly 20 legal moves",
              count_legal_moves(board) == 20);
        check("White pawn on e2 can move two squares",
              has_move(board, "e2", "e4"));
        check("Knight on b1 can move to c3",
              has_move(board, "b1", "c3"));
        check("No castling is allowed yet (e4 square attack check aside, pieces block)",
              !has_move(board, "e1", "g1"));
    }

    // --- Test 2: after 1.e4 there are still 20 legal moves ---
    {
        ChessBoard board;
        play_sequence(board, { "e2e4" });
        check("After 1.e4 Black also has exactly 20 legal moves",
              count_legal_moves(board) == 20);
        check("Black pawn on e7 can move two squares",
              has_move(board, "e7", "e5"));
    }

    // --- Test 3: Fool's Mate leads to checkmate ---
    {
        ChessBoard board;
        // The classic Fool's Mate: 1.f3 e5 2.g4 Qh4#
        // For the queen to reach h4 the diagonal d8-e7-f6-g5-h4 must be clear,
        // so Black plays 1...e6 first and only then 2...Qh4.
        play_sequence(board, { "f2f3", "e7e6", "g2g4", "d8h4" });
        check("Fool's Mate position is checkmate",
              board.is_checkmate());
        check("Fool's Mate position is not stalemate",
              !board.is_stalemate());
        check("Fool's Mate position has zero legal moves",
              count_legal_moves(board) == 0);
    }

    // --- Test 4: stalemate ---
    {
        // Classic stalemate: Black king a8, White queen c7, White king c6.
        // Black to move is NOT in check, but the king has no legal move:
        //   a7 is covered by the queen on c7,
        //   b7 is covered by the queen on c7 and the king on c6,
        //   b8 is covered by the queen on c7.
        ChessBoard board;
        board.clear_board();
        board.clear_castling_rights();
        board.place("a8", KING,  BLACK);
        board.place("c7", QUEEN, WHITE);
        board.place("c6", KING,  WHITE);
        board.set_side_to_move(BLACK);

        check("Stalemate position has zero legal moves",
              count_legal_moves(board) == 0);
        check("Stalemate position is not in check",
              !board.in_check(BLACK));
        check("Stalemate position is detected correctly",
              board.is_stalemate());
        check("Stalemate position is not checkmate",
              !board.is_checkmate());
    }

    // --- Test 5: castling ---
    {
        ChessBoard board;
        // 1.e4 e5 2.Nf3 Nc6 3.Bc4 Bc5 4.O-O
        //
        // NOTE: row 0 is rank 1, so the White back rank lives on row 0 and
        // the Black back rank lives on row 7.
        play_sequence(board, { "e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "f8c5", "e1g1" });
        check("White can castle king-side after clearing the path",
              board.at(0, 6).type == KING);
        check("White rook moved to f1 after castling",
              board.at(0, 5).type == ROOK);
        check("The square the king came from is now empty",
              board.at(0, 4).is_empty());
    }

    // --- Test 6: en passant ---
    {
        ChessBoard board;
        // 1.e4 a6 2.e5 d5 3.exd6 (en passant)
        play_sequence(board, { "e2e4", "a7a6", "e4e5", "d7d5", "e5d6" });
        check("En passant capture places a white pawn on d6",
              board.at(5, 3).type == PAWN && board.at(5, 3).color == WHITE);
        check("En passant removes the black pawn that moved two squares",
              board.at(4, 3).is_empty());
    }

    // --- Test 7: promotion ---
    {
        // Put a White pawn on a7, ready to move to a8 and promote.
        ChessBoard board;
        board.clear_board();
        board.clear_castling_rights();
        board.place("a7", PAWN, WHITE);
        board.place("e1", KING, WHITE);
        board.place("h8", KING, BLACK);
        board.set_side_to_move(WHITE);

        // The engine must offer a promotion move to each of the four pieces.
        int promotion_options = 0;
        bool has_queen = false;
        for (const Move& move : board.legal_moves())
        {
            if (move.toRow == 7 && move.toCol == 0 && move.promotion != EMPTY)
            {
                ++promotion_options;
                if (move.promotion == QUEEN) has_queen = true;
            }
        }

        check("A pawn on the 7th rank has four promotion options",
              promotion_options == 4);
        check("One of the promotion options is a queen",
              has_queen);

        // Play the promotion to a queen and confirm the result.
        for (const Move& move : board.legal_moves())
        {
            if (move.toRow == 7 && move.toCol == 0 && move.promotion == QUEEN)
            {
                board.make_move(move);
                break;
            }
        }

        check("Pawn reaching the last rank is promoted to a piece",
              !board.at(7, 0).is_empty() && board.at(7, 0).color == WHITE);
        check("The promoted piece is no longer a pawn",
              board.at(7, 0).type != PAWN && board.at(7, 0).type != EMPTY);
        check("The promoted piece is a queen",
              board.at(7, 0).type == QUEEN);
    }

    // --- Test 8: a pinned piece may not move ---
    {
        // White king e1, White knight e2, Black rook e8.
        //
        // The knight is pinned along the e-file. The knight itself may not
        // move (that would expose the king), but the king still has the
        // normal escape squares off the file, so this is not checkmate.
        ChessBoard board;
        board.clear_board();
        board.clear_castling_rights();
        board.place("e1", KING,  WHITE);
        board.place("e2", KNIGHT, WHITE);
        board.place("e8", ROOK,  BLACK);
        board.set_side_to_move(WHITE);

        check("The king is not in check (the knight blocks the rook)",
              !board.in_check(WHITE));
        check("The pinned knight itself has no legal moves",
              board.legal_moves_from(1, 4).empty());
        check("The king still has its four escape squares",
              count_legal_moves(board) == 4);

        // Now remove the knight's escape route by putting the king in a
        // position where it is genuinely checkmated, to be sure the two
        // conditions are not confused with each other.
        ChessBoard mate;
        mate.clear_board();
        mate.clear_castling_rights();
        mate.place("e1", KING,   WHITE);
        mate.place("e2", KNIGHT, WHITE);
        mate.place("e8", ROOK,   BLACK);
        mate.place("d1", ROOK,   BLACK);
        mate.place("f1", ROOK,   BLACK);
        mate.set_side_to_move(WHITE);

        check("A pinned knight with no king escape is checkmate",
              mate.is_checkmate());
        check("The checkmate position has no legal moves",
              count_legal_moves(mate) == 0);
    }

    // --- Test 9: the 50-move rule ---
    {
        ChessBoard board;
        // Play 100 half-moves where nothing is captured and no pawn moves.
        // We use a repeating sequence of knight moves that keeps the kings safe.
        const char* sequence[] = {
            "g1f3", "g8f6", "f3g1", "f6g8",
            "b1c3", "b8c6", "c3b1", "c6b8"
        };

        bool ok = true;

        for (int i = 0; i < 100 && ok; ++i)
        {
            const char* text = sequence[i % 8];
            bool played = false;

            for (const Move& move : board.legal_moves())
            {
                if (ChessBoard::square_name(move.fromRow, move.fromCol) == std::string(text).substr(0, 2) &&
                    ChessBoard::square_name(move.toRow, move.toCol)       == std::string(text).substr(2, 2))
                {
                    played = board.make_move(move);
                    break;
                }
            }

            if (!played) { ok = false; break; }
        }

        check("A sequence of 100 quiet half-moves can be played", ok);
        check("The halfmove clock reaches 100", board.halfmove_clock() == 100);
        check("The 50-move rule is detected",
              board.is_fifty_move_draw());
    }

    // --- Test 8b: threefold repetition ---
    //
    // We shuffle the knights back and forth. After the sequence has gone
    // round twice we are back at the starting position for the third time,
    // which is a draw.
    {
        ChessBoard board;
        board.reset();

        const char* sequence[] = {
            "g1f3", "g8f6", "f3g1", "f6g8"
        };

        auto play = [&](const char* text) -> bool
        {
            const std::string from = std::string(text).substr(0, 2);
            const std::string to   = std::string(text).substr(2, 2);

            for (const Move& move : board.legal_moves())
            {
                if (ChessBoard::square_name(move.fromRow, move.fromCol) == from &&
                    ChessBoard::square_name(move.toRow, move.toCol)       == to)
                {
                    return board.make_move(move);
                }
            }
            return false;
        };

        // One round of the sequence brings us back to the starting position
        // for the second time, so this is not yet a draw.
        bool ok = true;
        for (int i = 0; i < 4 && ok; ++i) ok = play(sequence[i]);

        check("The knights can shuffle back and forth", ok);
        check("One round of shuffling is not a repetition",
              !board.is_threefold_repetition());

        // A second round brings us back to the starting position for the
        // third time: once at the start of the game, once after the first
        // round, and once now. That is a draw.
        for (int i = 0; i < 4 && ok; ++i) ok = play(sequence[i]);

        check("Two rounds of shuffling is a threefold repetition",
              board.is_threefold_repetition());
    }

    // --- Test 9: make_move / undo_move round trip ---
    {
        ChessBoard board;
        const int before = count_legal_moves(board);
        const std::string before_ascii = board.to_ascii();

        std::vector<Move> moves = board.legal_moves();
        board.make_move(moves[0]);
        board.undo_move(moves[0]);

        check("Undo restores the exact same position",
              board.to_ascii() == before_ascii);
        check("Undo restores the same number of legal moves",
              count_legal_moves(board) == before);
    }

    std::cout << "\n=== " << passed << " passed, " << failed << " failed ===\n\n";
    return failed == 0;
}
