<?php

/**
 * @file
 * Standalone SQL parameter and routing regression checks. Run with PHP CLI.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

define('MENU_CALLBACK', 4);
require dirname(__DIR__) . '/ci_json.module';

class CiJsonTestResponse extends Exception {
  public $data;

  public function __construct($data) {
    $this->data = $data;
  }
}

class CiJsonTestSelect {
  public function __call($name, $args) {
    if ($name === 'fetchAssoc') {
      return array('nid' => 1);
    }
    return $this;
  }
}

class CiJsonTestMissingNode {
  public function fetchField() {
    return FALSE;
  }
}

function check($condition, $message) {
  if (!$condition) {
    throw new RuntimeException($message);
  }
}

function arg($index) {
  return $GLOBALS['input'];
}

function db_select($table, $alias) {
  return new CiJsonTestSelect();
}

function db_query($sql, $params = array()) {
  $GLOBALS['queries'][] = array($sql, $params);
  if (strpos($sql, 'SELECT nid FROM {node}') !== FALSE) {
    return new CiJsonTestMissingNode();
  }
  if (strpos($sql, 'aaconcordances_copy') !== FALSE) {
    return array((object) array('cantus_id' => '001234', 'follow_count' => '2'));
  }
  $count = array_shift($GLOBALS['counts']);
  return array_fill(0, $count, (object) array('ft' => 'Text', 'cid' => '001234', 'genre' => 'A'));
}

function backdrop_json_output($data) {
  // Stop before the production callback's exit without modifying its source.
  throw new CiJsonTestResponse($data);
}

$routes = ci_json_menu();
$expected = array('json-nextchants/%', 'json-merged-feasts', 'json-merged-chants',
  'json-text', 'json-feasts', 'csv-feasts', 'xls-feasts', 'json-cids',
  'json-cid-info/%', 'json-cid-data');
check(array_keys($routes) === $expected, 'The ten legacy routes must be preserved.');
foreach ($routes as $route) {
  check(is_callable($route['page callback']), 'Every route must have a callable callback.');
}

$cases = 0;
foreach (array("O' salutaris", "' OR '1'='1", 'Sanctus%_', 'Žalm') as $input) {
  foreach (array(array(50), array(1, 2), array(0, 0, 1)) as $counts) {
    $queries = array();
    $expected_counts = $counts;
    try {
      ci_json_text_search_page();
      throw new RuntimeException('Missing JSON response.');
    } catch (CiJsonTestResponse $response) {
      check(count($response->data) === array_sum($expected_counts), 'Search results changed.');
    }
    check(count($queries) === count($expected_counts), 'Search fallback sequence changed.');
    check($queries[0][1] === array(':prefix' => $input . '%'), 'Prefix parameter changed.');
    if (count($queries) >= 2) {
      check($queries[1][1] === array(':contains' => '%' . $input . '%', ':prefix' => $input . '%'), 'Contains parameters changed.');
    }
    if (count($queries) === 3) {
      check($queries[2][1] === array(':contains' => '%' . $input . '%'), 'CID fallback parameter changed.');
    }
    foreach ($queries as $query) {
      check(strpos($query[0], $input) === FALSE, 'Input leaked into SQL text.');
      preg_match_all('/:(prefix|contains)\b/', $query[0], $matches);
      check(count($matches[0]) === count($query[1]), 'Placeholder count mismatch.');
      foreach ($matches[0] as $placeholder) {
        check(array_key_exists($placeholder, $query[1]), 'Unbound placeholder.');
      }
    }
    $cases++;
  }
}

$input = "001234' OR '1'='1";
$queries = array();
try {
  ci_json_nextchants($input);
  throw new RuntimeException('Missing next-chants response.');
} catch (CiJsonTestResponse $response) {
  check($response->data[0]['cid'] === '001234', 'Next-chants response changed.');
  check($response->data[0]['info'] === NULL, 'Missing-node helper behavior changed.');
}
check(strpos($queries[0][0], $input) === FALSE, 'CID leaked into next-chants SQL.');
check(strpos($queries[0][0], '= :cid') !== FALSE, 'Missing CID placeholder.');
check($queries[0][1] === array(':cid' => $input), 'CID parameter changed.');
echo "PASS: 10 routes, $cases search scenarios, next-chants parameter binding.\n";
