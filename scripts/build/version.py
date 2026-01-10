#!/usr/bin/env python3

import argparse
import json
import os
import sys


def load_json(json_path):
    """Load the json file."""
    if not os.path.exists(json_path):
        raise FileNotFoundError(f"No such file: {json_path}")

    try:
        with open(json_path, "r", encoding="utf-8") as f:
            return json.load(f)
    except json.JSONDecodeError as e:
        raise ValueError(f"Invalid json format: {e}")
    except Exception as e:
        raise RuntimeError(f"Failed to read the file: {e}")


def main():
    parser = argparse.ArgumentParser(description="Version Helper")
    parser.add_argument("--json", required=True, help="Path to version json")
    args = parser.parse_args()

    try:
        data = load_json(args.json)
        print(data["version"], end="")
    except Exception as e:
        sys.stderr.write("\033[0;31m" f"Error: {str(e)}\n" "\033[0m")
        sys.exit(1)


if __name__ == "__main__":
    main()
