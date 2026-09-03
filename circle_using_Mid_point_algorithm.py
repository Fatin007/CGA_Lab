import matplotlib.pyplot as plt

def midpoint_circle(xc, yc, r):
    x_points = []
    y_points = []

    x = 0
    y = r
    d = 1 - r

    def plot_points(x, y):
        # 8-way symmetry
        x_points.extend([xc+x, xc-x, xc+x, xc-x, xc+y, xc-y, xc+y, xc-y])
        y_points.extend([yc+y, yc+y, yc-y, yc-y, yc+x, yc+x, yc-x, yc-x])

    plot_points(x, y)

    while x < y:
        x += 1
        if d < 0:
            d += 2*x + 1
        else:
            y -= 1
            d += 2*(x - y) + 1
        plot_points(x, y)

    return x_points, y_points


# ---- Input ----
xc, yc = map(int, input("Enter center xc yc: ").split())
r = int(input("Enter radius r: "))

xs, ys = midpoint_circle(xc, yc, r)

print("Plotted points:")
for px, py in zip(xs, ys):
    print(f"({px}, {py})")

# ---- Plot ----
plt.figure(figsize=(6, 6))
plt.scatter(xs, ys, color='purple', s=20)
plt.title("Midpoint Circle Drawing Algorithm")
plt.xlabel("X axis")
plt.ylabel("Y axis")
plt.grid(True)
plt.axhline(0, color='black', linewidth=0.5)
plt.axvline(0, color='black', linewidth=0.5)
plt.gca().set_aspect('equal', adjustable='box')
plt.show()