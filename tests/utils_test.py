import io
import pathlib
from contextlib import redirect_stdout

import pytest
from apsi.servers import UnlabeledServer
from apsi.utils import (
    disable_console_log,
    enable_console_log,
    get_thread_count,
    set_log_file,
    set_log_level,
    set_thread_count,
)


def _something_that_logs(apsi_params: str) -> str:
    server = UnlabeledServer()
    server.init_db(apsi_params)
    server.add_item("item")

    expected_message = "computing OPRF hashes for 1 items"
    return expected_message


@pytest.mark.parametrize("thread_count", [2, 1])
def test_thread_count(thread_count: int):
    set_thread_count(thread_count)
    assert get_thread_count() == thread_count


def test_log_file_output(tmp_path: pathlib.Path, apsi_params: str):
    set_log_level("ALL")
    log_file_path = tmp_path / "test.log"
    set_log_file(str(log_file_path))

    expected_message = _something_that_logs(apsi_params)

    with open(log_file_path, "r") as fh:
        log_file_content = fh.read()
    assert expected_message in log_file_content


def test_disabled_console_log_no_output(apsi_params: str):
    set_log_level("ALL")
    disable_console_log()

    f = io.StringIO()
    with redirect_stdout(f):
        _something_that_logs(apsi_params)

    assert f.getvalue() == ""


@pytest.mark.xfail
def test_enabled_console_log_output(apsi_params: str):
    set_log_level("ALL")
    enable_console_log()

    f = io.StringIO()
    with redirect_stdout(f):
        expected_message = _something_that_logs(apsi_params)

    assert expected_message in f.getvalue()
