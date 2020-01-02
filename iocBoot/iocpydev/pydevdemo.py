def fncA(a=None):
    if a is None:
        raise UserWarning("Parameter a not defined")

def fnc1():
    fncA()

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
