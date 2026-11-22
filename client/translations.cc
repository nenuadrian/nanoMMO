#include "client/translations.h"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <bitset>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "content/text.h"

namespace nanommo {

namespace {

// Works out a Plural-Forms expression for a count, reading it as it goes, from
// the loosest of C's operators to the tightest. One it can't read, or that
// divides by nothing, isn't ok.
class Expression {
 public:
  Expression(std::string_view text, uint64_t n) : text_(text), n_(n) {}

  std::optional<uint64_t> Evaluate() {
    const uint64_t value = Ternary();
    Skip();
    if (!ok_ || at_ != text_.size()) {
      return std::nullopt;
    }
    return value;
  }

 private:
  // Expressions inside expressions, in brackets or after a ?, deeper than any
  // language's rule goes, and nowhere near deep enough to run out of stack.
  static constexpr int kDeepest = 32;

  // condition ? then : otherwise, which goes to the right.
  uint64_t Ternary() {
    if (depth_ == kDeepest) {
      ok_ = false;
      return 0;
    }
    ++depth_;
    uint64_t value = Or();
    if (Take("?")) {
      const uint64_t then = Ternary();
      ok_ = ok_ && Take(":");
      const uint64_t otherwise = Ternary();
      value = value != 0 ? then : otherwise;
    }
    --depth_;
    return value;
  }

  uint64_t Or() {
    uint64_t value = And();
    while (Take("||")) {
      const uint64_t right = And();
      value = value != 0 || right != 0 ? 1 : 0;
    }
    return value;
  }

  uint64_t And() {
    uint64_t value = Equality();
    while (Take("&&")) {
      const uint64_t right = Equality();
      value = value != 0 && right != 0 ? 1 : 0;
    }
    return value;
  }

  uint64_t Equality() {
    uint64_t value = Comparison();
    while (true) {
      if (Take("==")) {
        value = value == Comparison() ? 1 : 0;
      } else if (Take("!=")) {
        value = value != Comparison() ? 1 : 0;
      } else {
        return value;
      }
    }
  }

  uint64_t Comparison() {
    uint64_t value = Remainder();
    while (true) {
      // The two-letter ones first, or "<=" would read as "<".
      if (Take("<=")) {
        value = value <= Remainder() ? 1 : 0;
      } else if (Take(">=")) {
        value = value >= Remainder() ? 1 : 0;
      } else if (Take("<")) {
        value = value < Remainder() ? 1 : 0;
      } else if (Take(">")) {
        value = value > Remainder() ? 1 : 0;
      } else {
        return value;
      }
    }
  }

  uint64_t Remainder() {
    uint64_t value = Primary();
    while (Take("%")) {
      const uint64_t by = Primary();
      if (by == 0) {
        ok_ = false;
        return 0;
      }
      value %= by;
    }
    return value;
  }

  // n, a number, or an expression in brackets.
  uint64_t Primary() {
    if (Take("(")) {
      const uint64_t value = Ternary();
      ok_ = ok_ && Take(")");
      return value;
    }
    if (Take("n")) {
      return n_;
    }
    const std::size_t start = at_;
    while (at_ < text_.size() &&
           std::isdigit(static_cast<unsigned char>(text_[at_])) != 0) {
      ++at_;
    }
    uint64_t number = 0;
    if (!absl::SimpleAtoi(text_.substr(start, at_ - start), &number)) {
      ok_ = false;
    }
    return number;
  }

  void Skip() {
    while (at_ < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[at_])) != 0) {
      ++at_;
    }
  }

  bool Take(std::string_view token) {
    Skip();
    if (!text_.substr(at_).starts_with(token)) {
      return false;
    }
    at_ += token.size();
    return true;
  }

  std::string_view text_;
  uint64_t n_;
  std::size_t at_ = 0;
  int depth_ = 0;
  bool ok_ = true;
};

