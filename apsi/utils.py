"""Utility functions for multi threading and logging."""

from _pyapsi.utils import (
    _set_thread_count,
    _get_thread_count,
    _set_log_level,
    _set_console_log_disabled,
    _set_log_file,
)


VALID_LOG_LEVELS = ["ALL", "DEBUG", "INFO", "WARNING", "ERROR", "OFF"]


def set_thread_count(thread_count: int) -> None:
    """Set the global APSI thread count.

    Allows parallelization of some operations to improve the runtime performance.
    """
    if thread_count < 1 or not isinstance(thread_count, int):
        raise ValueError(
            f"The thread_count needs to be a positive integer but is {thread_count}"
        )

    _set_thread_count(thread_count)


def get_thread_count() -> int:
    """Get the currently specified thread count for parallelization in APSI."""
    return _get_thread_count()


def set_log_level(level: str) -> None:
    """Set APSI log level.

    Args:
        level: One of "ALL", "DEBUG", "INFO", "WARNING", "ERROR", "OFF"

    Raises:
        ValueError: In case an invalid log level is provided
    """
    if level not in VALID_LOG_LEVELS:
        raise ValueError(f"Invalid log level {level}, choose one of {VALID_LOG_LEVELS}")
    _set_log_level(level)


def enable_console_log() -> None:
    """Enable APSI logging to standard output."""
    _set_console_log_disabled(False)


def disable_console_log() -> None:
    """Disable APSI logging to standard output."""
    _set_console_log_disabled(True)


def set_log_file(file_path: str) -> None:
    """Set file path to which APSI log output should be written."""
    _set_log_file(file_path)
