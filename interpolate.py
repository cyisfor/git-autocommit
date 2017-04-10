from sympy.polys.polyfuncs import interpolate
from sympy import simplify, factor
from sympy.abc import x

def doit(*points):
	print(factor(interpolate(list(points),x)))

"""
1 character = 3600s (don't commit)
		 60 characters = 60s to commit
		 600 characters means commit now.
		 (1,9000)
		 (60,60)
		 (600,0)
"""

doit((1,3600),

		 (600,0))
"""
1 word = 3600s
10 words = 60s
50 words = now
"""
doit((1,3600),
		 (10,60),
		 (50,0))

"""
0 lines = 3600s
1 line = 600s
5 lines = 60s
10 lines = now
"""
doit((1,600),
		 (5,60),
		 (10,0))
