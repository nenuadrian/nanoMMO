#!/usr/bin/env python3
"""The client's translations, as gettext PO files in po/.

extract writes the template, po/nanommo.pot: each string the client's sources
hand to Tr, TrN, TrLabel or N_, or to one of the helpers that translate what
they're given, such as GameButton, and the content's names, in the context
the client looks them up in. update merges the template into each language's
PO file, as gettext's msgmerge would: a translation stays with its string, a
string that changed a little keeps its old translation marked fuzzy, and one
that's gone is kept at the end, obsolete, for a string that comes back."""

import argparse
import copy
import difflib
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PO = ROOT / "po"
TEMPLATE = PO / "nanommo.pot"

# The functions whose arguments are strings to translate: for each, by how
# many arguments it's called with (None for any), the places, from 1, of its
# context, its English and its plural.
KEYWORDS = {
    "Tr": {1: (None, 1, None), 2: (1, 2, None)},
    "TrN": {3: (None, 1, 2), 4: (1, 2, 3)},
    "TrLabel": {1: (None, 1, None)},
    "N_": {1: (None, 1, None), 2: (1, 2, None)},
    # The game's own helpers, which translate what they're given.
    "GameButton": {None: (None, 1, None)},
    "BeginPanel": {None: (None, 1, None)},
    "Line": {None: (None, 1, None)},
    "BarButton": {None: (None, 4, None)},
    "Stepper": {None: (None, 1, None)},
    "Swatches": {None: (None, 1, None)},
}
# Those whose string is an ImGui label, of which what's before "##" shows.
LABELS = {"TrLabel", "GameButton", "BeginPanel"}

# The content's names, by the file, the field its list is in, and the context
# each is looked up in: its kind, and its id where it has one
# (client/names.h).
CONTENT = [
    ("content/items.txtpb", "items", "item", "An item's name."),
    ("content/skills.txtpb", "skills", "skill", "A skill's name."),
    ("content/monsters.txtpb", "kinds", "monster",
     "A kind of monster's name."),
    ("content/npcs.txtpb", "kinds", "npc", "An NPC's name."),
    ("content/quests.txtpb", "quests", "quest", "A quest's name."),
    ("content/recipes.txtpb", "recipes", "recipe", "A recipe's name."),
    ("content/figures.txtpb", "hairs", "hair", "A way to wear the hair."),
    ("content/rig.txtpb", "builds", "build", "A build, a character's shape."),
]
LANDS = "terrain/lands.h"

HEADER = """\
# nanoMMO's words in English, which each language's PO file translates.
msgid ""
msgstr ""
"Project-Id-Version: nanoMMO\\n"
"MIME-Version: 1.0\\n"
"Content-Type: text/plain; charset=UTF-8\\n"
"Content-Transfer-Encoding: 8bit\\n"
"Plural-Forms: nplurals=2; plural=(n != 1);\\n"
"""

# An old translation is offered for a changed string, fuzzy, while they're at
# least this alike, from 0 to 1.
ALIKE = 0.6


class Error(Exception):
    pass


class Entry:
    """A string to translate, with what each PO file says of it."""

    def __init__(self, id, context=None, plural=None):
        self.context = context
        self.id = id
        self.plural = plural
        self.strings = [""] * (2 if plural is not None else 1)
        # Translators' own comments, the sources', where it's from, and flags
        # such as fuzzy.
        self.comments = []
        self.extracted = []
        self.references = []
        self.flags = []
        self.obsolete = False

    def key(self):
        return (self.context, self.id)

    def translated(self):
        return all(self.strings) and "fuzzy" not in self.flags


# C++: just enough of it to find the calls and their strings.

CPP_TOKEN = re.compile(
    r"""(?P<comment>//[^\n]*|/\*.*?\*/)
      | (?P<raw>R"(?P<delimiter>[^(\s]*)\((?:.|\n)*?\)(?P=delimiter)")
      | (?P<string>"(?:[^"\\\n]|\\.)*")
      | (?P<char>'(?:[^'\\\n]|\\.)*')
      | (?P<name>[A-Za-z_]\w*)
      | (?P<number>\d[\w.']*)
      | (?P<space>\s+)
      | (?P<punct>::|->|.)""",
    re.VERBOSE | re.DOTALL,
)
ESCAPES = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "'": "'", "\\": "\\"}