// What a PO file says of one string: its context, its English, and its
// English plural, with a translation for each form, or one.
struct Entry {
  std::string context;
  std::string id;
  std::optional<std::string> plural;
  std::vector<std::string> strings;
  bool fuzzy = false;
};

// A quoted string, as PO files and C write them, with its escapes.
absl::StatusOr<std::string> Unquote(std::string_view quoted) {
  if (quoted.size() < 2 || quoted.front() != '"' || quoted.back() != '"') {
    return absl::InvalidArgumentError("expected a string in quotes");
  }
  std::string text;
  for (std::size_t i = 1; i + 1 < quoted.size(); ++i) {
    const char c = quoted[i];
    if (c == '"') {
      return absl::InvalidArgumentError("a quote that isn't escaped");
    }
    if (c != '\\') {
      text += c;
      continue;
    }
    if (i + 2 >= quoted.size()) {
      return absl::InvalidArgumentError("a string that ends in a backslash");
    }
    switch (quoted[++i]) {
      case 'n':
        text += '\n';
        break;
      case 't':
        text += '\t';
        break;
      case 'r':
        text += '\r';
        break;
      case '"':
        text += '"';
        break;
      case '\\':
        text += '\\';
        break;
      default:
        return absl::InvalidArgumentError(
            absl::StrCat("an escape there isn't: \\", quoted.substr(i, 1)));
    }
  }
  return text;
}

std::string_view Trimmed(std::string_view line) {
  while (!line.empty() &&
         std::isspace(static_cast<unsigned char>(line.front())) != 0) {
    line.remove_prefix(1);
  }
  while (!line.empty() &&
         std::isspace(static_cast<unsigned char>(line.back())) != 0) {
    line.remove_suffix(1);
  }
  return line;
}

// Each entry in the file, the header first, as it has them: a keyword and its
// string on a line, more of the string on the lines after it, in quotes, and
// comments, of which only "#, fuzzy" matters here. Obsolete entries, "#~",
// are comments too.
absl::StatusOr<std::vector<Entry>> ReadEntries(std::string_view po) {
  std::vector<Entry> entries;
  Entry entry;
  // Whether the entry has its msgctxt or its msgid yet, and its msgid.
  bool started = false;
  bool has_id = false;
  // A "#, fuzzy" for the entry to come.
  bool fuzzy = false;
  // Where the next line in quotes goes on.
  std::string* last = nullptr;
  const auto finish = [&] {
    if (started) {
      entries.push_back(std::move(entry));
    }
    entry = Entry();
    started = false;
    has_id = false;
    last = nullptr;
  };
  const auto start = [&] {
    entry.fuzzy = fuzzy;
    fuzzy = false;
    started = true;
  };
  int number = 0;
  for (std::size_t at = 0; at <= po.size();) {
    std::size_t end = po.find('\n', at);
    if (end == std::string_view::npos) {
      end = po.size();
    }
    const std::string_view line = Trimmed(po.substr(at, end - at));
    at = end + 1;
    ++number;
    const auto error = [number](std::string_view why) {
      return absl::InvalidArgumentError(
          absl::StrCat("line ", number, ": ", why));
    };
    if (line.empty()) {
      continue;
    }
    // A comment after an entry's strings is the next one's.
    const bool complete = !entry.strings.empty();
    if (line.front() == '#') {
      if (complete) {
        finish();
      } else if (started) {
        return error("a comment in the middle of an entry");
      }
      fuzzy = fuzzy || (line.starts_with("#,") &&
                        line.find("fuzzy") != std::string_view::npos);
      continue;
    }
    if (line.front() == '"') {
      if (last == nullptr) {
        return error("a string that follows no keyword");
      }
      absl::StatusOr<std::string> more = Unquote(line);
      if (!more.ok()) {
        return error(more.status().message());
      }
      *last += *more;
      continue;
    }
    const std::size_t space = line.find_first_of(" \t");
    if (space == std::string_view::npos) {
      return error(absl::StrCat("a keyword without a string: ", line));
    }
    const std::string_view keyword = line.substr(0, space);
    absl::StatusOr<std::string> text = Unquote(Trimmed(line.substr(space)));
    if (!text.ok()) {
      return error(text.status().message());
    }
    // A new entry starts at its context, or at its English.
    if ((keyword == "msgctxt" || keyword == "msgid") && complete) {
      finish();
    }
    if (keyword == "msgctxt") {
      if (started) {
        return error("a msgctxt in the middle of an entry");
      }
      start();
      entry.context = *std::move(text);
      last = &entry.context;
    } else if (keyword == "msgid") {
      if (has_id) {
        return error("a second msgid in an entry");
      }
      if (!started) {
        start();
      }
      has_id = true;
      entry.id = *std::move(text);
      last = &entry.id;
    } else if (keyword == "msgid_plural") {
      if (!has_id || entry.plural || complete) {
        return error("a msgid_plural that isn't after a msgid");
      }
      entry.plural = *std::move(text);
      last = &*entry.plural;
    } else if (keyword == "msgstr") {
      if (!has_id || entry.plural || complete) {
        return error("a msgstr that isn't after a msgid");
      }
      entry.strings.push_back(*std::move(text));
      last = &entry.strings.back();
    } else if (keyword.starts_with("msgstr[") && keyword.ends_with("]")) {
      const std::string form = absl::StrCat(entry.strings.size());
      if (!entry.plural || keyword.substr(7, keyword.size() - 8) != form) {
        return error(absl::StrCat("expected msgstr[", form, "]"));
      }
      entry.strings.push_back(*std::move(text));
      last = &entry.strings.back();
    } else {
      return error(absl::StrCat("a keyword there isn't: ", keyword));
    }
  }
  if (started && entry.strings.empty()) {
    return absl::InvalidArgumentError("the last entry has no msgstr");
  }
  finish();
  return entries;
}

