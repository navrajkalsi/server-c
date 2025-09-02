import requests

r = requests.get("localhost:1419")
print(r.text)
