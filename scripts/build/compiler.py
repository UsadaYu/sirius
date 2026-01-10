#!/usr/bin/env python3

import argparse
import json
import re
import sys
from typing import Dict, Final, Optional


class JsK:
    LANG_C = "c"
    LANG_CXX = "cxx"


class Utils:
    ARGS_ACTION = ["min_version", "project_flags", "test_matrix"]

    AliasMap = Dict[str, set[str]]
    StandardNameMap = Dict[str, str]

    LANG_ALIASES: Final[AliasMap] = {
        JsK.LANG_C: {"c"},
        JsK.LANG_CXX: {"cxx", "c++", "cpp"},
    }

    COMPILER_ALIASES: Final[AliasMap] = {
        "gnu": {"gnu", "gnuc", "gcc", "g++"},
        "clang": {"clang", "clang++"},
        "clang-cl": {"clang-cl", "clang cl"},
        "msvc": {"msvc", "cl", "cl.exe"},
    }

    _COMPILER_ALIAS_TO_STANDARD: Optional[StandardNameMap] = None
    _LANG_ALIAS_TO_STANDARD: Optional[StandardNameMap] = None

    @classmethod
    def _get_compiler_alias_map(cls) -> StandardNameMap:
        if cls._COMPILER_ALIAS_TO_STANDARD is None:
            cls._COMPILER_ALIAS_TO_STANDARD = {
                alias: standard_name
                for standard_name, aliases in cls.COMPILER_ALIASES.items()
                for alias in aliases
            }
        return cls._COMPILER_ALIAS_TO_STANDARD

    @classmethod
    def _get_lang_alias_map(cls) -> StandardNameMap:
        if cls._LANG_ALIAS_TO_STANDARD is None:
            cls._LANG_ALIAS_TO_STANDARD = {
                alias: standard_name
                for standard_name, aliases in cls.LANG_ALIASES.items()
                for alias in aliases
            }
        return cls._LANG_ALIAS_TO_STANDARD

    @staticmethod
    def _normalize_identifier(identifier: str) -> str:
        identifier = identifier.strip().lower()
        if identifier.endswith(".exe"):
            identifier = identifier[:-4]
        return identifier

    @classmethod
    def _normalize_with_alias_map(
        cls, identifier: str, alias_map: StandardNameMap, entity_type: str
    ) -> str:
        normalized_id = cls._normalize_identifier(identifier)

        if standard_name := alias_map.get(normalized_id):
            return standard_name

        raise ValueError(f"Unsupported {entity_type}: {identifier}")

    @classmethod
    def normalize_compiler_id(cls, compiler_id: str) -> str:
        """
        Convert the compiler alias to standardized id.

        Args:
            compiler_id: Compiler name or alias.

        Returns:
            Standardized compiler id.

        Raises:
            ValueError: When the compiler is not supported.
        """
        return cls._normalize_with_alias_map(
            compiler_id, cls._get_compiler_alias_map(), "compiler"
        )

    @classmethod
    def normalize_lang(cls, lang: str) -> str:
        return cls._normalize_with_alias_map(
            lang, cls._get_lang_alias_map(), "language"
        )

    @classmethod
    def args_parse(cls) -> argparse.ArgumentParser:
        parser = argparse.ArgumentParser(description="Compiler Helper")

        parser.add_argument(
            "--json", required=True, help="Path to compiler json"
        )
        parser.add_argument(
            "--compiler",
            required=True,
            help="Compiler ID (gnu, msvc, clang, etc.). Ignore case",
        )
        parser.add_argument(
            "--action",
            required=True,
            choices=Utils.ARGS_ACTION,
            help="Action to perform",
        )
        parser.add_argument(
            "--lang",
            type=lambda var: var.lower(),
            choices=cls._get_lang_alias_map(),
            help="Language for `test_matrix`, `project_flags`. Ignore case",
        )

        return parser

    @staticmethod
    def load_json_without_comments(filename):
        with open(filename, "r", encoding="utf-8") as f:
            content = f.read()
            content = re.sub(r"//.*", "", content)
            content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)
            return json.loads(content)


class CompilerConfigurator:
    def __init__(self, json_path, compiler_id):
        self.data = Utils.load_json_without_comments(json_path)

        if compiler_id not in self.data:
            raise ValueError(f"Unknown compiler: {compiler_id}")

        self.cfg_compiler_id = self.data[compiler_id]

    def get_compiler_min_version(self) -> str:
        return self.cfg_compiler_id["min_version"]

    def get_project_flags(self):
        cfg_standards = self.cfg_compiler_id["project"]["standards"]
        c_std = cfg_standards[JsK.LANG_C]
        cxx_std = cfg_standards[JsK.LANG_CXX]

        return {
            JsK.LANG_C: c_std,
            JsK.LANG_CXX: cxx_std,
        }

    def get_test_matrix(self):
        """
        Return the list of all supported iso standard flags for generating the
        test matrix.
        """

        cfg_standards = self.cfg_compiler_id["test"]["standards"]
        c_stds = cfg_standards[JsK.LANG_C]
        cxx_stds = cfg_standards[JsK.LANG_CXX]

        matrix = {JsK.LANG_C: [], JsK.LANG_CXX: []}

        for std in c_stds:
            matrix[JsK.LANG_C].append(std)

        for std in cxx_stds:
            matrix[JsK.LANG_CXX].append(std)

        return matrix


def main():
    parser = Utils.args_parse()
    args = parser.parse_args()

    try:
        lang = ""
        compiler_id = Utils.normalize_compiler_id(args.compiler)
        config = CompilerConfigurator(args.json, compiler_id)

        if args.action in ["project_flags", "test_matrix"]:
            if not args.lang:
                raise ValueError(
                    f"`--lang` is required for the action `{args.action}`"
                )
            else:
                lang = Utils.normalize_lang(args.lang)

        if args.action == "min_version":
            print(config.get_compiler_min_version(), end="")

        elif args.action == "project_flags":
            flags = config.get_project_flags()
            print(flags[lang], end="")

        elif args.action == "test_matrix":
            matrix = config.get_test_matrix()
            print(";".join(matrix[lang]), end="")

    except Exception as e:
        sys.stderr.write("\033[0;31m" f"Error: {str(e)}\n" "\033[0m")
        sys.exit(1)


if __name__ == "__main__":
    main()
