from pyapsi import APSIClient

if __name__ == "__main__":
    server_address = "tcp://localhost:1234"
    n_threads = 2
    client = APSIClient()
    client.query(server_address, "meti", n_threads)
    client.query(server_address, "noex", n_threads)
