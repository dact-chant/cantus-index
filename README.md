# Cantus Index API Documentation (under construction)

## `json-cid` (get concordances by Cantus ID)
The endpoint `https://cantusindex.org/json-cid/{CantusID}` retrieves the chants from CI databases which have the specified Cantus ID.

## `json-cid-mel` (get melodies by Cantus ID)
The endpoint `https://cantusindex.org/json-cid-mel/{CantusID}` retrieves the chants from CI databases which have the specified Cantus ID. It filters the results to include only those records where the 'melody' field is not null.

## `json-nextchants` (get next chants by Cantus ID)
The endpoint `https://cantusindex.org/json-nextchants/{CantusID}` retrieves the Cantus IDs which usually follow after the specified Cantus ID. These data are usually displayed to indexers as "Chant suggestions".
(under construction)  

## `json-nextfeasts` (get next feasts by Feast ID)
(under construction)  

## `json-text` (get chants by search string)
The endpoint `https://cantusindex.org/json-text/{searchString}`  retrieves chants from the Cantus Index based on a given search string.
It provides two levels of search: (1) Initial Search: Retrieves chants whose full text starts with the specified search string. (2) Fallback Search: If the initial search returns fewer than 50 results, it retrieves chants containing the search string anywhere in their full text. 
The results include metadata about the chant, such as its Cantus ID, the full text of the chant, and its genre.
