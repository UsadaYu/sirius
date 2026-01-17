#!/usr/bin/env python

import argparse
import json
import os
import sys

try:
    # Python 2
    from io import open
except ImportError:
    # Python 3
    pass

try:
    # Python 3
    JSONDecodeError = json.JSONDecodeError
except AttributeError:
    # Python 2
    JSONDecodeError = ValueError


def load_json(json_path):
    """Load the json file."""
    if not os.path.exists(json_path):
        raise IOError("No such file: {}".format(json_path))

    try:
        with open(json_path, "r", encoding="utf-8") as f:
            return json.load(f)
    except JSONDecodeError as e:
        raise ValueError("Invalid json format: {}".format(e))
    except Exception as e:
        raise RuntimeError("Failed to read the file: {}".format(e))


def main():
    parser = argparse.ArgumentParser(description="Version Helper")
    parser.add_argument("--json", required=True, help="Path to version json")
    args = parser.parse_args()

    try:
        data = load_json(args.json)
        sys.stdout.write(str(data["version"]))
    except Exception as e:
        sys.stderr.write("\033[0;31mError: {}\033[0m\n".format(str(e)))
        sys.exit(1)


if __name__ == "__main__":
    main()