def unescape(body, where):
    text = []
    i = 0
    while i < len(body):
        if body[i] != "\\":
            text.append(body[i])
            i += 1
            continue
        if i + 1 >= len(body) or body[i + 1] not in ESCAPES:
            raise Error(f"{where}: an escape there isn't: {body[i:i + 2]}")
        text.append(ESCAPES[body[i + 1]])
        i += 2
    return "".join(text)


def cpp_tokens(text, path):
    """The tokens, as (kind, value, line), without the spaces. A string's
    value is what's in its quotes, escapes and all."""
    tokens = []
    line = 1
    for match in CPP_TOKEN.finditer(text):
        kind = match.lastgroup
        value = match.group()
        if kind == "string":
            value = value[1:-1]
        if kind == "comment" and value.startswith("//"):
            value = value[2:].strip()
        if kind != "space":
            tokens.append((kind, value, line))
        line += match.group().count("\n")
    return tokens


def arguments(tokens, start):
    """A call's arguments, each a list of tokens, from just after its "("."""
    args = [[]]
    depth = 0
    for i in range(start, len(tokens)):
        kind, value, _ = tokens[i]
        if kind == "punct" and value in "([{":
            depth += 1
        elif kind == "punct" and value in ")]}":
            if depth == 0:
                return args if args != [[]] else []
            depth -= 1
        elif kind == "punct" and value == "," and depth == 0:
            args.append([])
            continue
        args[-1].append(tokens[i])
    return None


def literal(arg, path):
    """The string an argument is, when it's string literals alone, which C++
    joins, or None."""
    if arg and all(kind == "string" for kind, _, _ in arg):
        return "".join(unescape(value, f"{path}:{line}")
                       for _, value, line in arg)
    return None


def literals(arg, path):
    """The strings an argument can be: itself, or either side of a ?:."""
    whole = literal(arg, path)
    if whole is not None:
        return [whole]
    found = []
    side = []
    for token in arg + [("punct", ":", 0)]:
        if token[0] == "punct" and token[1] in "?:":
            if literal(side, path) is not None:
                found.append(literal(side, path))
            side = []
        else:
            side.append(token)
    return found


def from_source(path):
    """Each string a client source file translates, in order, with a comment
    for its translators where one's above it, starting "Translators:", which
    goes with the first string in the statement after it."""
    text = (ROOT / path).read_text(encoding="utf-8")
    tokens = cpp_tokens(text, path)
    found = []
    note = None
    for i, (kind, value, line) in enumerate(tokens):
        if kind == "comment":
            if value.startswith("Translators:"):
                note = value[len("Translators:"):].strip()
            elif note is not None and tokens[i - 1][0] == "comment":
                note += " " + value
            continue
        if kind == "punct" and value in ";}":
            note = None
            continue
        if kind != "name" or value not in KEYWORDS:
            continue
        if i + 1 >= len(tokens) or tokens[i + 1][1] != "(":
            continue
        # A member's or another namespace's, such as ImGui::Begin, isn't ours.
        before = [token[1] for token in tokens[max(i - 2, 0):i]]
        if before[-1:] in (["::"], ["."], ["->"]) and \
                before != ["nanommo", "::"]:
            continue
        args = arguments(tokens, i + 2)
        if args is None:
            continue
        spec = KEYWORDS[value].get(len(args), KEYWORDS[value].get(None))
        if spec is None:
            continue
        context_at, id_at, plural_at = spec
        if id_at > len(args):
            continue
        context = literal(args[context_at - 1], path) if context_at else None
        plural = literal(args[plural_at - 1], path) if plural_at else None
        if (context_at and context is None) or (plural_at and plural is None):
            continue
        ids = ([literal(args[id_at - 1], path)] if plural_at
               else literals(args[id_at - 1], path))
        for id in ids:
            if id is None:
                continue
            if value in LABELS:
                id = id.split("##")[0]
            if not id:
                continue
            entry = Entry(id, context, plural)
            entry.references = [path]
            if note:
                entry.extracted = [note]
                note = None
            found.append(entry)
    return found


# Protobuf's text format: just enough of it to read the content's names.

