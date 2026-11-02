# CI Paired References

Backdrop module for paired chant-reference fields on `master_chant` nodes.

## Managed pairs

| Reference | First field | Paired field | Display formatter |
| --- | --- | --- | --- |
| AH | `field_ah_volume` | `field_ah_item` | `vol. N, no. N` |
| CT | `field_ct_volume` | `field_ct_page_number` | `vol. N, p. N` |

The module expects these fields to exist as integer fields with unlimited
cardinality. It does not create or alter field definitions. The form combines
each pair into rows, validates that both values are present and numeric, and
saves each row at the same delta in both fields.

## Display setup

In Manage display for `master_chant`, select **Paired reference** for
`field_ah_volume` and `field_ct_volume`. Hide `field_ah_item` and
`field_ct_page_number` so each pair is rendered once.

Future paired references can be added to `ci_paired_references_definitions()`.
