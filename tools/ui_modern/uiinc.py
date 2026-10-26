"""Lenient reader for SWG UI source (.ui / .inc): an element tree with attribute spans, so tools can look up any
object by its UI path (/Styles.New.buttons.hud.style) and rewrite attributes in place without reformatting the file.

The files are XML-like but not XML: includes are spliced textually, attributes repeat, text bodies hold @string ids.
Only what the theme tools need is modelled: tags, attributes (with their character spans), children, text.
"""
from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path

# Attribute names are mostly identifiers, but some carry '+' or '.' (chatterspeech+accelerate, buttonUI.enabled).
TAG_RE = re.compile(r"<(/?)([A-Za-z0-9_]+)((?:\s+[^\s=<>/']+='[^']*')*)\s*(/?)>", re.S)
ATTR_RE = re.compile(r"([^\s=<>/']+)='([^']*)'")


@dataclass
class Node:
    tag: str
    attrs: dict[str, str]
    spans: dict[str, tuple[int, int]]       # attribute -> (start, end) of its value in the file text
    file: Path
    start: int
    parent: "Node | None" = None
    children: list["Node"] = field(default_factory=list)
    include: str | None = None              # for <include>name</include>

    @property
    def name(self) -> str:
        return self.attrs.get("Name", "")

    def child(self, name: str) -> "Node | None":
        low = name.lower()
        for c in self.children:
            if c.name.lower() == low:
                return c
        return None

    def path(self) -> str:
        parts = []
        n = self
        while n is not None and n.parent is not None:
            parts.append(n.name)
            n = n.parent
        return "/" + ".".join(reversed(parts))

    def walk(self):
        yield self
        for c in self.children:
            yield from c.walk()


def read_raw(path: Path) -> str:
    """File text with its line endings untouched (most .inc files are CRLF), so spans and rewrites stay exact."""
    with open(path, encoding="latin-1", newline="") as f:
        return f.read()


def write_raw(path: Path, text: str):
    with open(path, "w", encoding="latin-1", newline="") as f:
        f.write(text)


def parse_file(path: Path, root: Node | None = None) -> Node:
    text = read_raw(path)
    top = root or Node("File", {}, {}, path, 0)
    stack = [top]
    pos = 0
    for m in TAG_RE.finditer(text):
        closing, tag, attr_text, selfclose = m.groups()
        if tag == "include":
            if not closing:
                end = text.find("</include>", m.end())
                inc = Node("include", {}, {}, path, m.start(), stack[-1])
                inc.include = text[m.end():end].strip()
                stack[-1].children.append(inc)
            continue
        if closing:
            if len(stack) > 1:
                stack.pop()
            continue
        attrs, spans = {}, {}
        base = m.start(3)
        for am in ATTR_RE.finditer(attr_text):
            attrs[am.group(1)] = am.group(2)
            spans[am.group(1)] = (base + am.start(2), base + am.end(2))
        node = Node(tag, attrs, spans, path, m.start(), stack[-1])
        stack[-1].children.append(node)
        if not selfclose:
            stack.append(node)
    return top


def load_tree(ui_dir: Path, root_file: str = "ui_root.ui") -> Node:
    """ui_root.ui with every include spliced in place (by reference: included nodes keep their own file)."""
    seen: set[str] = set()

    def expand(node: Node):
        for i, c in enumerate(list(node.children)):
            if c.include:
                inc = ui_dir / c.include
                if c.include.lower() in seen or not inc.exists() or inc.stat().st_size == 0:
                    continue
                seen.add(c.include.lower())
                holder = parse_file(inc)
                idx = node.children.index(c)
                for hc in holder.children:
                    hc.parent = node
                node.children[idx:idx + 1] = holder.children
                for hc in holder.children:
                    expand(hc)
            else:
                expand(c)

    top = parse_file(ui_dir / root_file)
    expand(top)
    return top


def resolve(top: Node, path: str) -> Node | None:
    """'/Styles.New.buttons.hud.style' -> node. The root page is the first child of the file node."""
    root = top.children[0] if top.tag == "File" else top
    n = root
    for part in path.lstrip("/").split("."):
        if not part:
            continue
        n = n.child(part)
        if n is None:
            return None
    return n


INSERT = "\0insert\0"     # marks an edit that adds a new attribute line (see Rewriter.add)


class Rewriter:
    """Collects attribute edits per file and applies them back to front so spans stay valid."""

    def __init__(self):
        self.edits: dict[Path, list[tuple[int, int, str]]] = {}

    def set(self, node: Node, attr: str, value: str):
        if attr not in node.spans:
            raise KeyError(f"{node.path()} has no {attr} to rewrite")
        s, e = node.spans[attr]
        self.edits.setdefault(node.file, []).append((s, e, value))

    def add(self, node: Node, before: str, attr: str, value: str):
        """Add attr='value' on its own line just above the existing attribute `before`, with its indentation."""
        s = node.spans[before][0] - len(before) - 2          # start of  before='
        self.edits.setdefault(node.file, []).append((s, s, INSERT + f"{attr}='{value}'"))

    @staticmethod
    def splice(text: str, edits) -> str:
        nl = "\r\n" if "\r\n" in text[:4096] else "\n"
        for s, e, v in sorted(set(edits), reverse=True):
            if v.startswith(INSERT):
                line_start = text.rfind("\n", 0, s) + 1
                v = v[len(INSERT):] + nl + text[line_start:s]
            text = text[:s] + v + text[e:]
        return text

    def apply(self, out_dir: Path | None = None) -> list[Path]:
        written = []
        for f, edits in self.edits.items():
            text = self.splice(read_raw(f), edits)
            dst = (out_dir / f.name) if out_dir else f
            write_raw(dst, text)
            written.append(dst)
        return written
