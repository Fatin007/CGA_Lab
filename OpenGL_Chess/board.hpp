// board.hpp -- The chess rules engine.
//
// This file contains NO OpenGL code. That is deliberate: the chess logic can
// be understood (and tested) on its own, completely independently of the 3D
// graphics. The AI in ai.cpp reuses exactly the same move generator.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Piece types and colours.
// ---------------------------------------------------------------------------
enum PieceType
{
    EMPTY = 0,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING
};

enum Color
{
    WHITE = 0,
    BLACK = 1
};

// A piece on the board. An empty square holds {EMPTY, WHITE} and is treated
// as empty whenever type == EMPTY.
struct Piece
{
    PieceType type = EMPTY;
    Color     color = WHITE;

    bool is_empty() const { return type == EMPTY; }
};

// One legal move. A move is always described by source and destination
// square, plus a few flags for the special cases (castling, en passant and
// promotion).
struct Move
{
    int       fromRow = 0;
    int       fromCol = 0;
    int       toRow   = 0;
    int       toCol   = 0;
    PieceType promotion = EMPTY;    // piece the pawn becomes, if any
    bool      castleKingSide  = false;
    bool      castleQueenSide = false;
    bool      enPassant       = false;

    // The piece standing on the destination square before the move is made.
    // The AI uses this to try captures first (move ordering), which makes
    // alpha-beta pruning much more effective.
    Piece     captured_piece_hint;

    bool is_null() const { return fromRow == toRow && fromCol == toCol; }
};

// Castling rights are stored per colour, one flag per piece that can still
// castle. We use bit positions inside a single byte per colour.
enum CastlingRight
{
    CASTLE_WHITE_KING   = 1 << 0,
    CASTLE_WHITE_QUEEN  = 1 << 1,
    CASTLE_BLACK_KING   = 1 << 2,
    CASTLE_BLACK_QUEEN  = 1 << 3
};

// A snapshot of everything needed to undo a move. The engine keeps a stack of
// these, so "make_move" and "undo_move" are written only once and the AI can
// search freely by making and undoing moves.
struct MoveUndo
{
    Move      move;
    Piece     capturedPiece;       // piece that was on the destination square
    Piece     capturedPawn;        // pawn removed by an en passant capture
    int       capturedPawnRow;     // where that pawn was (-1 if none)
    int       capturedPawnCol;
    Color     previousSideToMove;
    int       previousCastlingRights;
    int       previousEnPassantTarget;
    int       previousHalfmoveClock;
    int       previousFullmoveNumber;
};

// ---------------------------------------------------------------------------
// The board.
//
// Board coordinates:
//   row 0 .. 7  ->  rank 1 .. rank 8   (rank 1 is White's home row)
//   col 0 .. 7  ->  file  a .. file h
//
// So the square "e4" is row 3, col 4.
// ---------------------------------------------------------------------------
class ChessBoard
{
public:
    ChessBoard();

    // Puts the pieces back in their starting positions.
    void reset();

    Piece at(int row, int col) const;

    // All legal moves for the side that is currently to move.
    std::vector<Move> legal_moves() const;

    // Legal moves for one specific square (empty vector if the piece there is
    // not the side to move, or if no legal move starts from that square).
    std::vector<Move> legal_moves_from(int row, int col) const;

    // True if the given square is attacked by any piece of the given colour.
    bool is_square_attacked(int row, int col, Color by) const;

    // Applies a move and pushes an undo record. Returns false if the move is
    // not legal (in that case nothing changes).
    bool make_move(const Move& move);

    // Reverses the most recent move.
    void undo_move(const Move& move);

    bool in_check(Color color) const;

    // Checkmate = in check AND there is no legal move.
    bool is_checkmate() const;

    // Stalemate = NOT in check AND there is no legal move.
    bool is_stalemate() const;

    // True when 50 moves have passed without a pawn move or a capture.
    bool is_fifty_move_draw() const;

    // True when the same position has occurred three times.
    //
    // The rules of chess say a game is drawn if the players reach the same
    // position three times. Without this rule, two computers playing each
    // other can shuffle the pieces back and forth for ever, because they
    // keep choosing moves that look exactly as good as each other.
    //
    // Two positions count as "the same" when the pieces are on the same
    // squares, it is the same side's turn, and the castling rights and the
    // en passant target are the same.
    bool is_threefold_repetition() const;

