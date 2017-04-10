from sympy import *
#X, x1, y1, x2, y2, x3, y3 = symbols("X x1 y1 x2 y2 x3 y3")
x = Symbol("x")

"""	lagrange interpolation
		 given (x1,y1), (x2,y2), (x3,y3)
		 L(x) = y1 * (X - x2) / (x1 - x2) * (X - x3) / (x1 - x3)
		        +
						y2 * (X - x1) / (x2 - x1) * (X - x3) / (x2 - x3)
					  +
						y3 * (X - x2) / (x3 - x2) * (X - x1) / (x3 - x1)
					...group by X as much as possible
"""
def doit(*points):
	expr = 0
	for j in range(len(points)):
		l = points[j][1] # yj
		for m in range(len(points)):
			if m == j: continue
			l = l * (x - points[m][0]) / (points[j][0] - points[m][0]);
		expr = expr + l
	print(simplify(expr))

"""
1 character = 3600s (don't commit)
		 60 characters = 60s to commit
		 600 characters means commit now.
		 (1,9000)
		 (60,60)
		 (600,0)
"""

doit([1,3600],
		 [60,60],
		 [600,0])
"""
1 word = 3600s
10 words = 60s
50 words = now
"""
doit([1,3600],
		 [10,60],
		 [50,0])

"""
0 lines = 3600s
1 line = 600s
5 lines = 60s
10 lines = now
"""
doit([0,3600],
		 [1,600],
		 [5,60],
		 [10,0])
