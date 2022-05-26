"""(Un-)labeled APSI client implementations."""

from typing import Dict, List

from _pyapsi import APSIClient as _Client


class _BaseClient(_Client):
    queried_items: List[str]

    def oprf_request(self, items: List[str]) -> bytes:
        """Create an OPRF request for a given item.

        This is the first step when querying a server for items.
        """
        self.queried_items = items
        return self._oprf_request(items)

    def build_query(self, oprf_response: bytes) -> bytes:
        """Build a query based on the server's response to an initial OPRF request.

        This is the second step when querying for items.

        Raises:
            RuntimeError: If `oprf_request` was not called before.
        """
        if not self.queried_items:
            raise RuntimeError("You need to create an OPRF request first.")

        return self._build_query(oprf_response)


class LabeledClient(_BaseClient):
    """A client for labeled asynchronous private set intersection (APSI).

    For a complete query, use the client interface in the following order:
        1. `oprf_request`
        2. `build_query`
        3. `extract_result`
    """

    def __init__(self, params_json: str):
        """Initialize a client for labeled APSI.

        Args:
            params_json: The JSON string representation of APSI/SEAL parameters
        """
        super().__init__(params_json)

    def extract_result(self, query_response: bytes) -> Dict[str, str]:
        """Extract the resulting item, label pairs from the server's query response.

        This is the final step when querying for items.

        Returns:
            Found items as keys where corresponding labels are values.
        """
        labels = self._extract_labeled_result_from_query_response(query_response)
        # Labels are retrieved from a fixed size memory and can thus contain other data
        # in case a specific label does not fill the full maximum label length.
        # Accordingly, everything after the first '\x00' is cut off.
        found_items_with_labels = {
            item: label.split("\x00", 1)[0]
            for item, label in zip(self.queried_items, labels)
            if label
        }
        return found_items_with_labels


class UnlabeledClient(_BaseClient):
    """A client for unlabeled asynchronous private set intersection (APSI).

    For a complete query, use the client interface in the following order:
        1. `oprf_request`
        2. `build_query`
        3. `extract_result`
    """

    def __init__(self, params_json: str):
        """Initialize a client for unlabeled APSI.

        Args:
            params_json: The JSON string representation of APSI/SEAL parameters
        """
        super().__init__(params_json)

    def extract_result(self, query_response: bytes) -> List[str]:
        """Extract the matched items from the server's query response.

        This is the final step when querying for items.
        """
        matches = super()._extract_unlabeled_result_from_query_response(query_response)
        return [item for item, match in zip(self.queried_items, matches) if match]
