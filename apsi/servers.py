from typing import Iterable, Tuple

from _pyapsi import APSIServer as _Server


class _BaseServer(_Server):
    db_initialized: bool = False

    @property
    def label_byte_count(self) -> int:
        return self.db_label_byte_count

    def _requires_db(self):
        if not self.db_initialized:
            raise RuntimeError("Please initialize or load a database first.")

    def save_db(self, db_file_path: str) -> None:
        """Save the database in unencrypted binary representation at the given file
        path.
        """
        self._requires_db()
        self._save_db(db_file_path)

    def load_db(self, db_file_path: str) -> None:
        """Load a previously saved binary database representation into memory."""
        self._load_db(db_file_path)
        self.db_initialized = True

    def handle_oprf_request(self, oprf_request: bytes) -> bytes:
        """Handle an initial APSI Client OPRF request and return a compatible bytes
        response that can be used by the client to create the main query.
        """
        self._requires_db()
        return self._handle_oprf_request(oprf_request)

    def handle_query(self, query: bytes) -> bytes:
        """Handle an APSI Client query following up on an initial OPRF request and
        return the encrypted query response in an APSI Client compatible byte string.
        """
        self._requires_db()
        return self._handle_query(query)


class LabeledServer(_BaseServer):
    def __init__(self):
        """A server for labeled asynchronous private set intersection (APSI).

        For this server to do something meaningful, initialize an empty database with
        `init_db` or load an existing one with `load_db`.
        """
        super().__init__()

    def init_db(
        self,
        params_json: str,
        label_byte_count: int,
        nonce_byte_count: int = 16,
        compressed: bool = False,
    ) -> None:
        """Initialize an empty database with the specified configuration.

        Args:
            params_json: The JSON string representation of APSI/SEAL parameters
            label_byte_count: The label size in bytes
                TODO: Clarify if that's the max label size or if they all need to be it
            nonce_byte_count: The nonce size in bytes; For more details see
                https://github.com/microsoft/apsi#label-encryption
            compressed: Reduces memory footprint of database but increases computational
                demand
        """
        self._init_db(params_json, label_byte_count, nonce_byte_count, compressed)
        self.db_initialized = True

    def add_item(self, item: str, label: str) -> None:
        """Add an item with a label to the server's database so that the item can be
        queried by a client to learn about the label. If one considers this a key value
        store, the item is the key and the value is the label.
        """
        self._requires_db()
        self._add_item(item, label)

    def add_items(self, items_with_label: Iterable[Tuple[str, str]]) -> None:
        """Add multiple pairs of item and corresponding label to the server's database
        so that any of the items can be queried by a client to learn about the matching
        label. If one considers this a key value store, the item is the key and the
        value is the label.
        """
        self._requires_db()
        # TODO: Expose batch add in C++ PyAPSI
        for item, label in items_with_label:
            self.add_item(item=item, label=label)


class UnlabeledServer(_BaseServer):
    def __init__(self):
        """A server for unlabeled asynchronous private set intersection (APSI).

        For this server to do something meaningful, initialize an empty database with
        `init_db` or load an existing one with `load_db`.
        """
        super().__init__()

    def init_db(self, params_json: str, compressed: bool = False) -> None:
        """Initialize an empty database with the specified configuration.

        Args:
            params_json: The JSON string representation of APSI/SEAL parameters
            compressed: Reduces memory footprint of database but increases computational
                demand
        """
        self._init_db(params_json, 0, 0, compressed)
        self.db_initialized = True

    def add_item(self, item: str) -> None:
        """Add an item to the server's database so that the item can be queried by a
        client.
        """
        self._requires_db()
        self._add_item(item, "")

    def add_items(self, items: Iterable[str]) -> None:
        """Add multiple items to the server's database so that they can be queried by a
        client.
        """
        self._requires_db()
        # TODO: Expose batch add in C++ PyAPSI
        for item in items:
            self.add_item(item)