TEXT_TOKEN = re.compile(
    r"""(?P<comment>\#[^\n]*)
      | (?P<string>"(?:[^"\\\n]|\\.)*"|'(?:[^'\\\n]|\\.)*')
      | (?P<word>[-+]?[\w.]+)
      | (?P<space>\s+)
      | (?P<punct>.)""",
    re.VERBOSE,
)


def read_message(tokens, at, path):
    """The fields from tokens[at], as (name, value) pairs, until a "}" or the
    end, where a message's value is its own fields, a list's a list, and a
    string's the string. Returns them, and where it stopped."""
    fields = []
    while at < len(tokens) and tokens[at] != ("punct", "}"):
        kind, name = tokens[at]
        if kind != "word":
            raise Error(f"{path}: expected a field's name, not {name!r}")
        at += 1
        if tokens[at] == ("punct", ":"):
            at += 1
        value, at = read_value(tokens, at, path)
        fields.append((name, value))
        if at < len(tokens) and tokens[at] in (("punct", ","), ("punct", ";")):
            at += 1
    return fields, at


def read_value(tokens, at, path):
    kind, value = tokens[at]
    if (kind, value) in (("punct", "{"), ("punct", "<")):
        fields, at = read_message(tokens, at + 1, path)
        return fields, at + 1
    if (kind, value) == ("punct", "["):
        values = []
        at += 1
        while tokens[at] != ("punct", "]"):
            item, at = read_value(tokens, at, path)
            values.append(item)
            if tokens[at] == ("punct", ","):
                at += 1
        return values, at + 1
    if kind == "string":
        text = ""
        while at < len(tokens) and tokens[at][0] == "string":
            text += unescape(tokens[at][1][1:-1], path)
            at += 1
        return text, at
    return value, at + 1


def read_text_format(path):
    text = (ROOT / path).read_text(encoding="utf-8")
    tokens = [
        (m.lastgroup, m.group())
        for m in TEXT_TOKEN.finditer(text)
        if m.lastgroup not in ("space", "comment")
    ]
    fields, _ = read_message(tokens, 0, path)
    return fields


def field(fields, name, default=None):
    return next((value for key, value in fields if key == name), default)


def from_content():
    """The content's names, each in its context, as client/names.cc looks
    them up."""
    found = []

    def add(path, context, id, note, plural=None):
        entry = Entry(id, context, plural)
        entry.references = [path]
        entry.extracted = [note]
        found.append(entry)

    # The kinds of monster a quest asks for so many of.
    counted = {
        field(field(quest, "kill", []), "monster")
        for key, quest in read_text_format("content/quests.txtpb")
        if key == "quests"
    }
    for path, list_field, kind, note in CONTENT:
        for key, each in read_text_format(path):
            if key != list_field:
                continue
            name = field(each, "name", "")
            if not name:
                continue
            id = field(each, "id")
            context = f"{kind} {id}" if id is not None else kind
            add(path, context, name, note)
            if kind == "monster" and id in counted:
                add(path, context, f"$0 {name}",
                    "So many of a kind of monster, as a quest asks for: $0 "
                    "is how many.", plural=f"$0 {name}s")
            if kind == "monster":
                for key, skill in each:
                    if key == "skills" and field(skill, "name"):
                        add(path, f"{context} skill {field(skill, 'id')}",
                            field(skill, "name"), "A monster's skill.")
            if kind == "quest" and field(each, "text"):
                add(path, context, field(each, "text"),
                    "What a quest's giver says, giving it.")
    return found


def from_lands():
    """The lands' names, which kLands has, as their keys too."""
    tokens = cpp_tokens((ROOT / LANDS).read_text(encoding="utf-8"), LANDS)
    found = []
    for i in range(len(tokens) - 3):
        if [token[1] for token in tokens[i:i + 3]] != [".", "name", "="]:
            continue
        if tokens[i + 3][0] == "string":
            entry = Entry(unescape(tokens[i + 3][1], LANDS), "land")
            entry.references = [LANDS]
            entry.extracted = [
                "A land's name, which \"The $0\" makes a title."]
            found.append(entry)
    return found


