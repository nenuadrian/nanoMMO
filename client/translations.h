#ifndef NANOMMO_CLIENT_TRANSLATIONS_H_
#define NANOMMO_CLIENT_TRANSLATIONS_H_

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace nanommo {

/// How a language picks among a plural's forms by the count, as a PO file's
/// Plural-Forms says, such as Romanian's "nplurals=3; plural=(n==1 ? 0 :
/// (n==0 || (n%100 > 0 && n%100 < 20)) ? 1 : 2);": a C expression of n, with
/// ?:, ||, &&, the comparisons, %, numbers and brackets.
class PluralRule {
 public:
  /// English's: one form for 1, and the other for any other count.
  PluralRule() = default;

  /// A rule that can't be read, or that picks a form past its last, is
  /// refused.
  static absl::StatusOr<PluralRule> Parse(std::string_view forms);

  int forms() const { return forms_; }

  /// The form for the count, from 0.
  int Form(uint64_t n) const;

 private:
  int forms_ = 2;
  std::string plural_ = "n != 1";
};

/// A language's translations, from a gettext PO file: what each English
/// string is in the language, by its context, if it has one, with the rule
/// its plurals follow.
class Catalog {
 public:
  /// Reads a PO file. What's marked fuzzy, or left untranslated, is left out,
  /// and so is a translation with a placeholder, $0 to $9, that its English
  /// doesn't have, which absl::Substitute would have no argument for: each
  /// of those shows in English.
  static absl::StatusOr<Catalog> Parse(std::string_view po);

  /// The pseudo-language, from the template: each of its strings with its
  /// letters marked, a third longer, in brackets, so that the screen shows
  /// what isn't translated, and what a longer language would cut off.
  static absl::StatusOr<Catalog> PseudoFrom(std::string_view pot);

  /// The translation, or null for one there isn't.
  const char* Find(std::string_view context, std::string_view text) const;

  /// A plural's form for the count, by its English for one, or null.
  const char* Find(std::string_view context, std::string_view one,
                   uint64_t n) const;

 private:
  // By the context and the English, as gettext keys them: the two with an
  // EOT between, or the English alone. A plural has a string for each form.
  std::map<std::string, std::vector<std::string>, std::less<>> translations_;
  PluralRule plurals_;
};

/// The English made over for the pseudo-language: "[Șéțțîñgș ~~~]".
std::string Pseudo(std::string_view english);

/// A language the interface can be in: its code, which the settings keep and
/// its PO file is named by, and its name, in itself.
struct Language {
  const char* code;
  const char* name;
};

/// The pseudo-language's code: it's made from the template, nanommo.pot.
inline constexpr std::string_view kPseudo = "pseudo";

/// English, which is the code's own, Romanian, from ro.po, and the
/// pseudo-language.
inline constexpr std::array<Language, 3> kLanguages = {{
    {"", "English"},
    {"ro", "Română"},
    {"pseudo", "Pseudo"},
}};

/// Where the PO files are, next to the client.
std::string LanguageFolder();

/// Puts the interface in the language, by its code, from its PO file in the
/// folder: none for English. One that can't be read leaves it in English.
absl::Status UseLanguage(std::string_view code, const std::string& folder);

/// The text in the language in use, or the text itself where there's no
/// translation, which is all of them in English. The pointer lasts until the
/// language changes, or, untranslated, as long as the text does.
const char* Tr(const char* text);

/// In a context, for a string that could mean one of two things, or one of
/// the content's names, such as "item 8".
const char* Tr(std::string_view context, const char* text);

/// The form for the count: in English, one for 1 and many for the rest.
const char* TrN(const char* one, const char* many, uint64_t n);
const char* TrN(std::string_view context, const char* one, const char* many,
                uint64_t n);

/// An ImGui label in the language in use, with the English one as its ID
/// after "###", so that a widget stays itself in every language.
const char* TrLabel(const char* label);

/// Marks a string for the template without translating it, as gettext's N_
/// does, for a table that's translated where it's shown, and in a context,
/// which Tr is given there too.
constexpr const char* N_(const char* text) { return text; }
constexpr const char* N_(const char* /*context*/, const char* text) {
  return text;
}

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_TRANSLATIONS_H_