// The placeholders a string has, $0 to $9, as absl::Substitute reads them,
// for whom $$ is a dollar.
std::bitset<10> Placeholders(std::string_view text) {
  std::bitset<10> found;
  for (std::size_t i = 0; i + 1 < text.size(); ++i) {
    if (text[i] != '$') {
      continue;
    }
    const char next = text[++i];
    if (next >= '0' && next <= '9') {
      found.set(static_cast<std::size_t>(next - '0'));
    }
  }
  return found;
}

std::string Key(std::string_view context, std::string_view text) {
  return context.empty() ? std::string(text)
                         : absl::StrCat(context, "\x04", text);
}

// A header's field, such as "Plural-Forms", or nothing.
std::optional<std::string_view> HeaderField(std::string_view header,
                                            std::string_view name) {
  for (std::size_t start = 0; start < header.size();) {
    std::size_t end = header.find('\n', start);
    if (end == std::string_view::npos) {
      end = header.size();
    }
    const std::string_view line = header.substr(start, end - start);
    start = end + 1;
    if (line.starts_with(name) && line.substr(name.size()).starts_with(":")) {
      return Trimmed(line.substr(name.size() + 1));
    }
  }
  return std::nullopt;
}

}  // namespace

absl::StatusOr<PluralRule> PluralRule::Parse(std::string_view forms) {
  const std::size_t count = forms.find("nplurals=");
  const std::size_t plural = forms.find("plural=", count + 1);
  if (count == std::string_view::npos || plural == std::string_view::npos) {
    return absl::InvalidArgumentError(
        absl::StrCat("Plural-Forms without nplurals= and plural=: ", forms));
  }
  PluralRule rule;
  const std::string_view number =
      Trimmed(forms.substr(count + 9, forms.find(';', count) - count - 9));
  if (!absl::SimpleAtoi(number, &rule.forms_) || rule.forms_ < 1) {
    return absl::InvalidArgumentError(
        absl::StrCat("nplurals isn't a number of forms: ", number));
  }
  std::string_view expression = forms.substr(plural + 7);
  expression = Trimmed(expression.substr(0, expression.find(';')));
  rule.plural_ = std::string(expression);
  // Any count goes round to one of these, as the rules take n % 100 or
  // n % 10 and no more.
  for (uint64_t n = 0; n < 1000; ++n) {
    const std::optional<uint64_t> form = Expression(expression, n).Evaluate();
    if (!form) {
      return absl::InvalidArgumentError(
          absl::StrCat("a plural rule that can't be read: ", expression));
    }
    if (*form >= static_cast<uint64_t>(rule.forms_)) {
      return absl::InvalidArgumentError(
          absl::StrCat("the plural rule picks form ", *form, " for ", n,
                       ", of ", rule.forms_));
    }
  }
  return rule;
}

