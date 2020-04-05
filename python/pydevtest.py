import socket

class HttpClient(object):
    def __init__(self, host, port=80):
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._addr = (host, port)

    def get(self, url="/"):
        req = "GET %s HTTP/1.1\r\n\r\n" % url
        self._socket.connect(self._addr)
        self._socket.sendall(req)
        rsp = self._socket.recv(1024)
        self._socket.close()
        try:
            proto, code, msg = rsp.split("\r\n")[0].split(" ")
            pydev.iointr('proto', proto)
            pydev.iointr('code',  int(code))
            pydev.iointr('msg',   msg)
        except:
            pass
        return rsp


if __name__ == "__main__":
    class pydev:
        @staticmethod
        def iointr(name, value):
            print("pydev.iointr('%s', '%s')" % (name, value))
    google = HttpClient("www.google.com")
    rsp = google.get()
    print("")
    print(rsp)

