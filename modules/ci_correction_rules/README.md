# CI Correction rules

This Backdrop module adds an editable **CI Correction rule** content type. Each
rule stores a category, literal text to find, replacement text, match type, and
optional case-insensitive exceptions. Titles are generated as `Find → Replace
with` whenever a rule is saved.

Enable the module at `admin/modules`. Installation creates the content type and
its fields, then imports the spelling rules captured from Latinchant's existing
correction behavior. The rules are ordinary content nodes, so users with the
usual node permissions can add, edit, publish, unpublish, and delete them. The
module also adds a paged overview at `admin/content/ci-correction-rules` for
users with the **Administer CI correction rules** permission.

The module exposes `ci_correction_rules_load()` to retrieve published rules
and `ci_correction_rules_apply($word, $rule)` to apply one rule to one word.
Whole-word, part-of-word, and prefix matching are literal and
case-insensitive. Spelling replacements preserve initial capitalization and
all-capital words. Capitalization rules use the replacement exactly as entered.
Exceptions exclude a word if it contains any exception, regardless of case.

The imported snapshot is in `data/legacy_spelling_rules.json`; its
`source_row` values point to the original Cantus corrections CSV row. Existing
Cantus Index data is not changed by installing this module; it creates the new
content type and rule nodes only.
