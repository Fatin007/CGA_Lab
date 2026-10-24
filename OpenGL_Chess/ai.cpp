// ai.cpp -- Minimax search with alpha-beta pruning.

#include "ai.hpp"

#include <algorithm>
#include <random>
#include <iostream>
#include <bits/stdc++.h>

// ---------------------------------------------------------------------------
// Piece values.
//
// These are the traditional values, roughly in "centipawns" (1/100 of a pawn).
// The king is worth far more than everything else because losing it means
// losing the game.
// ---------------------------------------------------------------------------
namespace
{
    int piece_value(PieceType type)
    {
        switch (type)
        {
        case PAWN:   return 100;
        case KNIGHT: return 320;
        case BISHOP: return 330;
        case ROOK:   return 500;
        case QUEEN:  return 900;
        case KING:   return 20000;
        default:     return 0;
        }
    }

    // -----------------------------------------------------------------------
    // Piece-square tables.
    //
    // Instead of the usual table of 64 numbers for each piece type, we use a
    // small 4x4 table and mirror it. The index is:
    //
    //     min(rank, 7 - rank) * 4 + min(file, 7 - file)
    //
    // This keeps the table short enough to understand at a glance, while still
    // capturing the two ideas that matter: keep the king at home, and move
    // your other pieces towards the centre.
    //
    // The tables are written for White. Black's score is negated.
    // -----------------------------------------------------------------------
    const int PAWN_TABLE[16] = {
        //  a8-b8  c8-d8  e8-f8  g8-h8
           0,   0,   0,   0,     // rank 8 (and rank 1 by mirroring)
           5,  10,  10,   5,     // rank 7 (and rank 2)
           1,   3,   3,   1,     // rank 6 (and rank 3)
           0,   0,   0,   0      // rank 5 (and rank 4)
    };

    const int KNIGHT_TABLE[16] = {
        -5,  -5,  -5,  -5,
        -3,   0,   5,  -3,
        -1,   3,   5,  -1,
         0,   2,   0,   0
    };

    const int BISHOP_TABLE[16] = {
        -5,  -5,  -5,  -5,
        -2,   0,   3,  -2,
         0,   2,   4,   0,
        -2,   1,   1,  -2
    };

    const int ROOK_TABLE[16] = {
         0,   2,   2,   0,
        -2,   0,   0,  -2,
        -2,   0,   0,  -2,
        -4,  -2,  -2,  -4
    };

    const int QUEEN_TABLE[16] = {
        -5,  -5,  -5,  -5,
        -2,  -1,  -1,  -2,
        -1,   0,   2,  -1,
        -2,   2,   3,  -2
    };

    const int KING_TABLE[16] = {
         8,  10,  10,   8,
         6,   8,   8,   6,
         2,   4,   4,   2,
         0,   0,   0,   0
    };

    // Looks up the bonus for a piece standing on a given square.
    int piece_square_bonus(PieceType type, int row, int col)
    {
        // Mirror the square into the lower-left quadrant.
        int rank = std::min(row, 7 - row);
        int file = std::min(col, 7 - col);
        int index = rank * 4 + file;

        switch (type)
        {
        case PAWN:   return PAWN_TABLE[index];
        case KNIGHT: return KNIGHT_TABLE[index];
        case BISHOP: return BISHOP_TABLE[index];
        case ROOK:   return ROOK_TABLE[index];
        case QUEEN:  return QUEEN_TABLE[index];
        case KING:   return KING_TABLE[index];
        default:     return 0;
        }
    }

    // --- Mate scoring ---
    //
    // A checkmate must always beat any non-mate position, so it gets a huge
    // score. But a huge FLAT score has a flaw: it cannot tell a mate in one
    // apart from a mate in two. The AI would happily give up a mate in one
    // for a mate in two, because both are worth exactly 1000000.
    //
    // The fix is to make the mate score depend on how quickly the mate is
    // delivered. We subtract a penalty for every ply already played since the
    // root move, so a mate found now is worth more than the same mate found a
    // move later.
    //
    // The penalty is small enough that every mate still beats every
    // non-mate score (the biggest possible material score is far below
    // MATE_SCORE), but big enough that "mate next move" beats "mate in two".
    //
    // The penalty must also be larger than any material swing we could care
    // about, otherwise a mate in two might lose to a mate in three that
    // happens to win a queen on the way. A queen is worth 900, so 10000
    // comfortably covers "a whole queen" and still leaves every mate score
    // far above any ordinary position.
    const int MATE_SCORE    = 1000000;
    const int MATE_PENALTY  = 10000;

