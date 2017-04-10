from sympy import *
#X, x1, y1, x2, y2, x3, y3 = symbols("X x1 y1 x2 y2 x3 y3")
X = Symbol("x")

x1 = 1
y1 = 9000
x2 = 60
y2 = 60
x3 = 600
y3 = 0

def doit(points):
	expr = 0
	for j in range(len(points)):
		l = points[j][1] # yj
		for m in range(len(points)):
			if m == j: continue
			l = l * (x - points[m][0]) / (points[j][0] - points[m][0]);
		expr = expr + l
	print(simplify(expr))

doit([1,9000],
		 [60,60],
		 [600,0])
