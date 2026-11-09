# CI Master Chants

Backdrop 1.x module for the existing `master_chant` content type. Requires its
existing stored text fields `field_full_text` and `field_incipit`; it does not
create or migrate their definitions.

## Incipit policy

On every node save, derive `field_incipit` from `field_full_text`. Replace HTML
tags with spaces, decode entities, collapse whitespace, then take the **first
ten words**, separated by whitespace. Shorter texts are retained in full. No
ellipsis is added. The destination field's configured `max_length` is still
respected: an incipit exceeding that capacity is shortened on a word boundary.
Empty full text clears a previous incipit. Language keys are retained for translated incipits;
an untranslated incipit uses the full-text language selected by Field API.

The ten-word rule applies to both normal saves and the missing-incipit backfill.
The implementation is isolated in `ci_master_chants_generate_incipit()`.

## Installation and backfill

1. Upload `modules/ci_master_chants` and enable **CI Master Chants**.
2. Clear caches so the administrative menu is rebuilt.
3. Open `/admin/content/ci-master-chants` as a user with `administer nodes`.
4. Confirm **Backfill missing incipits** and let the batch finish.

The backfill selects candidates in SQL: only master chants with a missing,
NULL, empty, or whitespace-only incipit and nonblank full text are returned.
A LEFT JOIN includes chants without an incipit field row. The batch loads only
these candidates, 25 at a time, including unpublished nodes. It checks blank
values again after loading each node to preserve concurrent edits. It can be
rerun safely. Keyset pagination (`nid > last_nid`) avoids skipping candidates
as the preceding batch fills incipits and removes them from the SQL result.
There is no automatic full-database scan on module installation or page visits.

Only the incipit field is written through Field API on the current revision;
other fields, node modification timestamps, and prior revisions are preserved.
Entity and listing caches are cleared. Historical revisions are not backfilled.
Run during a quiet editing period to avoid concurrent edits to the same chant.

Regular saves always recalculate the incipit, including an existing nonblank
value. Imports must use `node_save()` to invoke the hook; direct SQL writes do
not run node hooks. Disable any former incipit generator to avoid competing
writers. This module does not change Cantus ID assignment.