def extract():
    """The template's entries, each string once, where it's first found."""
    sources = sorted(
        str(path.relative_to(ROOT))
        for pattern in ("client/*.cc", "client/*.h")
        for path in ROOT.glob(pattern)
        if not path.name.endswith("_test.cc")
    )
    entries = {}
    for entry in [e for path in sources for e in from_source(path)] + \
            from_lands() + from_content():
        known = entries.get(entry.key())
        if known is None:
            entries[entry.key()] = entry
            continue
        if known.plural != entry.plural:
            raise Error(f"{entry.id!r} is a plural in one place and not in "
                        f"another: {known.references[0]}, "
                        f"{entry.references[0]}")
        known.references = sorted(set(known.references + entry.references))
        known.extracted += [
            e for e in entry.extracted if e not in known.extracted]
    return list(entries.values())


# PO files.

def quote(text):
    return '"' + (text.replace("\\", "\\\\").replace('"', '\\"')
                  .replace("\n", "\\n").replace("\t", "\\t")
                  .replace("\r", "\\r")) + '"'


def po_string(keyword, text):
    """A keyword and its string, a line of it to each line of the text, as
    gettext writes one with line breaks in it."""
    lines = text.splitlines(keepends=True)
    if len(lines) < 2:
        return [f"{keyword} {quote(text)}"]
    return [f'{keyword} ""'] + [quote(line) for line in lines]


def write_entry(entry):
    lines = [f"# {c}" if c else "#" for c in entry.comments]
    lines += [f"#. {c}" for c in entry.extracted]
    if entry.references:
        lines.append("#: " + " ".join(entry.references))
    if entry.flags:
        lines.append("#, " + ", ".join(entry.flags))
    body = []
    if entry.context is not None:
        body += po_string("msgctxt", entry.context)
    body += po_string("msgid", entry.id)
    if entry.plural is None:
        body += po_string("msgstr", entry.strings[0])
    else:
        body += po_string("msgid_plural", entry.plural)
        for i, text in enumerate(entry.strings):
            body += po_string(f"msgstr[{i}]", text)
    if entry.obsolete:
        body = [f"#~ {line}" for line in body]
    return "\n".join(lines + body) + "\n"


def write_po(header, entries):
    return "\n".join([header] + [write_entry(e) for e in entries])


def read_po(path):
    """The header, as its text and as an entry, and the other entries, as
    the file has them, obsolete ones too."""
    entries = []
    # Each entry's comments come before it, so a new one starts empty, and
    # the line in quotes after a keyword goes on its string: last says which.
    entry = Entry(None)
    entry.strings = []
    last = None
    for number, line in enumerate(path.read_text(encoding="utf-8")
                                  .splitlines(), 1):
        where = f"{path.name}:{number}"
        line = line.strip()
        obsolete = line.startswith("#~")
        if obsolete:
            line = line[2:].strip()
        if not line:
            continue
        # A comment, msgctxt or msgid after an entry's strings is the next
        # entry's.
        if entry.strings and line.startswith(("#", "msgctxt ", "msgid ")):
            entries.append(entry)
            entry = Entry(None)
            entry.strings = []
            last = None
        started = entry.id is not None or entry.context is not None
        if line.startswith("#"):
            if started:
                raise Error(f"{where}: a comment in the middle of an entry")
            kind, rest = line[:2], line[2:].strip()
            if kind == "#.":
                entry.extracted.append(rest)
            elif kind == "#:":
                entry.references += rest.split()
            elif kind == "#,":
                entry.flags += [f.strip() for f in rest.split(",")]
            elif kind != "#|":
                entry.comments.append(line[1:].strip())
            continue
        if line.startswith('"'):
            if last is None:
                raise Error(f"{where}: a string that follows no keyword")
            if isinstance(last, int):
                entry.strings[last] += unquote(line, where)
            else:
                setattr(entry, last,
                        getattr(entry, last) + unquote(line, where))
            continue
        keyword, _, rest = line.partition(" ")
        value = unquote(rest.strip(), where)
        after_id = entry.id is not None and entry.plural is None
        if keyword == "msgctxt" and not started:
            entry.context, last = value, "context"
        elif keyword == "msgid" and entry.id is None:
            entry.id, last = value, "id"
        elif keyword == "msgid_plural" and after_id:
            entry.plural, last = value, "plural"
        elif keyword == "msgstr" and after_id and not entry.strings:
            entry.strings, last = [value], 0
        elif (entry.plural is not None
              and keyword == f"msgstr[{len(entry.strings)}]"):
            entry.strings.append(value)
            last = len(entry.strings) - 1
        else:
            raise Error(f"{where}: {keyword} where it can't be")
        entry.obsolete = obsolete
    if entry.id is not None and not entry.strings:
        raise Error(f"{path.name}: the last entry has no msgstr")
    if entry.strings:
        entries.append(entry)
    if not entries or entries[0].key() != (None, ""):
        raise Error(f"{path.name}: the header, msgid \"\", isn't first")
    header = entries.pop(0)
    return write_entry(header), header, entries


