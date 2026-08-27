import matplotlib.pyplot as plt

def midpoint_line(x1, y1, x2, y2):
    x_points = []
    y_points = []

    dx = x2 - x1
    dy = y2 - y1

    # Determine direction of increment for x and y
    x_step = 1 if dx >= 0 else -1
    y_step = 1 if dy >= 0 else -1

    dx = abs(dx)
    dy = abs(dy)

    x, y = x1, y1
    x_points.append(x)
    y_points.append(y)

    if dx > dy:
        # Slope <= 1: driving axis is X
        d = 2 * dy - dx
        delta_E = 2 * dy
        delta_NE = 2 * (dy - dx)

        for _ in range(dx):
            if d <= 0:
                d += delta_E
            else:
                d += delta_NE
                y += y_step
            x += x_step
            x_points.append(x)
            y_points.append(y)
    else:
        # Slope > 1: driving axis is Y
        d = 2 * dx - dy
        delta_N = 2 * dx
        delta_NE = 2 * (dx - dy)

        for _ in range(dy):
            if d <= 0:
                d += delta_N
            else:
                d += delta_NE
                x += x_step
            y += y_step
            x_points.append(x)
            y_points.append(y)

    return x_points, y_points


# ---- Input ----
x1, y1 = map(int, input("Enter start point x1 y1: ").split())
x2, y2 = map(int, input("Enter end point x2 y2: ").split())

xs, ys = midpoint_line(x1, y1, x2, y2)

print("Plotted points:")
for px, py in zip(xs, ys):
    print(f"({px}, {py})")

# ---- Plot ----
plt.figure(figsize=(6, 6))
plt.plot(xs, ys, marker='s', color='green', markersize=6, linestyle='-')
plt.title("Midpoint Line Drawing Algorithm")
plt.xlabel("X axis")
plt.ylabel("Y axis")
plt.grid(True)
plt.axhline(0, color='black', linewidth=0.5)
plt.axvline(0, color='black', linewidth=0.5)
plt.show()