    // How many moves the AI looks ahead for each difficulty.
    int search_depth(Difficulty difficulty)
    {
        switch (difficulty)
        {
        case EASY:   return 1;
        case MEDIUM: return 3;
        case HARD:   return 4;
        default:     return 2;
        }
    }
}

const char* difficulty_name(Difficulty difficulty)
{
    switch (difficulty)
    {
    case EASY:   return "Easy";
    case MEDIUM: return "Medium";
    case HARD:   return "Hard";
    default:     return "Unknown";
    }
}

// ---------------------------------------------------------------------------
// Evaluation.
//
// The score is always from White's point of view: a positive score means
// White is better off.
// ---------------------------------------------------------------------------
int ChessAI::evaluate(const ChessBoard& board) const
{
    int score = 0;

    for (int row = 0; row < 8; ++row)
    {
        for (int col = 0; col < 8; ++col)
        {
            const Piece piece = board.at(row, col);
            if (piece.is_empty()) continue;

            // The table is written for White, so Black's bonus is negated.
            int sign = (piece.color == WHITE) ? 1 : -1;

            score += sign * (piece_value(piece.type) +
                             piece_square_bonus(piece.type, row, col));
        }
    }

    return score;
}

// ---------------------------------------------------------------------------
// Minimax with alpha-beta pruning.
//
//   depth         how many moves to look ahead
//   alpha         the best score the player who is maximising can already guarantee
//   beta          the best score the player who is minimising can already guarantee
//   white_to_move whose turn it is at this node
//
// Alpha-beta pruning works like this: if we already know a move is worse than
// something the other side has, there is no point exploring it further.
// ---------------------------------------------------------------------------
int ChessAI::minimax(ChessBoard& board, int depth, int plies_from_root,
                      int alpha, int beta, bool white_to_move)
{
    // Let the caller stop the search early if it wants to (for example when
    // the player restarts the game).
    if (cancel_requested_.load())
        return 0;

    positions_searched_++;

    // --- Terminal cases ---

    // Checkmate: the side to move loses.
    //
    // The score is huge so the search prefers a mate over anything else, and
    // it is reduced by the number of plies that have been played since the
    // root move. That way a mate delivered right now beats the same mate
    // delivered one move later, and the AI goes for the quickest kill.
    //
    // (The penalty has to be measured from the ROOT, not from the remaining
    // depth: the remaining depth shrinks as we go deeper, so using it here
    // would reward mates that are found late.)
    if (board.is_checkmate())
    {
        const int mate_score = MATE_SCORE - plies_from_root * MATE_PENALTY;
        return white_to_move ? -mate_score : mate_score;
    }

    // Stalemate: nobody wins, so the score is simply zero.
    if (board.is_stalemate())
        return 0;

    // The 50-move rule: the game is drawn.
    if (board.is_fifty_move_draw())
        return 0;

    // We deliberately do NOT check for a threefold repetition here.
    //
    // The search makes and undoes millions of moves on a trial-and-error
    // basis, so it never follows one real line of play -- a repetition it
    // "found" would be an artefact of the search, not a real draw. Checking
    // would also cost time at every node for no benefit.
    //
    // The repetition rule is applied in the real game, in main.cpp.

    // We have reached the depth limit, so we judge the position by its
    // material and piece placement.
    if (depth <= 0)
        return evaluate(board);

    // --- Recursive step ---

    std::vector<Move> moves = board.legal_moves();

    // No moves means checkmate or stalemate, both handled above.
    if (moves.empty())
        return evaluate(board);

    // Move ordering: try the captures first, and among the captures take the
    // most valuable victim first. This makes alpha-beta pruning far more
    // effective, because good moves are searched early and bad branches get
    // cut off sooner.
    //
    // The score is "victim value - attacker value", which is the standard
    // MVV-LVA idea: taking a queen with a pawn is a good trade, but losing a
    // queen for a pawn is not.
    auto move_priority = [](const Move& move)
    {
        int score = 0;

        // Captures are tried first, and the more valuable the victim, the
        // earlier the move is tried.
        if (!move.captured_piece_hint.is_empty())
            score += piece_value(move.captured_piece_hint.type) * 10;

        // Promotions are very valuable, so try them early too.
        if (move.promotion == QUEEN)  score += 500;
        if (move.promotion == ROOK)   score += 300;
        if (move.promotion == BISHOP) score += 200;
        if (move.promotion == KNIGHT) score += 150;

        return score;
    };

    std::stable_sort(moves.begin(), moves.end(),
                     [&](const Move& a, const Move& b)
                     {
                         return move_priority(a) > move_priority(b);
                     });

    int best_score = white_to_move ? -MATE_SCORE : MATE_SCORE;

    for (const Move& move : moves)
    {
        // make_move refuses any move that would capture a king, because that
        // must never happen in a real game. If it refuses, we skip the move
        // and do NOT undo it (nothing was pushed onto the undo stack).
        if (!board.make_move(move))
            continue;

        // After our move it is the other side's turn, and we search one ply
        // deeper with the opposite goal.
        const int score = minimax(board, depth - 1, plies_from_root + 1,
                                  alpha, beta, !white_to_move);

        board.undo_move(move);

        if (white_to_move)
        {
            if (score > best_score) best_score = score;
            if (best_score > alpha)   alpha = best_score;
        }
        else
        {
            if (score < best_score) best_score = score;
            if (best_score < beta)    beta = best_score;
        }

        // Alpha-beta cut-off: the opponent has already found a move that is
        // better than anything we could find here, so we stop.
        if (beta <= alpha)
            break;
    }

    return best_score;
}