def unquote(text, where):
    if len(text) < 2 or text[0] != '"' or text[-1] != '"':
        raise Error(f"{where}: expected a string in quotes")
    body = text[1:-1]
    if re.search(r'(?<!\\)(?:\\\\)*"', body):
        raise Error(f"{where}: a quote that isn't escaped")
    return unescape(body, where)


def header_field(header, name):
    for line in header.strings[0].splitlines():
        if line.startswith(name + ":"):
            return line[len(name) + 1:].strip()
    return None


def forms_of(header):
    match = re.search(r"nplurals\s*=\s*(\d+)",
                      header_field(header, "Plural-Forms") or "")
    if not match:
        raise Error("a header without Plural-Forms: nplurals=")
    return int(match.group(1))


def merge(template, header, old):
    """The language's entries, as the template has them, with what each had
    in the language before."""
    forms = forms_of(header)
    by_key = {e.key(): e for e in old}
    used = set()
    merged = []
    for t in template:
        entry = Entry(t.id, t.context, t.plural)
        entry.extracted = list(t.extracted)
        entry.references = list(t.references)
        entry.strings = [""] * (forms if t.plural is not None else 1)
        had = by_key.get(t.key())
        if had is not None and (had.plural is None) == (t.plural is None):
            used.add(had.key())
            entry.comments = had.comments
            entry.flags = [f for f in had.flags if f == "fuzzy"]
            entry.strings = (had.strings + entry.strings)[:len(entry.strings)]
        elif (near := nearest(t, old)) is not None:
            used.add(near.key())
            entry.comments = near.comments
            entry.flags = ["fuzzy"]
            entry.strings = list(near.strings)
        merged.append(entry)
    # What's gone, translated, stays at the end for a string that comes back.
    gone = []
    for e in old:
        if e.key() not in used and any(e.strings):
            e = copy.copy(e)
            e.obsolete = True
            e.extracted, e.references = [], []
            gone.append(e)
    return merged + gone


def nearest(t, old):
    """The old translation of a string most like this one, in its context,
    for one that changed a little."""
    best, alike = None, ALIKE
    for e in old:
        if e.context != t.context or (e.plural is None) != (t.plural is None):
            continue
        if not any(e.strings):
            continue
        ratio = difflib.SequenceMatcher(None, e.id, t.id).ratio()
        if ratio >= alike:
            best, alike = e, ratio
    return best


def languages():
    return sorted(PO.glob("*.po"))


def command_extract(_):
    entries = extract()
    TEMPLATE.write_text(write_po(HEADER, entries), encoding="utf-8")
    print(f"{TEMPLATE.relative_to(ROOT)}: {len(entries)} strings")


def command_update(_):
    _, _, template = read_po(TEMPLATE)
    for path in languages():
        header_text, header, old = read_po(path)
        merged = merge(template, header, old)
        path.write_text(write_po(header_text, merged), encoding="utf-8")
        live = [e for e in merged if not e.obsolete]
        done = sum(e.translated() for e in live)
        print(f"{path.relative_to(ROOT)}: {done} of {len(live)} translated")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(required=True)
    commands.add_parser(
        "extract", help="write the template from the sources",
    ).set_defaults(run=command_extract)
    commands.add_parser(
        "update", help="merge the template into each language",
    ).set_defaults(run=command_update)
    args = parser.parse_args()
    try:
        args.run(args)
    except Error as error:
        sys.exit(f"po.py: {error}")


if __name__ == "__main__":
    main()
