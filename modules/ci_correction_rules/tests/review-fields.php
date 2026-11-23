<?php

/**
 * @file
 * Standalone field-definition and repeatable update checks.
 */

require_once dirname(__DIR__) . '/ci_correction_rules.module';
require_once dirname(__DIR__) . '/ci_correction_rules.install';

function t($text, $args = array()) { return strtr($text, $args); }
function node_type_load($type) { return (object) array('type' => $type); }
function field_info_field($name) {
  return isset($GLOBALS['fields'][$name]) ? $GLOBALS['fields'][$name] : FALSE;
}
function field_create_field($field) {
  if (isset($GLOBALS['fields'][$field['field_name']])) {
    throw new RuntimeException('Duplicate field creation.');
  }
  $GLOBALS['fields'][$field['field_name']] = $field;
}
function field_info_instance($entity_type, $name, $bundle) {
  return isset($GLOBALS['instances'][$name]) ? $GLOBALS['instances'][$name] : FALSE;
}
function field_create_instance($instance) {
  if (isset($GLOBALS['instances'][$instance['field_name']])) {
    throw new RuntimeException('Duplicate instance creation.');
  }
  $GLOBALS['instances'][$instance['field_name']] = $instance;
}
function field_update_instance($instance) {
  $GLOBALS['instances'][$instance['field_name']] = $instance;
}
function field_get_items($entity_type, $entity, $name) {
  return isset($entity->{$name}['und']) ? $entity->{$name}['und'] : FALSE;
}
function check($condition, $message) {
  if (!$condition) { throw new RuntimeException($message); }
}

$fields = array();
$instances = array();
ci_correction_rules_install_content_type();
$status = $instances['field_ci_rule_review_status'];
check($status['required'] === TRUE, 'Review status must be required.');
check($status['default_value'] === array(array('value' => 'approved')), 'New rules must default to Approved.');
check(array_keys($fields['field_ci_rule_review_status']['settings']['allowed_values']) === array('approved', 'needs_review', 'context_required'), 'Unexpected review states.');
check($fields['field_ci_rule_usage_notes']['type'] === 'text_long' && $instances['field_ci_rule_usage_notes']['settings']['text_processing'] === 0, 'Usage notes must be long plain text.');

// Simulate an existing installation with customized existing field settings.
unset($fields['field_ci_rule_review_status'], $instances['field_ci_rule_review_status']);
unset($fields['field_ci_rule_usage_notes'], $instances['field_ci_rule_usage_notes']);
$instances['field_ci_rule_find']['widget']['weight'] = 37;
$instances['field_ci_rule_exceptions']['description'] = 'Old help text';
ci_correction_rules_update_1001();
check(count($fields) === 7 && count($instances) === 7, 'Update must add exactly two fields.');
check($instances['field_ci_rule_find']['widget']['weight'] === 37, 'Existing form configuration must be preserved.');
check($instances['field_ci_rule_exceptions']['description'] === ci_correction_rules_exceptions_description(), 'Existing Exceptions help must be updated.');
$updated = serialize(array($fields, $instances));
ci_correction_rules_update_1001();
check(serialize(array($fields, $instances)) === $updated, 'Repeating the update must be safe.');

$node = (object) array('nid' => 1, 'title' => 'adiu → adju', 'status' => 0);
$node->field_ci_rule_find['und'][0]['value'] = 'adiu';
$node->field_ci_rule_replace['und'][0]['value'] = 'adju';
$node->field_ci_rule_match_type['und'][0]['value'] = 'prefix';
$before = serialize($node);
$rule = ci_correction_rules_get_rule($node);
check($rule['review_status'] === 'approved' && $rule['usage_notes'] === '', 'Old rules must have compatible read defaults.');
check($rule['old'] === 'adiu' && $rule['title'] === 'adiu- → adju-', 'Prefix display must preserve raw expressions.');
check(serialize($node) === $before, 'Reading old rules must not change publication, fields, or stored title.');
$node->field_ci_rule_review_status['und'][0]['value'] = 'context_required';
$node->field_ci_rule_usage_notes['und'][0]['value'] = '<meaning> & notes';
$rule = ci_correction_rules_get_rule($node);
check($rule['review_status'] === 'context_required' && $rule['usage_notes'] === '<meaning> & notes', 'Explicit review status and notes must be retained.');
echo "12 field and update checks passed.\n";
