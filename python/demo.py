import random

def fncA(a=None):
    if a is None:
        raise UserWarning("Parameter a not defined")

def helloworld(a):
    print "Happy New Year 2020!!!"
    fnc1()

def writeAo(a):
    print "writeAo(",a,")"
def writeLongout(a):
    print "writeLongout(",a,")"
def writeBo(a):
    print "writeBo(",a,")"
def writeMbbo(a):
    print "writeMbbo(",a,")"
def writeStringout(a):
    print "writeStringout(",a,")"

def readLongin():
    print "readLongin() =>", num
    return num

def readAi():
    num = random.randint(1, 100)/100.0
    print "readAi() =>"
    return num
def readLongin():
    num = random.randint(1, 100)
    print "readLongin() =>", num
    return num
def readBi():
    num = random.randint(0, 1)
    print "readBi() =>", num
    return num
def readMbbi():
    num = random.randint(0, 15)
    print "readMbbi() =>", num
    return num
def readStringin():
    num = "Random:" + str(random.randint(1, 100))
    print "readStringin() =>", num
    return num
