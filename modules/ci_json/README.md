# CI JSON

Data export endpoints for [Cantus Index](https://cantusindex.org), built for
Backdrop CMS 1.x. This module replaces the active endpoints of `my_json`.

## Requirements

- Backdrop CMS 1.x with Node, Field, and Taxonomy enabled; PHP 7 or later.
- The existing Cantus Index content types, fields, and vocabularies.
- Existing tables `aaconcordances_copy`, `aamerged`, and `aamergedfeasts`.

This module uses the existing site's data schema; it does not create tables or
fields. It includes the two helpers previously supplied by `my_tools`, so
`my_tools` is not a dependency. Other modules may still need `my_tools`.

## Endpoints

| Path | Output | Parameters |
| --- | --- | --- |
| `json-nextchants/{cid}` | Following chants and their fields | Cantus ID in path |
| `json-merged-feasts` | Feast name merges | None |
| `json-merged-chants` | Published chant merges | `skip`: last seen ID, default 0; `limit`: default 10000 |
| `json-text/{text}` | Chant text search | Search string in path |
| `json-feasts` | Feast data in JSON | `from`, `count` |
| `csv-feasts` | Semicolon-separated CSV | `from`, `count` |
| `xls-feasts` | Legacy TSV download named `feasts.xls` | `from`, `count` |
| `json-cids` | Master chant identifiers | None |
| `json-cid-info/{cid}` | Chant fields and reverse relationships | Cantus ID in path |
| `json-cid-data/{cid}` | Legacy chant detail response | Cantus ID in path |

Feast pagination defaults to `from=0` and `count=99999999`, matching the original
module. The `skip` merge parameter is an ID cursor, not a row offset.
`json-cid-info` requires `access content`; the other routes retain public access.
The legacy detail endpoint remains supported because it is an active URL.

## Installation and migration

1. Back up the site and place this directory in `modules/custom/ci_json`.
2. Check custom modules for direct calls to `my_json_*`. Migrate any such calls
   to their corresponding `ci_json_*` functions before disabling the old module.
3. During a maintenance window, disable `my_json` and enable **CI JSON** on the
   modules administration page. Avoid serving traffic with both route providers
   enabled. Keep the old module's files available for rollback.
4. Rebuild the menu/router cache (clearing all Backdrop caches also does this).
5. Verify all ten endpoints against representative existing data and compare
   their responses with the old module. Check anonymous access and a user with
   `access content` separately.

Rollback: disable `ci_json`, re-enable `my_json`, and rebuild caches. No data
migration is required. Rolling back also restores the original SQL vulnerability.

## Scope and compatibility

The security change binds search strings and the next-chants CID as database
parameters. Apostrophes are data rather than SQL syntax. Existing SQL `LIKE`
wildcards (`%` and `_`), search priorities, limits, response fields, permissions,
and download formats are preserved. The next-chants helper intentionally differs
from the detail exporter: it does not add reverse relationships.

Commented-out endpoints and unused historical implementations are omitted.
No aliases for the old module's PHP functions are registered.

This targeted migration does not address the other legacy access-control,
pagination, export-format, or data-model issues. Review those separately before
using this module on a site with private content.

## Validation

Run `php -l ci_json.module` and `php tests/sql-parameters.php`. The standalone
regression test captures database queries and parameters without connecting to a
site. It checks apostrophes and SQL-like text, all search fallback branches,
preserved wildcard behavior, the next-chants query, and route callbacks.
Integration testing still requires a configured Cantus Index Backdrop site.

## License

GPL-2.0-or-later, consistent with Backdrop CMS. See `LICENSE.txt`.
