<?php

/**
 * @file
 * Standalone matching regression checks: php tests/matching.php.
 */

if (PHP_SAPI !== 'cli') {
  return;
}

require_once dirname(__DIR__) . '/ci_correction_rules.module';

$base_rule = array(
  'category' => 'spelling',
  'old' => 'eius',
  'new' => 'ejus',
  'match_type' => 'whole_word',
  'exceptions' => '',
);

$cases = array(
  array('eius', 'ejus', array()),
  array('Eius', 'Ejus', array()),
  array('EIUS', 'EJUS', array()),
  array('eiusdem', 'eiusdem', array()),
  array('Pompeius', 'Pompeius', array()),
  array('Eiusdem', 'Ejusdem', array('match_type' => 'prefix')),
  array('Pompeius', 'Pompeius', array('match_type' => 'prefix')),
  array('Pompeius', 'Pompejus', array('match_type' => 'part_of_word')),
  array('Pompeius', 'Pompeius', array('match_type' => 'part_of_word', 'exceptions' => 'pompeius')),
  array('Pompeiusque', 'Pompeiusque', array('match_type' => 'part_of_word', 'exceptions' => "unused\r\n POMPEIUS \r\n")),
  array('eiusque', 'ejusque', array('match_type' => 'part_of_word', 'exceptions' => "\n\n")),
  array('eiuseius', 'ejusejus', array('match_type' => 'part_of_word')),
  array('pascantur', 'pascantur', array('old' => 'pasca', 'new' => 'pascha', 'match_type' => 'prefix', 'exceptions' => 'pascantur')),
  array('pascanturque', 'pascanturque', array('old' => 'pasca', 'new' => 'pascha', 'match_type' => 'prefix', 'exceptions' => 'pascantur')),
  array('pascale', 'paschale', array('old' => 'pasca', 'new' => 'pascha', 'match_type' => 'prefix', 'exceptions' => 'pascantur')),
  array('Cytharizāns', 'Citharizāns', array('old' => 'cytharizāns', 'new' => 'citharizāns')),
  array('āmen', 'Āmen', array('category' => 'capitalization', 'old' => 'āmen', 'new' => 'Āmen')),
  array('Eius', 'eius', array('category' => 'capitalization', 'new' => 'eius')),
  array('eius', '$1', array('new' => '$1')),
  array('a.b', 'x', array('old' => 'a.b', 'new' => 'x')),
  array('acb', 'acb', array('old' => 'a.b', 'new' => 'x')),
  array('eius', 'eius', array('old' => '')),
  array('eius', 'eius', array('match_type' => 'unsupported')),
);

foreach ($cases as $index => $case) {
  $actual = ci_correction_rules_apply($case[0], $case[2] + $base_rule);
  if ($actual !== $case[1]) {
    fwrite(STDERR, 'Case ' . ($index + 1) . ': expected ' . var_export($case[1], TRUE)
      . ', got ' . var_export($actual, TRUE) . PHP_EOL);
    exit(1);
  }
}

echo count($cases) . ' matching regression checks passed.' . PHP_EOL;

$replacement_cases = array(
  array('eius, Eius; EIUS.', 'eius', 'ejus', 'ejus, Eius; EIUS.', 1),
  array('eiusdem Pompeius', 'eius', 'ejus', 'eiusdem Pompeius', 0),
  array('āmen (āmen)', 'āmen', 'Āmen', 'Āmen (Āmen)', 2),
  array('eius/eiusque', 'eius', 'ejus', 'ejus/eiusque', 1),
  array('eius', 'eius', '$1', '$1', 1),
);

foreach ($replacement_cases as $index => $case) {
  $replacement_count = 0;
  $actual = ci_correction_rules_replace_word($case[0], $case[1], $case[2], $replacement_count);
  if ($actual !== $case[3] || $replacement_count !== $case[4]) {
    fwrite(STDERR, 'Replacement case ' . ($index + 1) . ': expected '
      . var_export($case[3], TRUE) . ' (' . $case[4] . ' replacements), got '
      . var_export($actual, TRUE) . ' (' . $replacement_count . ' replacements).' . PHP_EOL);
    exit(1);
  }
}

echo count($replacement_cases) . ' text replacement regression checks passed.' . PHP_EOL;