int PluralRule::Form(uint64_t n) const {
  const std::optional<uint64_t> form = Expression(plural_, n).Evaluate();
  return form && *form < static_cast<uint64_t>(forms_) ? static_cast<int>(*form)
                                                       : 0;
}

absl::StatusOr<Catalog> Catalog::Parse(std::string_view po) {
  absl::StatusOr<std::vector<Entry>> entries = ReadEntries(po);
  if (!entries.ok()) {
    return entries.status();
  }
  Catalog catalog;
  for (const Entry& entry : *entries) {
    // The header, which is the translation of nothing.
    if (entry.id.empty() && entry.context.empty()) {
      if (const std::optional<std::string_view> forms =
              HeaderField(entry.strings.front(), "Plural-Forms")) {
        absl::StatusOr<PluralRule> rule = PluralRule::Parse(*forms);
        if (!rule.ok()) {
          return rule.status();
        }
        catalog.plurals_ = *std::move(rule);
      }
      continue;
    }
    const std::bitset<10> english =
        Placeholders(entry.id) | Placeholders(entry.plural.value_or(""));
    const bool translated =
        !entry.fuzzy &&
        std::ranges::none_of(entry.strings, &std::string::empty) &&
        std::ranges::all_of(entry.strings, [&](const std::string& text) {
          return (Placeholders(text) & ~english).none();
        });
    if (translated) {
      catalog.translations_[Key(entry.context, entry.id)] = entry.strings;
    }
  }
  return catalog;
}

absl::StatusOr<Catalog> Catalog::PseudoFrom(std::string_view pot) {
  absl::StatusOr<std::vector<Entry>> entries = ReadEntries(pot);
  if (!entries.ok()) {
    return entries.status();
  }
  Catalog catalog;
  for (const Entry& entry : *entries) {
    if (entry.id.empty() && entry.context.empty()) {
      continue;
    }
    std::vector<std::string>& made =
        catalog.translations_[Key(entry.context, entry.id)];
    made.push_back(Pseudo(entry.id));
    if (entry.plural) {
      made.push_back(Pseudo(*entry.plural));
    }
  }
  return catalog;
}

const char* Catalog::Find(std::string_view context,
                          std::string_view text) const {
  const auto found = context.empty() ? translations_.find(text)
                                     : translations_.find(Key(context, text));
  return found != translations_.end() && found->second.size() == 1
             ? found->second.front().c_str()
             : nullptr;
}

const char* Catalog::Find(std::string_view context, std::string_view one,
                          uint64_t n) const {
  const auto found = context.empty() ? translations_.find(one)
                                     : translations_.find(Key(context, one));
  if (found == translations_.end() ||
      static_cast<int>(found->second.size()) != plurals_.forms()) {
    return nullptr;
  }
  return found->second[static_cast<std::size_t>(plurals_.Form(n))].c_str();
}

