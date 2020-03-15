import socket
import pydev

a = "Hello World from test!!!"

def pow(a, b=2):
    return a**b

class HttpClient(object):
    def __init__(self, host, port):
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._addr = (host, port)

    def get(self, url="/"):
        req = "GET %s HTTP/1.1\r\n\r\n" % url
        self._socket.connect(self._addr)
        self._socket.sendall(req)
        rsp = self._socket.recv(1024)
        self._socket.close()
        return rsp

title = None
def setTitle(text):
    global title
    title = text

    pydev.iointr("title", title)

test = 0
def setTest(val):
    global test
    test = val
    pydev.iointr("test", test)

if __name__ == "__main__":
    s = HttpClient("www.google.com", 80)
    print s.get("/")
    print s.get()
