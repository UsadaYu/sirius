#!/usr/bin/env python3
"""
The json reading tool, supports simple json structure access.

Usage:
    python3 json_reader.py <json_file> [key1 key2 ...]

Example:
    python3 json_reader.py config.json version
    python3 json_reader.py config.json gnu version
    python3 json_reader.py config.json  # Display the entire json
"""

import json
import sys
import os
from typing import Any, List


ANSI_RED = "\033[0;31m"
ANSI_YELLOW = "\033[1;33m"
ANSI_RESET = "\033[0m"


class JsonReader:
    """Json file reader."""

    def __init__(self, json_file: str):
        """
        Initialize the json reader.

        Args:
            json_file: Json file path.
        """
        self.json_file = json_file
        self.data = self._load_json()

    def _load_json(self) -> Any:
        """Load the json file."""
        if not os.path.exists(self.json_file):
            raise FileNotFoundError(
                f"The file does not exist: {self.json_file}"
            )

        try:
            with open(self.json_file, "r", encoding="utf-8") as f:
                return json.load(f)
        except json.JSONDecodeError as e:
            raise ValueError(f"Invalid json format: {e}")
        except Exception as e:
            raise RuntimeError(f" Failed to read the file: {e}")

    def get_value(self, keys: List[str]) -> Any:
        """
        Obtain the value based on the key path.

        Args:
            keys: List of key paths.

        Returns:
            The corresponding value on success, `None` otherwise.
        """
        current = self.data

        for key in keys:
            if isinstance(current, dict) and key in current:
                current = current[key]
            else:
                found = False
                if isinstance(current, dict):
                    for k, v in current.items():
                        if isinstance(v, dict) and key in v:
                            current = v[key]
                            found = True
                            break

                if not found:
                    return None

        return current

    def display_value(self, value: Any, indent: int = 2) -> str:
        """Format the display value."""
        if value is None:
            return "null"

        if isinstance(value, (dict, list)):
            return json.dumps(value, indent=indent, ensure_ascii=False)
        else:
            return str(value)

    def display_all(self) -> str:
        """Display the entire json."""
        return json.dumps(self.data, indent=2, ensure_ascii=False)


def print_usage():
    print(__doc__, flush=True)
    print("Examples of supported json structures:", flush=True)
    print("--- Flat structure ---", flush=True)
    print(
        """{
  "version": "1.3.4"
}\n""",
        flush=True,
    )
    print("--- Nested structure ---", flush=True)
    print(
        """{
  "gnu": {
    "version": "13.2.0"
  },
  "clang": {
    "version": "18.1.3"
  }
}\n""",
        flush=True,
    )


def main():
    if len(sys.argv) < 2:
        print_usage()
        sys.exit(1)

    json_file = sys.argv[1]

    try:
        reader = JsonReader(json_file)

        if len(sys.argv) == 2:
            print(reader.display_all())
            sys.exit(0)

        keys = sys.argv[2:]
        value = reader.get_value(keys)

        if value is not None:
            print(reader.display_value(value))
        else:
            print(
                ANSI_RED
                + f"[Warning] Key path not found: `{' -> '.join(keys)}`"
                + ANSI_YELLOW
                + "\n"
                + f"Available top-level keys: {list(reader.data.keys())}"
                + ANSI_RESET,
                flush=True,
            )
            sys.exit(1)

    except FileNotFoundError as e:
        print(f"{ANSI_RED}[Error] {e}{ANSI_RESET}")
        sys.exit(1)
    except ValueError as e:
        print(f"{ANSI_RED}[Json Error] {e}{ANSI_RESET}")
        sys.exit(1)
    except Exception as e:
        print(f"{ANSI_RED}[Exception] {e}{ANSI_RESET}")
        sys.exit(1)


if __name__ == "__main__":
    main()
