# CI Correction rules

This Backdrop module adds an editable **CI Correction rule** content type. Each
rule stores a category, literal text to find, replacement text, match type, and
optional case-insensitive exceptions. Titles are generated as `Find → Replace
with` whenever a rule is saved.

Enable the module at `admin/modules`. Installation creates the content type and
its fields, then imports the spelling rules captured from Latinchant's existing
correction behavior. The module adds a **Correction tools** link to the regular
site menu. Its pages use the Cantus Index theme and unique paths under
`/corrections` (overview) and `/correction-tools`: `/rules`, `/review`,
`/history`, `/concordance`, `/network-scan`, and `/network-update`. The
concordance overview is labeled **CI Network Correction Suggestions**.
The one-rule `/network-scan` remains available by direct URL but is omitted
from the navigation. Rule add,
edit, and delete forms also
use paths under `/correction-tools/rules`. Access is restricted to accounts
with the **debra**, **admin**, or **administrator** role.

The `/corrections` overview reports the total number of correction rules, how
many rules have applied changes, and how many Master Chants have been changed.
It links to each correction workflow. The former `/concordance-spelling` path
redirects to `/correction-tools/concordance`.

Each correction-rule node links to its normal view page. Beneath the rule,
authorized users see applied changes from `aa_corrections` and current
suggestions computed from matching Cantus Index chant full texts. Suggestions
are not stored as new audit rows until they are applied.

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

The module also preserves the concordance and correction network tools at
`/correction-tools/concordance`, `/correction-tools/network-scan`, and
`/correction-tools/network-update`. Their existing `aa_correction_rules`,
`aa_correction_network_stats`, and `aaconcordances2` data is retained. Newly
applied corrections continue to add their exact word pair to the network rules
table.

The imported snapshot is in `data/legacy_spelling_rules.json`; its
`source_row` values point to the original Cantus corrections CSV row. Existing
Cantus Index data is not changed by installing this module; it creates the new
content type and rule nodes only.
