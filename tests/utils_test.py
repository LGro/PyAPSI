import pytest

from apsi.utils import set_thread_count, get_thread_count


@pytest.mark.parametrize("thread_count", [2, 1])
def test_thread_count(thread_count):
    set_thread_count(thread_count)
    assert get_thread_count() == thread_count
