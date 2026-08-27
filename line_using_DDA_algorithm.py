import matplotlib.pyplot as plt

def dda_line(x1, y1, x2, y2):
    dx = x2 - x1
    dy = y2 - y1

    steps = max(abs(dx), abs(dy))

    x_inc = dx / steps
    y_inc = dy / steps

    x, y = x1, y1
    x_points = []
    y_points = []

    for _ in range(steps + 1):
        x_points.append(round(x))
        y_points.append(round(y))
        x += x_inc
        y += y_inc

    return x_points, y_points


# ---- Input ----
x1, y1 = map(int, input("Enter start point x1 y1: ").split())
x2, y2 = map(int, input("Enter end point x2 y2: ").split())

xs, ys = dda_line(x1, y1, x2, y2)

print("Plotted points:")
for px, py in zip(xs, ys):
    print(f"({px}, {py})")

# ---- Plot ----
plt.figure(figsize=(6, 6))
plt.plot(xs, ys, marker='s', color='blue', markersize=6, linestyle='-')
plt.title("DDA Line Drawing Algorithm")
plt.xlabel("X axis")
plt.ylabel("Y axis")
plt.grid(True)
plt.axhline(0, color='black', linewidth=0.5)
plt.axvline(0, color='black', linewidth=0.5)
plt.show()