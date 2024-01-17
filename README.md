# Cantus Index API Documentation (under construction)

## `json-cid` (get concordances by Cantus ID)
The endpoint `https://cantusindex.uwaterloo.ca/json-cid/{CantusID}` retrieves the chants from CI databases which have the specified Cantus ID.

## `json-cid-mel` (get melodies by Cantus ID)
The endpoint `https://cantusindex.uwaterloo.ca/json-cid-mel/{CantusID}` retrieves the chants from CI databases which have the specified Cantus ID. It filters the results to include only those records where the 'melody' field is not null.

## `json-nextchants` (get next chants by Cantus ID)
The endpoint `https://cantusindex.uwaterloo.ca/json-nextchants/{CantusID}` retrieves the Cantus IDs which usually follow after the specified Cantus ID. These data are usually displayed to indexers as "Chant suggestions".
(under construction)  

## `json-nextfeasts` (get next feasts by Feast ID)
(under construction)  

