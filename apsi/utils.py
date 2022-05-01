from _pyapsi.utils import (
    _set_thread_count,
    _get_thread_count,
    _set_log_level,
    _set_console_log_disabled,
    _set_log_file,
)


def set_thread_count(thread_count: int) -> None:
    """Set the global APSI thread count, which allows parallelization of some
    operations to improve the runtime performance.
    """
    if thread_count < 1 or not isinstance(thread_count, int):
        raise ValueError(
            f"The thread_count needs to be a positive integer but is {thread_count}"
        )

    _set_thread_count(thread_count)


def get_thread_count() -> int:
    """Get the currently specified thread count for parallelization in APSI."""
    return _get_thread_count()


set_log_level = _set_log_level


set_console_log_disabled = _set_console_log_disabled


set_log_file = _set_log_file
