from pyapsi import APSIServer

if __name__ == "__main__":
    n_threads = 2
    port = 1234
    server = APSIServer(n_threads)
    server.init_db()
    server.add_item("item", "1234567890")
    server.add_item("meti", "0987654321")
    server.run(port)
