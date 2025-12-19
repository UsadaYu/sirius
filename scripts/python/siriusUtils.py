class SiriusUtils:
    def __init__(self):
        pass

    @staticmethod
    def remove_json_comments(text: str) -> str:
        """
        Remove `//` single-line comments and `/*... */` Multi-line comments.
        """
        out = []
        i = 0
        n = len(text)
        in_string = False
        string_delim = None

        while i < n:
            ch = text[i]

            if in_string:
                # Handle within strings: allow escape characters
                if ch == "\\" and i + 1 < n:
                    out.append(text[i])
                    out.append(text[i + 1])
                    i += 2
                    continue
                if ch == string_delim:
                    in_string = False
                    string_delim = None
                out.append(ch)
                i += 1
            else:
                # Not a string: check if a string or comment has been entered
                if ch == '"' or ch == "'":
                    in_string = True
                    string_delim = ch
                    out.append(ch)
                    i += 1
                    continue

                # Start to check the annotation
                if ch == "/" and i + 1 < n:
                    nxt = text[i + 1]
                    if nxt == "/":
                        # Single-line comment: jump to the end of the line,
                        # but keep the line ending character
                        i += 2
                        while i < n and text[i] not in "\r\n":
                            i += 1
                        # Copy the possible line breaks over (keep the line numbers)
                        if i < n and text[i] == "\r":
                            out.append("\r")
                            i += 1
                        if i < n and text[i] == "\n":
                            out.append("\n")
                            i += 1
                        continue
                    elif nxt == "*":
                        # Multi-line comment: jump to `*/` while keeping the
                        # line break within the comment
                        i += 2
                        start = i
                        newline_count = 0
                        while i + 1 < n and not (
                            text[i] == "*" and text[i + 1] == "/"
                        ):
                            if text[i] == "\n":
                                newline_count += 1
                            i += 1
                        # If an ending symbol is found at the end of a comment,
                        # skip it as well
                        if i + 1 < n:
                            i += 2
                        # Keep the same number of line breaks to maintain the
                        # original line numbers
                        out.extend("\n" * newline_count)
                        continue
                    else:
                        # Not a comment. Normally output `/`
                        out.append(ch)
                        i += 1
                        continue
                # Ordinary characters
                out.append(ch)
                i += 1

        return "".join(out)
