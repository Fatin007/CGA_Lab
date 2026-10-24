// _test_pick.cpp -- Verifies the board layout and the mouse-picking maths.
//
// The picking maths is the part of the renderer that is hard to eyeball, so
// we test it without a window: we project known world points to screen
// coordinates, then unproject those screen coordinates again and check that
// we land back on the same square.

#include "render.hpp"
#include <iostream>
#include <cmath>

int main()
{
    // --- Test 1: board layout round trip ---
    {
        bool ok = true;
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                for (int flipped = 0; flipped < 2; ++flipped) {
                    math3d::Vec3 w = board_layout::square_to_world(row, col, flipped != 0);
                    int r2 = -1, c2 = -1;
                    if (!board_layout::world_to_square(w, flipped != 0, r2, c2) || r2 != row || c2 != col) {
                        std::cout << "  FAIL roundtrip (" << row << "," << col << ") flip=" << flipped
                                  << " -> (" << r2 << "," << c2 << ")\n";
                        ok = false;
                    }
                }
            }
        }
        std::cout << (ok ? "  [PASS] board layout round trips for all 64 squares (both orientations)\n"
                         : "  [FAIL] board layout round trip\n");
    }

    // --- Test 2: known world positions ---
    {
        math3d::Vec3 a1 = board_layout::square_to_world(0, 0, false);
        math3d::Vec3 h8 = board_layout::square_to_world(7, 7, false);
        math3d::Vec3 e4 = board_layout::square_to_world(3, 4, false);   // row 3 = rank 4
        bool ok = std::fabs(a1.x + 3.5f) < 1e-5 && std::fabs(a1.z + 3.5f) < 1e-5 &&
                  std::fabs(h8.x - 3.5f) < 1e-5 && std::fabs(h8.z - 3.5f) < 1e-5 &&
                  std::fabs(e4.x - 0.5f) < 1e-5 && std::fabs(e4.z + 0.5f) < 1e-5;
        std::cout << (ok ? "  [PASS] a1=(-3.5,-3.5) h8=(3.5,3.5) e4=(0.5,-0.5)\n"
                         : "  [FAIL] known world positions\n");

        // Flipped: a1 should now be at z = +3.5
        math3d::Vec3 a1f = board_layout::square_to_world(0, 0, true);
        bool okf = std::fabs(a1f.z - 3.5f) < 1e-5;
        std::cout << (okf ? "  [PASS] flipping mirrors the z axis\n"
                          : "  [FAIL] flipping\n");
    }

    // --- Test 3: off-board positions are rejected ---
    {
        int r = -1, c = -1;
        bool inside  = board_layout::world_to_square(math3d::Vec3(0,0,0), false, r, c);
        bool outside = board_layout::world_to_square(math3d::Vec3(10,0,0), false, r, c);
        std::cout << ((inside && !outside) ? "  [PASS] off-board positions are rejected\n"
                                           : "  [FAIL] off-board rejection\n");
    }

    // --- Test 4: picking round trip ---
    //
    // The camera looks at the board from an angle. We pick a screen pixel,
    // build the ray the same way the renderer does, and intersect it with the
    // board plane. The picked square must actually CONTAIN the hit point.
    //
    // We also project the centre of that square back to the screen and check
    // that it lands in the same place the click started, which proves the
    // forward and backward directions agree.
    {
        Camera camera;
        camera.yaw = 40.0f;
        camera.pitch = 55.0f;
        camera.distance = 11.0f;

        const int W = 1000, H = 800;
        const float aspect = (float)W / (float)H;
        const math3d::Mat4 proj = math3d::perspective(45.0f, aspect, 0.1f, 100.0f);
        const math3d::Mat4 view = camera.view_matrix();
        const math3d::Vec3 eye = camera.eye_position();

        int contains_ok = 0, contains_bad = 0;
        int roundtrip_ok = 0, roundtrip_bad = 0;

        for (int px = 100; px < W; px += 25) {
            for (int py = 100; py < H; py += 25) {
                // Unproject the cursor onto the near and far planes.
                math3d::Vec3 near_p, far_p;
                if (!math3d::unproject_from_screen(proj, view, W, H,
                                                   (float)px, (float)py, -1.0f, near_p)) continue;
                if (!math3d::unproject_from_screen(proj, view, W, H,
                                                   (float)px, (float)py,  1.0f, far_p)) continue;

                const math3d::Vec3 dir = (far_p - near_p).normalized();

                // Intersect with the board plane (y = 0).
                if (std::fabs(dir.y) < 1e-6f) continue;
                const float t = -eye.y / dir.y;
                if (t < 0.0f) continue;
                const math3d::Vec3 hit = eye + dir * t;

                int r = -1, c = -1;
                if (!board_layout::world_to_square(hit, false, r, c)) continue;

                // --- Check 1: the picked square really contains the hit ---
                //
                // The square (r, c) covers world x from (c - 4) to (c - 3)
                // and world z from (r - 4) to (r - 3). The hit may sit
                // exactly on a border, so we allow a tiny epsilon.
                const float eps = 1e-3f;
                const float lo_x = static_cast<float>(c) - 4.0f - eps;
                const float hi_x = static_cast<float>(c) - 3.0f + eps;
                const float lo_z = static_cast<float>(r) - 4.0f - eps;
                const float hi_z = static_cast<float>(r) - 3.0f + eps;

                if (hit.x >= lo_x && hit.x <= hi_x &&
                    hit.z >= lo_z && hit.z <= hi_z)
                    contains_ok++;
                else
                    contains_bad++;

                // --- Check 2: projecting the square centre comes back ---
                //
                // The centre of a square is up to half a square away from
                // the pixel that picked it, so we allow a generous slack.
                const math3d::Vec3 centre = board_layout::square_to_world(r, c, false);
                float sx = 0.0f, sy = 0.0f;
                if (!math3d::project_to_screen(proj, view, centre, W, H, sx, sy)) continue;

                if (std::fabs(sx - (float)px) < 100.0f && std::fabs(sy - (float)py) < 100.0f)
                    roundtrip_ok++;
                else
                    roundtrip_bad++;
            }
        }

        const bool contains_ok_all = (contains_bad == 0 && contains_ok > 100);
        const bool roundtrip_ok_all = (roundtrip_bad == 0 && roundtrip_ok > 100);

        std::cout << (contains_ok_all
                      ? "  [PASS] every picked square contains its hit point (" + std::to_string(contains_ok) + " checks)\n"
                      : "  [FAIL] picked square does not contain hit point (ok=" + std::to_string(contains_ok) + " bad=" + std::to_string(contains_bad) + ")\n");

        std::cout << (roundtrip_ok_all
                      ? "  [PASS] picking round trips: " + std::to_string(roundtrip_ok) + " screen points map to the right square\n"
                      : "  [FAIL] picking round trip (ok=" + std::to_string(roundtrip_ok) + " bad=" + std::to_string(roundtrip_bad) + ")\n");
    }

    // --- Test 5: project_to_screen places the board centre near the middle ---
    {
        Camera camera;
        camera.yaw = 40.0f;
        camera.pitch = 55.0f;
        camera.distance = 11.0f;

        const int W = 1000, H = 800;
        const math3d::Mat4 proj = math3d::perspective(45.0f, (float)W / (float)H, 0.1f, 100.0f);
        const math3d::Mat4 view = camera.view_matrix();

        float sx = 0.0f, sy = 0.0f;
        const bool ok = math3d::project_to_screen(proj, view, math3d::Vec3(0, 0, 0), W, H, sx, sy);
        const bool centre_ok = ok && std::fabs(sx - W * 0.5f) < 1.0f && std::fabs(sy - H * 0.5f) < 1.0f;
        std::cout << (centre_ok ? "  [PASS] board centre projects to the middle of the window\n"
                                : "  [FAIL] board centre projection\n");
    }

    return 0;
}
