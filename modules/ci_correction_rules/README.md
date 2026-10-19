# CI Correction rules

This Backdrop module adds an editable **CI Correction rule** content type. Each
rule stores a category, literal text to find, replacement text, match type, and
optional case-insensitive exceptions. Titles are generated as `Find → Replace
with` whenever a rule is saved.

Enable the module at `admin/modules`. Installation creates the content type and
its fields, then imports the spelling rules captured from Latinchant's existing
correction behavior. The module adds a paged rule list at
`admin/content/ci-correction-rules`, a direct review page at
`admin/content/ci-correction-review`, and a change history at
`admin/content/ci-correction-audit`. Review and rule management require the
**Administer CI correction rules** permission or **Administer nodes**.

The module exposes `ci_correction_rules_load()` to retrieve published rules
and `ci_correction_rules_apply($word, $rule)` to apply one rule to one word.
Whole-word, part-of-word, and prefix matching are literal and
case-insensitive. Spelling replacements preserve initial capitalization and
all-capital words. Capitalization rules use the replacement exactly as entered.
Exceptions exclude a word if it contains any exception, regardless of case.

The review page scans the current Cantus Index `master_chant` full texts using
published Spelling rules. Selecting a chant applies the exact displayed word
change immediately to that chant. The node update and its audit row are saved
in one database transaction. Each audit row stores the chant node ID, the
correction rule node ID (`rule_nid`), the original and replacement words, the
applying user, and timestamp. The rule and chant links appear in correction
history.

Existing rows in `aa_corrections` are retained. The old `id` values are
preserved while the column becomes auto-incrementing for new local audit rows.
Legacy rows receive a rule link only if exactly one spelling rule matches;
ambiguous older rows stay unlinked. Previously imported pending rows remain
available for one-time application from correction history. New corrections
do not enter a pending import queue.

The module also preserves the correction network tools at
`/concordance-spelling`, `/corrections/network-scan`, and
`/corrections/network-update`. Their existing `aa_correction_rules`,
`aa_correction_network_stats`, and `aaconcordances2` data is retained. Newly
applied corrections continue to add their exact word pair to the network rules
table.

The imported snapshot is in `data/legacy_spelling_rules.json`; its
`source_row` values point to the original Cantus corrections CSV row. Existing
Cantus Index data is not changed by installing this module; it creates the new
content type and rule nodes only.