// ---------------------------------------------------------------------------
// Threading.
// ---------------------------------------------------------------------------
ChessAI::~ChessAI()
{
    cancel();
    wait_for_completion();
}

void ChessAI::start_search(const ChessBoard& board, Difficulty difficulty)
{
    // If a search is already running, stop it first.
    cancel();
    wait_for_completion();

    cancel_requested_.store(false);
    search_complete_.store(false);
    positions_searched_.store(0);

    // The worker gets its own copy of the board, so it never touches the one
    // the renderer is drawing.
    worker_ = std::thread(&ChessAI::search_worker, this, board, difficulty);
}

void ChessAI::cancel()
{
    cancel_requested_.store(true);
}

void ChessAI::wait_for_completion()
{
    if (worker_.joinable())
        worker_.join();
}

void ChessAI::search_worker(ChessBoard board, Difficulty difficulty)
{
    // This is our private copy of the board, and the search makes and undoes
    // millions of moves on it. Tracking repetitions would be pointless (the
    // search never follows one real line of play) and would slow the search
    // down, so we switch it off.
    board.set_track_repetitions(false);

    const int depth = search_depth(difficulty);

    std::vector<Move> moves = board.legal_moves();

    Move best_move;
    if (!moves.empty())
    {
        // The side to move is the one we are choosing a move for.
        const bool white_to_move = (board.side_to_move() == WHITE);

        // On Easy we do not always pick the very best move. We accept any move
        // that is within this margin of the best score, which makes the
        // computer beatable.
        const int sloppiness = (difficulty == EASY) ? 150 : 0;

        // Score every move once and remember the results.
        //
        // We keep the moves and their scores together in one list, because a
        // move can be skipped (see below) and the two lists would then get
        // out of step with each other.
        std::vector<std::pair<Move, int>> scored;
        scored.reserve(moves.size());

        int best_score = white_to_move ? -MATE_SCORE : MATE_SCORE;

        for (const Move& move : moves)
        {
            if (cancel_requested_.load())
                break;

            // make_move refuses any move that would capture a king. If it
            // refuses, the move is skipped (and not undone).
            if (!board.make_move(move))
                continue;

            // The root move itself is ply 1, so a mate delivered by this move
            // is scored with plies_from_root = 1.
            const int score = minimax(board, depth - 1, 1,
                                      -MATE_SCORE, MATE_SCORE, !white_to_move);
            board.undo_move(move);

            scored.push_back(std::make_pair(move, score));

            if (white_to_move)
            {
                if (score > best_score) best_score = score;
            }
            else
            {
                if (score < best_score) best_score = score;
            }
        }

        // Collect the moves that are good enough.
        std::vector<Move> candidates;
        for (const auto& entry : scored)
        {
            const Move& move = entry.first;
            const int score  = entry.second;

            const bool good = white_to_move ? (score >= best_score - sloppiness)
                                            : (score <= best_score + sloppiness);
            if (good)
                candidates.push_back(move);
        }

        if (candidates.empty())
        {
            // Fall back to every move we actually managed to search. (We
            // cannot fall back to the full list, because some of those moves
            // were rejected by make_move and would be illegal to play.)
            for (const auto& entry : scored)
                candidates.push_back(entry.first);
        }

        if (candidates.empty())
        {
            // No move could be searched at all. This should never happen,
            // but we must not pick from an empty list.
            best_move = Move();
        }
        else
        {
            // Choose one of them at random, so the computer does not always
            // play the same game.
            static std::random_device random_device;
            static std::mt19937 random_engine(random_device());
            std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
            best_move = candidates[pick(random_engine)];
        }
    }

    // Publish the result under the lock.
    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        result_move_ = best_move;
    }

    search_complete_.store(true);
}

Move ChessAI::result() const
{
    std::lock_guard<std::mutex> lock(result_mutex_);
    return result_move_;
}
