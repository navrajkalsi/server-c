import socket
import time

def make_request(host="localhost", port=1419, path="/"):
    # Create a TCP socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, port))

        # Build HTTP/1.1 request
        request = f"GET {path} HTTP/1.1\r\n"
        request += f"Host: {host}\r\n"
        request += "User-Agent: PythonTestClient/1.0\r\n"
        request += "Accept: */*\r\n"
        request += "Connection: close\r\n\r\n"

        # Send request
        s.sendall(request.encode("utf-8"))

        # Receive response
        response = b""
        while True:
            data = s.recv(4096)
            if not data:
                break
            response += data

        # print(response.decode("utf-8", errors="replace"))

if __name__ == "__main__":
    numOfRequests = 1000
    start = time.perf_counter()
    for i in range(numOfRequests):
        make_request("localhost", 1419, "/main.c")
    end = time.perf_counter()

    print(f"\nTotal time to make {numOfRequests} requests: {(end-start)*1000:.2f}ms")

