// ai.hpp -- The computer opponent.
//
// The AI uses the classic minimax search with alpha-beta pruning. It is the
// same algorithm used in most simple chess programs and is easy to explain:
//
//   * minimax looks ahead N moves for both sides and picks the best result
//   * alpha-beta pruning throws away branches that cannot possibly be the best
//
// The evaluation function scores a position from White's point of view:
//     material value of the pieces  +  a bonus for well-placed pieces

#pragma once

#include "board.hpp"

#include <atomic>
#include <mutex>
#include <thread>

// How hard the computer plays.
enum Difficulty
{
    EASY   = 0,
    MEDIUM = 1,
    HARD   = 2
};

// Human-readable name of a difficulty level.
const char* difficulty_name(Difficulty difficulty);

// ---------------------------------------------------------------------------
// The search engine.
//
// The AI runs on its own thread so that the window stays responsive while it
// is thinking. The main loop copies the board, hands it to the worker and
// polls for the result.
// ---------------------------------------------------------------------------
class ChessAI
{
public:
    ChessAI() = default;
    ~ChessAI();

    // Non-copyable: it owns a thread.
    ChessAI(const ChessAI&) = delete;
    ChessAI& operator=(const ChessAI&) = delete;

    // Starts thinking about the given position on a separate thread.
    //
    // The board is copied, so the worker thread never touches the board that
    // the renderer is drawing.
    void start_search(const ChessBoard& board, Difficulty difficulty);

    // True once the search has finished.
    bool is_search_complete() const { return search_complete_.load(); }

    // The move the search decided on. Only valid once is_search_complete() is
    // true.
    Move result() const;

    // Asks the search to stop early (used when the game is reset or closed).
    void cancel();

    // Waits for the worker thread to finish.
    void wait_for_completion();

    // How many positions the search looked at. Handy for the console output.
    int positions_searched() const { return positions_searched_.load(); }

private:
    // The main search function.
    //
    //   depth            how many plies are still left to look ahead
    //   plies_from_root  how many plies have been played since the root move
    //   alpha            the best score the maximiser can already guarantee
    //   beta             the best score the minimiser can already guarantee
    //   white_to_move    whose turn it is at this node
    //
    // plies_from_root is used to score mates: a mate delivered in fewer moves
    // is scored higher than the same mate delivered later.
    int minimax(ChessBoard& board, int depth, int plies_from_root,
                int alpha, int beta, bool white_to_move);

    // Scores the position from White's point of view.
    int evaluate(const ChessBoard& board) const;

    // Runs on the worker thread.
    void search_worker(ChessBoard board, Difficulty difficulty);

    // The state shared between the main thread and the worker thread.
    std::thread worker_;
    mutable std::mutex result_mutex_;

    Move            result_move_;
    std::atomic<bool> search_complete_;
    std::atomic<bool> cancel_requested_;
    std::atomic<int>  positions_searched_;
};
