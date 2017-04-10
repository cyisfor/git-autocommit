import numpy,math

def doit(*points):
	x = numpy.array([p[0] for p in points])
	y = numpy.array([p[1] for p in points])
	res = numpy.polyfit(x, numpy.log(y),1,w=numpy.sqrt(y))
	A = math.exp(res[0])
	print(A,"* exp(",res[1],"* x)")

"""
1 character = 3600s (don't commit)
		 60 characters = 60s to commit
		 600 characters means commit now.
		 (1,9000)
		 (60,60)
		 (600,0)
"""

doit((1,3600),
		 (60,60),
		 (600,1))
"""
1 word = 3600s
10 words = 60s
50 words = now
"""
doit((1,3600),
		 (10,60),
		 (50,1))

"""
0 lines = 3600s
1 line = 600s
5 lines = 60s
10 lines = now
"""
doit((1,600),
		 (5,60),
		 (10,1))
