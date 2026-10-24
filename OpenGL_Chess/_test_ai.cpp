// _test_ai.cpp -- Checks that the computer opponent works and how fast it is.
#include "board.hpp"
#include "ai.hpp"
#include <iostream>
#include <chrono>

int main()
{
    // --- Test 1: the AI finds a mate in one ---
    {
        ChessBoard board;
        board.clear_board();
        board.clear_castling_rights();

        // A clean mate in one:
        //
        //   * the black king is on h8, blocked by its own pawn on h7
        //   * a white rook on g5 covers g7 and g8, so the king has no escape
        //   * the white queen on e1 plays Qe8, which is checkmate
        //
        // Black is NOT in check before the move, so this really is a
        // "mate in one" that the computer should find.
        board.place("h8", KING,  BLACK);
        board.place("h7", PAWN,  BLACK);
        board.place("g5", ROOK,  WHITE);
        board.place("e1", QUEEN, WHITE);
        board.place("a1", KING,  WHITE);   // every side needs a king
        board.set_side_to_move(WHITE);

        // Sanity check: playing Qe8 really is checkmate.
        Move queen_to_e8;
        queen_to_e8.fromRow = 0; queen_to_e8.fromCol = 4;   // e1
        queen_to_e8.toRow   = 7; queen_to_e8.toCol   = 4;   // e8
        board.make_move(queen_to_e8);
        const bool really_mate = board.is_checkmate();
        board.undo_move(queen_to_e8);

        const bool black_was_in_check = board.in_check(BLACK);

        std::cout << "  [INFO] Qe8 is checkmate: " << (really_mate ? "yes" : "no")
                  << ", black in check beforehand: "
                  << (black_was_in_check ? "yes" : "no") << "\n";

        ChessAI ai;
        ai.start_search(board, HARD);
        ai.wait_for_completion();

        const Move move = ai.result();

        // Several moves deliver mate here (the queen can reach the 8th rank
        // or the h8 diagonal in more than one way), and the AI deliberately
        // picks at random among equally good moves. So we do not check WHICH
        // mating move it chose -- we check that the move it chose really is
        // mate.
        ChessBoard after = board;
        const bool played = after.make_move(move);
        const bool is_mate = played && after.is_checkmate();

        std::cout << (is_mate
                      ? "  [PASS] the AI finds a mate in one ("
                      : "  [FAIL] the AI did not find a mate in one (");
        std::cout << ChessBoard::square_name(move.fromRow, move.fromCol)
                  << " -> " << ChessBoard::square_name(move.toRow, move.toCol) << ")\n";

        if (!is_mate)
        {
            std::cout << "         in_check=" << after.in_check(BLACK)
                      << " black_moves=" << after.legal_moves().size() << "\n";
        }
    }

    // --- Test 1b: the AI prefers the QUICKEST mate ---
    //
    // In the position above the queen can deliver mate in one (Qe8#) but it
    // can also set up a mate in two. The mate score is reduced for every ply
    // spent before the mate is delivered, so the AI must prefer the mate in
    // one.
    {
        ChessBoard board;
        board.clear_board();
        board.clear_castling_rights();
        board.place("h8", KING,  BLACK);
        board.place("h7", PAWN,  BLACK);
        board.place("g5", ROOK,  WHITE);
        board.place("e1", QUEEN, WHITE);
        board.place("a1", KING,  WHITE);
        board.set_side_to_move(WHITE);

        // Work out which moves mate immediately.
        std::vector<Move> mating_moves;
        for (const Move& m : board.legal_moves())
        {
            ChessBoard test = board;
            if (test.make_move(m) && test.is_checkmate())
                mating_moves.push_back(m);
        }

        // Run the search several times. Every single run must return one of
        // the mating moves -- if the AI ever settles for a slower mate the
        // depth-aware mate score is not working.
        int runs = 8;
        int wrong = 0;
        for (int i = 0; i < runs; ++i)
        {
            ChessAI ai;
            ai.start_search(board, HARD);
            ai.wait_for_completion();

            const Move move = ai.result();

            bool is_mating = false;
            for (const Move& m : mating_moves)
            {
                if (m.fromRow == move.fromRow && m.fromCol == move.fromCol &&
                    m.toRow   == move.toRow   && m.toCol   == move.toCol)
                {
                    is_mating = true;
                    break;
                }
            }
            if (!is_mating) ++wrong;
        }

        std::cout << "  [INFO] " << mating_moves.size()
                  << " mating moves exist; the AI was asked to pick one "
                  << runs << " times\n";

        if (wrong == 0)
        {
            std::cout << "  [PASS] the AI always picks the quickest available mate\n";
        }
        else
        {
            std::cout << "  [FAIL] the AI missed the mate in one in "
                      << wrong << " of " << runs << " runs\n";
        }
    }

    // --- Test 2: the AI plays a full game against itself ---
    //
    // This test plays a real game and checks three things:
    //
    //   1. every move the AI picks is legal (the board accepts it)
    //   2. both kings are still on the board the whole time
    //   3. the game reaches a result: checkmate, stalemate, a 50-move draw or
    //      a threefold repetition
    //
    // A Medium-vs-Medium game can easily take 150+ moves, because neither
    // side is trying to force a quick finish -- they just keep playing the
    // best move they can see. The threefold repetition rule is what
    // eventually stops the endless shuffling. We allow up to 600 plies.
    {
        ChessBoard board;
        board.reset();

        ChessAI ai;
        int plies = 0;
        bool illegal_move = false;
        bool king_lost = false;

        while (plies < 600)
        {
            if (board.is_checkmate() || board.is_stalemate() ||
                board.is_fifty_move_draw() || board.is_threefold_repetition())
                break;

            ai.start_search(board, MEDIUM);
            ai.wait_for_completion();

            const Move move = ai.result();
            if (move.is_null()) break;

            if (!board.make_move(move)) { illegal_move = true; break; }
            ++plies;

            // Both kings must still be alive. The rules engine refuses to
            // capture a king, so this can never happen -- but it is worth
            // checking, because a missing king would break the checkmate
            // detection that the rest of the game relies on.
            bool white_king = false, black_king = false;
            for (int r = 0; r < 8 && !(white_king && black_king); ++r)
            {
                for (int c = 0; c < 8; ++c)
                {
                    const Piece p = board.at(r, c);
                    if (p.type != KING) continue;
                    if (p.color == WHITE) white_king = true;
                    else                        black_king = true;
                }
            }
            if (!white_king || !black_king) { king_lost = true; break; }
        }

        const bool ended = board.is_checkmate() || board.is_stalemate() ||
                           board.is_fifty_move_draw() ||
                           board.is_threefold_repetition();

        std::cout << "  [INFO] a Medium-vs-Medium game ran for " << plies << " plies";
        if (board.is_checkmate())       std::cout << ", ending in checkmate";
        else if (board.is_stalemate())  std::cout << ", ending in stalemate";
        else if (board.is_fifty_move_draw()) std::cout << ", ending in a 50-move draw";
        else if (board.is_threefold_repetition()) std::cout << ", ending in a threefold repetition";
        std::cout << "\n";

        if (illegal_move)
            std::cout << "  [FAIL] the AI tried to play an illegal move\n";
        else if (king_lost)
            std::cout << "  [FAIL] a king went missing during the game\n";
        else if (!ended)
            std::cout << "  [FAIL] the game did not reach a result within 600 plies\n";
        else
            std::cout << "  [PASS] the AI plays a complete legal game that ends in a result\n";
    }

    // --- Test 3: how long does each difficulty take? ---
    {
        ChessBoard board;
        board.reset();

        const char* names[] = { "Easy", "Medium", "Hard" };
        const Difficulty levels[] = { EASY, MEDIUM, HARD };

        for (int i = 0; i < 3; ++i)
        {
            ChessAI ai;
            const auto start = std::chrono::steady_clock::now();
            ai.start_search(board, levels[i]);
            ai.wait_for_completion();
            const auto end = std::chrono::steady_clock::now();

            const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            std::cout << "  [INFO] " << names[i] << ": " << ms << " ms, "
                      << ai.positions_searched() << " positions searched\n";
        }
        std::cout << "  [PASS] all three difficulties completed a search\n";
    }

    return 0;
}
