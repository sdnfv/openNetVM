# Source: https://stackoverflow.com/questions/21956683/enable-access-control-on-simple-http-server

# PYTHON 2 ONLY

from SimpleHTTPServer import SimpleHTTPRequestHandler
import BaseHTTPServer

class CORSHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        SimpleHTTPRequestHandler.end_headers(self)

if __name__ == '__main__':
    BaseHTTPServer.test(CORSHandler, BaseHTTPServer.HTTPServer)