    Color side_to_move() const { return side_to_move_; }
    void  set_side_to_move(Color color) { side_to_move_ = color; }

    // The square a pawn can be captured on en passant, or (-1, -1) if none.
    // Stored as a row index only, since the file is always the mover's file.
    int en_passant_target() const { return en_passant_target_; }

    // Number of half-moves since the last pawn move or capture. Used by the
    // 50-move rule.
    int halfmove_clock() const { return halfmove_clock_; }

    // Castling rights as a bitmask of CastlingRight values.
    int castling_rights() const { return castling_rights_; }
    bool has_castling_right(int right) const { return (castling_rights_ & right) != 0; }

    // Human-readable name of a square, e.g. "e4". Useful for the console.
    static std::string square_name(int row, int col);

    // Prints the board as text, for the 'M' key. Great for demonstrating that
    // the rules engine and the 3D view agree with each other.
    std::string to_ascii() const;

    // A short description of the current position, for the AI evaluation.
    int count_material(Color color) const;

    // --- Test helpers -------------------------------------------------------
    // These are used by the self-test to build specific positions (such as a
    // stalemate) that would be awkward to reach through normal play.
    // They are public so the test code can use them, but a normal game never
    // calls them.

    // Removes every piece from the board.
    void clear_board();

    // Places a piece on a square given by name, e.g. place("e1", KING, WHITE).
    void place(const std::string& square, PieceType type, Color color);

    // Removes the castling rights of a colour (used when building custom
    // positions, where the rights would otherwise be misleading).
    void clear_castling_rights();

    // Turns the repetition tracking on or off.
    //
    // Tracking is on for a real game, but we turn it off for the AI's
    // search. The search makes and undoes millions of moves on a
    // trial-and-error basis, so it never follows one real line of play: a
    // repetition it "found" would be an artefact of the search rather than a
    // real draw. Turning the tracking off also saves the search the work of
    // maintaining the list at every node.
    void set_track_repetitions(bool track) { track_repetitions_ = track; }
    bool tracks_repetitions() const { return track_repetitions_; }

private:
    // Generates all pseudo-legal moves (ignores king safety) for one piece.
    void generate_pseudo_moves(int row, int col, std::vector<Move>& out) const;

    // Filters the pseudo-legal moves down to the legal ones.
    std::vector<Move> filter_legal_moves(std::vector<Move> candidates) const;

    // Helper used by move generation.
    void add_moves_in_direction(int row, int col, int dr, int dc, std::vector<Move>& out) const;
    void add_knight_moves(int row, int col, std::vector<Move>& out) const;
    void add_castling_moves(int row, int col, std::vector<Move>& out) const;

    // Position of the king of the given colour, or (-1,-1) if not found.
    void find_king(Color color, int& row, int& col) const;

    // A fingerprint of the current position, used to spot repetitions.
    //
    // It mixes together the piece placement, the side to move, the castling
    // rights and the en passant target, because two positions only count as
    // the same if all four match.
    std::uint64_t position_key() const;

    // Removes castling rights that are no longer valid.
    void update_castling_rights(const Move& move);

    Piece board_[8][8];
    Color side_to_move_;
    int  castling_rights_;
    int  en_passant_target_;      // row index, or -1 when there is no target
    int  halfmove_clock_;         // half-moves since last capture or pawn move
    int  fullmove_number_;
    std::vector<MoveUndo> history_;

    // The key of every position that has been reached in this game.
    //
    // We push a key whenever a move is made and pop it when the move is
    // undone, so the list always describes the real line of play. If the
    // current position's key appears three times, the game is drawn.
    std::vector<std::uint64_t> position_keys_;

    // Whether the keys above are being recorded. See set_track_repetitions.
    //
    // It is true by default, so a normal game tracks repetitions. The AI
    // turns it off on its private copy of the board.
    bool track_repetitions_ = true;
};

// Runs a set of assertions that check the rules engine behaves correctly.
// Returns true if every test passed. Prints a report to the console.
bool run_rules_self_test();