std::string Pseudo(std::string_view english) {
  // Romanian's own marks where it has them, so that the fonts show they've
  // got them, and another of Latin's elsewhere.
  static constexpr std::array<std::pair<char, std::string_view>, 20> kMarked = {
      {{'a', "ă"}, {'c', "ç"}, {'e', "é"}, {'i', "î"}, {'n', "ñ"},
       {'o', "ö"}, {'s', "ș"}, {'t', "ț"}, {'u', "ü"}, {'y', "ý"},
       {'A', "Ă"}, {'C', "Ç"}, {'E', "É"}, {'I', "Î"}, {'N', "Ñ"},
       {'O', "Ö"}, {'S', "Ș"}, {'T', "Ț"}, {'U', "Ü"}, {'Y', "Ý"}}};
  std::string made = "[";
  std::size_t letters = 0;
  for (std::size_t i = 0; i < english.size(); ++i) {
    const char c = english[i];
    // A placeholder stays as it is, for absl::Substitute.
    if (c == '$' && i + 1 < english.size()) {
      made += english.substr(i, 2);
      ++i;
      continue;
    }
    if (std::isalpha(static_cast<unsigned char>(c)) != 0) {
      ++letters;
    }
    const auto marked = std::ranges::find(
        kMarked, c, &std::pair<char, std::string_view>::first);
    if (marked != kMarked.end()) {
      made += marked->second;
    } else {
      made += c;
    }
  }
  // A third longer, as many languages are longer than English.
  absl::StrAppend(&made, " ", std::string((letters + 2) / 3, '~'), "]");
  return made;
}

namespace {

// The language in use, and the labels TrLabel has made for it, which ImGui
// only reads, so each is made once.
struct InUse {
  std::optional<Catalog> catalog;
  std::map<std::string, std::string, std::less<>> labels;
};

InUse& Current() {
  static InUse& in_use = *new InUse;
  return in_use;
}

}  // namespace

std::string LanguageFolder() {
  return std::string(GetApplicationDirectory()) + "po";
}

absl::Status UseLanguage(std::string_view code, const std::string& folder) {
  Current() = InUse();
  if (code.empty()) {
    return absl::OkStatus();
  }
  const bool pseudo = code == kPseudo;
  const std::string file = pseudo ? "nanommo.pot" : absl::StrCat(code, ".po");
  const absl::StatusOr<std::string> text =
      ReadFile(absl::StrCat(folder, "/", file));
  if (!text.ok()) {
    return text.status();
  }
  absl::StatusOr<Catalog> catalog =
      pseudo ? Catalog::PseudoFrom(*text) : Catalog::Parse(*text);
  if (!catalog.ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat(code, ": ", catalog.status().message()));
  }
  Current().catalog = *std::move(catalog);
  return absl::OkStatus();
}

const char* Tr(const char* text) { return Tr("", text); }

const char* Tr(std::string_view context, const char* text) {
  const std::optional<Catalog>& catalog = Current().catalog;
  const char* found = catalog ? catalog->Find(context, text) : nullptr;
  return found != nullptr ? found : text;
}

const char* TrN(const char* one, const char* many, uint64_t n) {
  return TrN("", one, many, n);
}

const char* TrN(std::string_view context, const char* one, const char* many,
                uint64_t n) {
  const std::optional<Catalog>& catalog = Current().catalog;
  const char* found = catalog ? catalog->Find(context, one, n) : nullptr;
  if (found != nullptr) {
    return found;
  }
  return n == 1 ? one : many;
}

const char* TrLabel(const char* label) {
  const std::string_view all = label;
  const std::string_view shown = all.substr(0, all.find("##"));
  const std::optional<Catalog>& catalog = Current().catalog;
  const char* translated =
      catalog && !shown.empty() ? catalog->Find("", shown) : nullptr;
  if (translated == nullptr) {
    return label;
  }
  auto& labels = Current().labels;
  auto made = labels.find(all);
  if (made == labels.end()) {
    made = labels.emplace(all, absl::StrCat(translated, "###", all)).first;
  }
  return made->second.c_str();
}

}  // namespace nanommo
