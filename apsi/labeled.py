from typing import Dict, Iterable, List, Tuple

from _pyapsi.labeled import APSIServer as _Server
from _pyapsi.labeled import APSIClient as _Client


class Client(_Client):
    queried_items: List[str]

    def __init__(self, params_json: str):
        """A client for labeled asynchronous private set intersection (APSI).

        For a complete query, use the client interface in the following order:
            1. `oprf_request`
            2. `build_query`
            3. `extract_result_from_query_response`

        Args:
            params_json: The JSON string representation of APSI/SEAL parameters
        """
        super().__init__(params_json)

    def oprf_request(self, item: str) -> bytes:
        """Create an OPRF request for a given item. This is the first step when querying
        a server for items.
        """
        # TODO: Switch to a request multi with items: List[str]
        self.queried_items = (item,)
        return self._oprf_request(item)

    def build_query(self, oprf_response: bytes) -> bytes:
        """Build a query based on the server's response to an initial OPRF request. This
        is the second step when querying for items.
        """
        if not self.queried_items:
            raise RuntimeError("You need to create an OPRF request first.")

        return self._build_query(oprf_response)

    def extract_result_from_query_response(
        self, query_response: bytes
    ) -> Dict[str, str]:
        """Extract the resulting item, label pairs from the server's query response.
        This is the final step when querying for items.

        Returns:
            Found items as keys where corresponding labels are values.
        """
        labels = self._extract_result_from_query_response(query_response)
        found_items_with_labels = {
            item: label for item, label in zip(self.queried_items, labels) if label
        }
        return found_items_with_labels


class Server(_Server):
    db_initialized: bool = False

    def __init__(self):
        """A server for labeled asynchronous private set intersection (APSI).

        For this server to do something meaningful, initialize an empty database with
        `Server.init_db` or load an existing one with `Server.load_db`.
        """
        super().__init__()

    @property
    def label_byte_count(self) -> int:
        return self.db_label_byte_count

    def _requires_db(self):
        if not self.db_initialized:
            raise RuntimeError("Please initialize or load a database first.")

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
