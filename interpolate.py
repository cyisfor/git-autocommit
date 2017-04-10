from sympy import *
X, x1, y1, x2, y2, x3, y3 = symbols("X x1 y1 x2 y2 x3 y3")
print(simplify(y1 * (X - x2) / (x1 - x2) * (X - x3) / (x1 - x3)
							 +
							 y2 * (X - x1) / (x2 - x1) * (X - x3) / (x2 - x3)
							 +
							 y3 * (X - x2) / (x3 - x2) * (X - x1) / (x3 - x1)))
