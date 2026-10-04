"""Rename the product to JamulusAccess in the visible strings written out in the code.

Most visible strings use APP_NAME (global.h), which already says JamulusAccess. This script handles
the strings that write "Jamulus" out, in the code AND in every .ts file (source and translation
together, so no translation is lost).

Keeps: the credits and the licence statements (the Jamulus Development Team, "As of Jamulus...",
"part of Jamulus"), the pointer to the Jamulus website, code comments. Idempotent.

Usage: python tools/rename_product.py   (from the repository root)
"""
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
NAME_RE = re.compile(r"(?<![/\w.@-])Jamulus(?![\w/@.-])")
NEW = "JamulusAccess"
KEEP = re.compile(r"Development Team|As of Jamulus|part of Jamulus|Jamulus website|Jamulus 3\.|Jamulus server software|Jamulus client|source code documentation")
# code files whose written-out strings are renamed (the others only have comments or kept texts)
CODE = ["src/clientsettingsdlg.cpp", "src/util.cpp"]


def is_comment(line):
    s = line.lstrip()
    return s.startswith("//") or s.startswith("*") or s.startswith("/*")


def do_code():
    changed = 0
    for name in CODE:
        path = ROOT / name
        text = path.read_bytes().decode("utf-8")
        out = []
        for line in text.split("\n"):
            if "Jamulus" in line and not is_comment(line) and not KEEP.search(line) and ("tr (" in line or line.lstrip().startswith('"')):
                line = NAME_RE.sub(NEW, line)
            out.append(line)
        new = "\n".join(out)
        if new != text:
            path.write_bytes(new.encode("utf-8"))
            changed += 1
    return changed


MESSAGE_RE = re.compile(r"(<message>.*?</message>)", re.S)


def do_ts():
    changed = 0
    for path in (ROOT / "src/translation").glob("*.ts"):
        text = path.read_bytes().decode("utf-8")

        def fix(match):
            message = match.group(1)
            source = re.search(r"<source>(.*?)</source>", message, re.S)
            if not source or not NAME_RE.search(source.group(1)) or KEEP.search(source.group(1)):
                return message
            return NAME_RE.sub(NEW, message)  # source and translation

        new = MESSAGE_RE.sub(fix, text)
        if new != text:
            path.write_bytes(new.encode("utf-8"))
            changed += 1
    return changed


def do_about_menu():
    """'&About Jamulus...' (the dots keep it out of NAME_RE, which spares jamulus.io)."""
    path = ROOT / "src/util.cpp"
    text = path.read_bytes().decode("utf-8")
    new = text.replace('tr ( "&About Jamulus..." )', 'tr ( "&About JamulusAccess..." )')
    changed = int(new != text)
    path.write_bytes(new.encode("utf-8"))
    for ts in (ROOT / "src/translation").glob("*.ts"):
        text = ts.read_bytes().decode("utf-8")

        def fix(match):
            message = match.group(1)
            if "<source>&amp;About Jamulus...</source>" not in message:
                return message
            return re.sub(r"Jamulus(?=(\.\.\.|…|</))", NEW, message)

        new = MESSAGE_RE.sub(fix, text)
        if new != text:
            ts.write_bytes(new.encode("utf-8"))
            changed += 1
    return changed


if __name__ == "__main__":
    print("code files:", do_code())
    print("translation files:", do_ts())
    print("about menu:", do_about_menu())
