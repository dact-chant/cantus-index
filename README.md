# Cantus Index API Documentation

## `json-cid` (get concordances by Cantus ID)
The endpoint `https://cantusindex.org/json-cid/{CantusID}` retrieves the chants from CI databases which have the specified Cantus ID.

## `json-cid-mel` (get melodies by Cantus ID)
The endpoint `https://cantusindex.org/json-cid-mel/{CantusID}` retrieves the chants from CI databases which have the specified Cantus ID. It filters the results to include only those records where the 'melody' field is not null.

## `json-merged-chants` (get all Cantus ID merging actions)
The endpoint `https://cantusindex.org/json-merged-chants` provides a list of merged chant Cantus IDs, logging changes from old Cantus IDs to new Cantus IDs, including the dates of these changes.

### Response Format:

Each record in the JSON response includes the following fields:
- **id**: Unique identifier for the merged action.
- **old**: The old Cantus ID.
- **new**: The new Cantus ID replacing the old one.
- **date**: The date when the change was made (formatted as `YYYY-MM-DD`).

## `json-nextchants` (get next chants by Cantus ID)
The endpoint `https://cantusindex.org/json-nextchants/{CantusID}` retrieves the Cantus IDs which usually follow after the specified Cantus ID. These data are usually displayed to indexers as "Chant suggestions" in Cantus Input Tools.  

## `json-nextfeasts` (get next feasts by Feast ID)
(under construction)  

## `json-text` (get chants by search string)
The endpoint `https://cantusindex.org/json-text/{searchString}`  retrieves chants from the Cantus Index based on a given search string.
It provides two levels of search: (1) Initial Search: Retrieves chants whose full text starts with the specified search string. (2) Fallback Search: If the initial search returns fewer than 50 results, it retrieves chants containing the search string anywhere in their full text. 
The results include metadata about the chant, such as its Cantus ID, the full text of the chant, and its genre.

# Concordance Data Export for External Databases

External databases that wish to provide concordance data to **Cantus Index (CI)** should set up an automated export that runs **at least once per day**. This ensures that CI retrieves the latest concordance data for display on the CI website, in the Cantus Analysis Tool, and for json-cid API responses.

## Export Endpoint

Cantus Index will request the exported file daily from a specified endpoint, such as:
   https://yourchantdatabase.org/concordances-export.json
The export should be in JSON format and follow the specific structure below.

### Expected JSON Structure for Concordance Exports

Each external database should format its JSON export as a list of chant records, with each record containing the following fields.
Fields marked with an asterisk (*) are obligatory and must be included in every record. Other fields are optional but recommended when data is available.

- **siglum** (*): Abbreviation for the source manuscript or collection (e.g., `"A-ABC Fragm. 1"`).
- **srclink** (*): URL link to the source in the external database (e.g., `"https://yourdatabase.org/source/123"`).
- **chantlink** (*): URL link directly to the chant entry in the external database (e.g., `"https://yourdatabase.org/chant/45678"`).
- **folio** (*): Folio information for the chant (e.g., `"001v"`).
- **sequence**: The order of the chant on the folio (e.g., `"1"`).
- **incipit** (*): The opening words or phrase of the chant (e.g., `"Non sufficiens sibi semel aspexisse vis "`).
- **feast**: Feast or liturgical occasion associated with the chant (e.g., `"Nativitas Mariae"`).
- **genre**: Genre of the chant, such as antiphon (A), responsory (R), hymn (H), etc. (e.g., `"V"`).
- **office**: The office in which the chant is used, such as Matins (M) or Lauds (L) (e.g., `"M"`).
- **position**: Liturgical position of the chant in the office (e.g., `"01"`).
- **cantus_id** (*): The unique Cantus ID associated with the chant (e.g., `"007129a"`).
- **melody_id**: The unique Melody ID associated with the chant (e.g., `"001216m1"`).
- **image**: URL link to an image of the manuscript page, if available (e.g., `"https://yourdatabase.org/image/12345"`).
- **mode**: Mode of the chant, if available (e.g., `"1"`).
- **full_text**: Full text of the chant (e.g., `"Non sufficiens sibi semel aspexisse vis amoris multiplicavit in ea inten]tionem inquisitionis"`).
- **melody**: Melody encoded in Volpiano, if available (e.g., `"1---dH---h7--h--ghgfed--gH---h--h---"`).
- **db** (*): Code for the database providing the data, used for identification within CI (e.g., `"DBcode"`).

### Example JSON Export

```json
[
  {
    "siglum": "A-ABC Fragm. 1",
    "srclink": "https://yourdatabase.org/source/123",
    "chantlink": "https://yourdatabase.org/chant/45678",
    "folio": "001v",
    "sequence": "1",
    "incipit": "Non sufficiens sibi semel aspexisse vis",
    "feast": "Nativitas Mariae",
    "genre": "V",
    "office": "M",
    "position": "01",
    "cantus_id": "007129a",
    "melody_id": "001216m1",
    "image": "https://yourdatabase.org/image/12345",
    "mode": "1",
    "full_text": "Non sufficiens sibi semel aspexisse vis amoris multiplicavit in ea inten]tionem inquisitionis",
    "melody": "1---dH---h7--h--ghgfed--gH---h--h---",
    "db": "DBcode"
  },
  ...
]
