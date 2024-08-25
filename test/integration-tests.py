import subprocess
import time
import requests
import os
import signal

def start_server():
    server_process = subprocess.Popen(["../build/multi-server", "4"])
    time.sleep(1)
    return server_process

def stop_server(server_process):
    server_process.terminate()
    try:
        server_process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        os.kill(server_process.pid, signal.SIGKILL)

def test_root():
    response = requests.get('http://127.0.0.1:7878/')
    assert response.status_code == 200
    assert 'Hello, world!' in response.text

def test_404():
    response = requests.get('http://127.0.0.1:7878/nonexistent')
    assert response.status_code == 404
    assert '404 Not Found' in response.text

def test_sleep():
    start_time = time.time()
    response = requests.get('http://127.0.0.1:7878/sleep')
    assert response.status_code == 200
    assert 'Hello, world!' in response.text
    assert time.time() - start_time >= 5

if __name__ == '__main__':
    server_process = start_server()

    try:
        print("Running Integration Tests...")
        test_root()
        test_404()
        test_sleep()
        print("All Integration Tests Passed!")
    finally:
        stop_server(server_process)