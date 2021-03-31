import sys

def get_mb():
	return "A" * (512<<10)	# 512 KB


data = ""

i=0
while True:
    data += get_mb()
    i += 1
    print "%d KB, %d MB" % (sys.getsizeof(data)/(1<<10), sys.getsizeof(data)/(1<<20))


