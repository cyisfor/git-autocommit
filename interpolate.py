from sympy import *
X, x1, y1, x2, y2, x3, y3 = symbols("X x1 y1 x2 y2 x3 y3")
expr = (y1 * (X - x2) / (x1 - x2) * (X - x3) / (x1 - x3)
				+
				y2 * (X - x1) / (x2 - x1) * (X - x3) / (x2 - x3)
				+
				y3 * (X - x2) / (x3 - x2) * (X - x1) / (x3 - x1))

print(latex(expand(expr)))

def lessX(expr):
	# every operation on X costs 10
	c = count_ops(expr,visual=True).subs(X,10000)
	# every other operation costs 1
	c = c.replace(Symbol, type(S.One))
	return c

#print(simplify(expr, measure=lessX))
