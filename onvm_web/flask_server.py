#!usr/bin/env python3
"""This is the python server that handles onvm web request"""

from flask import Flask, request
from flask_cors import CORS

app = Flask(__name__)
CORS(app)

@app.route('/upload-file', method=['POST'])
def handle_upload_file():
    """Handle upload file"""
    data = request.form
    print(data)

if __name__ == "__main__":
    app.run(host='localhost', port=8000, debug=False)
