<?php

/**
 * Lists rule matches on Cantus Index chants for direct review and application.
 */
function ci_correction_rules_review_page() {
  if (!db_table_exists('field_data_field_full_text')) {
    return array('#markup' => '<p>' . t('The Cantus Index full-text field is unavailable.') . '</p>');
  }
  if (!db_table_exists('aa_corrections') || !db_field_exists('aa_corrections', 'rule_nid')) {
    return array('#markup' => '<p>' . t('The correction audit table is not ready. Run the database updates first.') . '</p>');
  }

  $rule_filter = isset($_GET['rule_nid']) ? max(0, (int) $_GET['rule_nid']) : 0;
  if ($rule_filter) {
    $rules = ci_correction_rules_load(NULL, TRUE);
    $rules = array_values(array_filter($rules, function ($rule) use ($rule_filter) {
      return (int) $rule['nid'] === $rule_filter;
    }));
  }
  else {
    $rules = ci_correction_rules_load('spelling', TRUE);
  }
  $page_size = 5;
  $total_rules = count($rules);
  if (!$total_rules) {
    return array('#markup' => '<p>' . t('No published correction rules were found.') . '</p>');
  }

  $total_pages = (int) ceil($total_rules / $page_size);
  $page = isset($_GET['page']) ? max(0, (int) $_GET['page']) : 0;
  $page = min($page, $total_pages - 1);
  $page_rules = array_slice($rules, $page * $page_size, $page_size);
  $affected = array();
  $node_words = array();
  $rule_rows = array();
  $rule_row_indexes = array();
  $pending_chants = array();
  $page_rule_nids = array_map(function ($rule) {
    return (int) $rule['nid'];
  }, $page_rules);
  $applied_counts = array();
  $applied_query = db_select('aa_corrections', 'c');
  $applied_query->innerJoin('node', 'n', 'n.nid = c.nid');
  $applied_query->addField('c', 'rule_nid');
  $applied_query->addExpression('COUNT(DISTINCT c.nid)', 'chant_count');
  $applied_query->condition('n.type', 'master_chant');
  $applied_query->condition('c.applied', 1);
  $applied_query->condition('c.rule_nid', $page_rule_nids, 'IN');
  $applied_query->groupBy('c.rule_nid');
  foreach ($applied_query->execute() as $count_row) {
    $applied_counts[(int) $count_row->rule_nid] = (int) $count_row->chant_count;
  }
  $match_labels = array(
    'whole_word' => t('Whole word'),
    'part_of_word' => t('Part of word'),
    'prefix' => t('Prefix'),
  );

  foreach ($page_rules as $rule) {
    $rule_nid = (int) $rule['nid'];
    $rule_rows[] = array(
      l($rule['title'], 'node/' . (int) $rule['nid']),
      check_plain($rule['old']),
      check_plain($rule['new']),
      isset($match_labels[$rule['match_type']]) ? $match_labels[$rule['match_type']] : '',
      nl2br(check_plain($rule['exceptions'])),
      isset($applied_counts[$rule_nid]) ? $applied_counts[$rule_nid] : 0,
      0,
    );
    $rule_row_indexes[$rule_nid] = count($rule_rows) - 1;

    if ($rule['old'] === '') {
      continue;
    }

    $result = db_select('field_data_field_full_text', 'f')
      ->fields('f', array('entity_id', 'field_full_text_value'))
      ->condition('f.bundle', 'master_chant')
      ->condition('f.deleted', 0)
      ->condition('f.field_full_text_value', '%' . db_like($rule['old']) . '%', 'LIKE')
      ->orderBy('f.entity_id', 'ASC')
      ->execute();

    foreach ($result as $row) {
      $nid = (int) $row->entity_id;
      $text = (string) $row->field_full_text_value;
      if (!isset($node_words[$nid])) {
        $node_words[$nid] = preg_split('/\s+/u', $text, -1, PREG_SPLIT_NO_EMPTY);
      }

      foreach ($node_words[$nid] as $segment) {
        preg_match_all('/[\p{L}\p{M}]+/u', $segment, $matches);
        foreach ($matches[0] as $old_word) {
          $new_word = ci_correction_rules_apply($old_word, $rule);
          if ($new_word === $old_word) {
            continue;
          }

          $key = (int) $rule['nid'] . "\0" . $old_word . "\0" . $new_word;
          $affected[$key]['rule_nid'] = (int) $rule['nid'];
          $affected[$key]['old'] = $old_word;
          $affected[$key]['new'] = $new_word;
          $affected[$key]['links'][$nid] = l($nid, 'node/' . $nid);
          $pending_chants[$rule_nid][$nid] = TRUE;
        }
      }
    }
  }

  foreach ($pending_chants as $rule_nid => $chants) {
    $rule_rows[$rule_row_indexes[$rule_nid]][6] = count($chants);
  }

  $navigation = array();
  if ($page > 0) {
    $navigation[] = l(t('Previous corrections'), 'correction-tools/review', array('query' => array('page' => $page - 1)));
  }
  $navigation[] = t('Page @current of @total', array('@current' => $page + 1, '@total' => $total_pages));
  if ($page + 1 < $total_pages) {
    $navigation[] = l(t('Next corrections'), 'correction-tools/review', array('query' => array('page' => $page + 1)));
  }

  $build['intro'] = array(
    '#markup' => '<p>' . t('Select the chant corrections to apply now. Selected changes are written directly to each chant and recorded in the correction history.')
      . ' ' . l(t('View correction history'), 'correction-tools/history') . '</p>',
  );
  $build['rules'] = array(
    '#theme' => 'table',
    '#header' => array(t('Correction rule'), t('Find'), t('Replace with'), t('Match type'), t('Exceptions'), t('Applied'), t('Pending')),
    '#rows' => $rule_rows,
  );
  $build['navigation_top'] = array('#markup' => '<p>' . implode(' | ', $navigation) . '</p>');

  if (empty($affected)) {
    $build['empty'] = array('#markup' => '<p>' . t('No affected Cantus Index chants were found for these rules.') . '</p>');
    $build['navigation_bottom'] = $build['navigation_top'];
    return $build;
  }

  $form = backdrop_get_form('ci_correction_rules_review_form', $affected, $node_words);
  $build['review_form'] = $form;
  $build['navigation_bottom'] = $build['navigation_top'];
  return $build;
}

/**
 * Builds the direct correction review form.
 */
function ci_correction_rules_review_form($form, &$form_state, $affected, $node_words) {
  $form['#attributes']['class'][] = 'cantus-corrections-form';
  $form['#attached']['js'][] = backdrop_get_path('module', 'ci_correction_rules') . '/js/cantus-corrections.js';
  $form['#selection_map'] = array();
  $form['select_all'] = array(
    '#markup' => '<p><a href="#" class="cantus-corrections-select-all">' . t('Select all') . '</a></p>',
    '#access' => FALSE,
  );
  $form['corrections'] = array(
    '#tree' => TRUE,
    '#prefix' => '<table class="cantus-corrections-table"><thead><tr><th scope="col">' . t('Correction')
      . '</th><th scope="col">' . t('Cantus Index nodes') . '</th></tr></thead><tbody>',
    '#suffix' => '</tbody></table>',
  );

  $index = 0;
  $has_selectable = FALSE;
  foreach ($affected as $item) {
    $pattern = ci_correction_rules_word_pattern($item['old']);
    $chant_count = count($item['links']);
    $chant_index = 0;

    foreach ($item['links'] as $nid => $link) {
      $words_html = array();
      foreach ($node_words[$nid] as $word) {
        $highlighted = preg_replace_callback($pattern, function ($matches) {
          return '<strong>' . check_plain($matches[0]) . '</strong>';
        }, check_plain($word));
        $words_html[] = $highlighted === NULL ? check_plain($word) : $highlighted;
      }

      $selection_id = 'selection_' . $index;
      $checkbox_id = 'cantus-correction-' . $index++;
      $title = t('Select chant @nid for correction @old to @new', array(
        '@nid' => $nid,
        '@old' => $item['old'],
        '@new' => $item['new'],
      ));
      $label = '<label class="element-invisible" for="' . check_plain($checkbox_id) . '">' . $title . '</label>';
      $row_prefix = $chant_index === 0
        ? '<tr><td>' . check_plain($item['old']) . ' &rarr; ' . check_plain($item['new']) . '</td><td><ul>'
        : '';
      $row_suffix = $chant_index + 1 === $chant_count ? '</ul></td></tr>' : '';
      $form['corrections'][$selection_id] = array(
        '#type' => 'checkbox',
        '#id' => $checkbox_id,
        '#title' => $title,
        '#title_display' => 'invisible',
        '#return_value' => $selection_id,
        '#theme_wrappers' => array(),
        '#attributes' => array('class' => array('cantus-corrections-checkbox')),
        '#prefix' => $row_prefix . '<li class="cantus-corrections-chant">',
        '#suffix' => $label . ' ' . $link . ': ' . implode(' ', $words_html) . '</li>' . $row_suffix,
      );
      $form['#selection_map'][$selection_id] = array(
        'nid' => (int) $nid,
        'rule_nid' => (int) $item['rule_nid'],
        'misspelled_word' => $item['old'],
        'correct_word' => $item['new'],
      );
      $has_selectable = TRUE;
      $chant_index++;
    }
  }

  $form['select_all']['#access'] = $has_selectable;
  $form['actions'] = array('#type' => 'actions', '#access' => $has_selectable);
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Apply selected corrections now'),
    '#attributes' => array('class' => array('cantus-corrections-submit')),
  );
  return $form;
}

/**
 * Validates direct correction selections and rejects conflicting replacements.
 */
function ci_correction_rules_review_form_validate($form, &$form_state) {
  $selected = isset($form_state['values']['corrections']) && is_array($form_state['values']['corrections'])
    ? array_filter($form_state['values']['corrections'])
    : array();
  if (!$selected) {
    form_set_error('corrections', t('Select at least one chant correction.'));
    return;
  }

  $targets = array();
  foreach ($selected as $selection_id => $value) {
    if (!isset($form['#selection_map'][$selection_id])) {
      form_set_error('corrections', t('The selection is invalid. Reload the page and try again.'));
      return;
    }
    $item = $form['#selection_map'][$selection_id];
    $key = $item['nid'] . "\0" . $item['misspelled_word'];
    if (isset($targets[$key]) && (
      $targets[$key]['correct_word'] !== $item['correct_word']
      || $targets[$key]['rule_nid'] !== $item['rule_nid']
    )) {
      form_set_error('corrections', t('Conflicting corrections were selected for the same chant and word.'));
      return;
    }
    $targets[$key] = $item;
  }
}

/**
 * Applies selected exact-word changes and records them in aa_corrections.
 */
function ci_correction_rules_review_form_submit($form, &$form_state) {
  global $user;
  $selected = isset($form_state['values']['corrections']) && is_array($form_state['values']['corrections'])
    ? $form_state['values']['corrections']
    : array();
  $by_node = array();
  foreach ($selected as $selection_id => $value) {
    if (!empty($value) && isset($form['#selection_map'][$selection_id])) {
      $item = $form['#selection_map'][$selection_id];
      $by_node[$item['nid']][] = $item;
    }
  }

  $saved_nodes = 0;
  $logged_changes = 0;
  $replaced_words = 0;
  $errors = array();
  foreach ($by_node as $nid => $items) {
    $node = node_load((int) $nid);
    $langcode = NULL;
    $delta = NULL;
    $full_text = $node ? ci_correction_rules_get_full_text($node, $langcode, $delta) : FALSE;
    if ($full_text === FALSE) {
      $errors[] = t('Chant node @nid could not be loaded or has no full text.', array('@nid' => $nid));
      continue;
    }

    $new_text = $full_text;
    $changes = array();
    foreach ($items as $item) {
      $count = 0;
      $new_text = ci_correction_rules_replace_word(
        $new_text,
        $item['misspelled_word'],
        $item['correct_word'],
        $count
      );
      if ($new_text === NULL) {
        $errors[] = t('A text-matching error occurred for chant node @nid.', array('@nid' => $nid));
        continue 2;
      }
      if ($count > 0) {
        $change_key = $item['rule_nid'] . "\0" . $item['misspelled_word'] . "\0" . $item['correct_word'];
        $changes[$change_key] = array(
          'nid' => (int) $nid,
          'rule_nid' => (int) $item['rule_nid'],
          'misspelled_word' => $item['misspelled_word'],
          'correct_word' => $item['correct_word'],
          'count' => isset($changes[$change_key]) ? $changes[$change_key]['count'] + $count : $count,
        );
      }
    }

    if ($new_text === $full_text || !$changes) {
      $errors[] = t('The selected word was no longer present in chant node @nid.', array('@nid' => $nid));
      continue;
    }

    $node->field_full_text[$langcode][$delta]['value'] = $new_text;
    $transaction = db_transaction();
    $node_logged_changes = 0;
    $node_replaced_words = 0;
    try {
      node_save($node);
      foreach ($changes as $change) {
        db_insert('aa_corrections')->fields(array(
          'nid' => $change['nid'],
          'rule_nid' => $change['rule_nid'],
          'misspelled_word' => $change['misspelled_word'],
          'correct_word' => $change['correct_word'],
          'applied' => 1,
          'applied_at' => REQUEST_TIME,
          'applied_uid' => (int) $user->uid,
        ))->execute();
        if (db_table_exists('aa_correction_rules')) {
          db_merge('aa_correction_rules')
            ->key(array(
              'misspelled_word' => $change['misspelled_word'],
              'correct_word' => $change['correct_word'],
            ))
            ->insertFields(array(
              'created' => REQUEST_TIME,
              'active' => 1,
            ))
            ->execute();
        }
        $node_logged_changes++;
        $node_replaced_words += $change['count'];
      }
      $saved_nodes++;
      $logged_changes += $node_logged_changes;
      $replaced_words += $node_replaced_words;
      unset($transaction);
    }
    catch (Exception $e) {
      $transaction->rollback();
      unset($transaction);
      watchdog_exception('ci_correction_rules', $e);
      $errors[] = t('Changes to chant node @nid could not be saved.', array('@nid' => $nid));
    }
  }

  backdrop_set_message(t('Updated @nodes chants, recorded @changes corrections, and replaced @words words.', array(
    '@nodes' => $saved_nodes,
    '@changes' => $logged_changes,
    '@words' => $replaced_words,
  )));
  if ($errors) {
    backdrop_set_message(theme('item_list', array('items' => $errors)), 'warning');
  }
  $form_state['redirect'] = 'correction-tools/review';
}

/**
 * Shows applied changes and preserves already-imported pending records.
 */
function ci_correction_rules_audit_page() {
  if (!db_table_exists('aa_corrections') || !db_field_exists('aa_corrections', 'rule_nid')) {
    return array('#markup' => '<p>' . t('The correction audit table is not ready. Run the database updates first.') . '</p>');
  }

  ci_correction_rules_cleanup_pending_records();
  $pending = (int) db_query('SELECT COUNT(*) FROM {aa_corrections} WHERE applied = 0')->fetchField();
  $query = db_select('aa_corrections', 'c')
    ->fields('c', array('id', 'nid', 'rule_nid', 'misspelled_word', 'correct_word', 'applied', 'applied_at', 'applied_uid'))
    ->orderBy('c.id', 'DESC')
    ->extend('PagerDefault')
    ->limit(50);
  $rows = array();
  foreach ($query->execute() as $record) {
    $chant = node_load((int) $record->nid);
    $chant_link = $chant ? l($chant->title, 'node/' . $chant->nid) : t('Missing node @nid', array('@nid' => $record->nid));
    $rule = !empty($record->rule_nid) ? node_load((int) $record->rule_nid) : FALSE;
    $rule_link = $rule ? l($rule->title, 'node/' . (int) $rule->nid) : t('Unlinked legacy record');
    $account = !empty($record->applied_uid) ? user_load((int) $record->applied_uid) : FALSE;
    $rows[] = array(
      (int) $record->id,
      $chant_link,
      $rule_link,
      check_plain($record->misspelled_word),
      check_plain($record->correct_word),
      $record->applied ? t('Applied') : t('Pending legacy import'),
      !empty($record->applied_at) ? format_date((int) $record->applied_at, 'short') : '-',
      $account ? check_plain($account->name) : (!empty($record->applied_uid) ? 'UID ' . (int) $record->applied_uid : '-'),
    );
  }

  $build['intro'] = array('#markup' => '<p>' . t('Applied correction history is stored in aa_corrections. @pending previously imported records are still pending and have not been changed.', array('@pending' => $pending)) . '</p>');
  $build['table'] = array(
    '#theme' => 'table',
    '#header' => array(t('ID'), t('Cantus chant'), t('Correction rule'), t('Original word'), t('Corrected word'), t('Status'), t('Applied at'), t('Applied by')),
    '#rows' => $rows,
    '#empty' => t('No correction changes have been recorded.'),
  );
  $build['pager'] = array('#theme' => 'pager');
  if ($pending) {
    $build['legacy_apply'] = backdrop_get_form('ci_correction_rules_apply_legacy_pending_form', $pending);
  }
  return $build;
}

/**
 * Offers a one-time path for the legacy imported queue without adding new rows.
 */
function ci_correction_rules_apply_legacy_pending_form($form, &$form_state, $pending) {
  $form['description'] = array('#markup' => '<p>' . t('This applies only the @count corrections imported before direct review was enabled.', array('@count' => $pending)) . '</p>');
  $form['apply'] = array(
    '#type' => 'submit',
    '#value' => t('Apply previously imported pending corrections'),
    '#disabled' => !$pending,
  );
  return $form;
}

/**
 * Applies legacy imported pending rows and records their result.
 */
function ci_correction_rules_apply_legacy_pending_form_submit($form, &$form_state) {
  global $user;
  ci_correction_rules_cleanup_pending_records();
  $rows = db_select('aa_corrections', 'c')
    ->fields('c', array('id', 'nid', 'misspelled_word', 'correct_word', 'rule_nid'))
    ->condition('c.applied', 0)
    ->orderBy('c.id', 'ASC')
    ->execute();
  $applied = 0;
  foreach ($rows as $record) {
    $node = node_load((int) $record->nid);
    $langcode = NULL;
    $delta = NULL;
    $full_text = $node ? ci_correction_rules_get_full_text($node, $langcode, $delta) : FALSE;
    if ($full_text === FALSE) {
      continue;
    }
    $count = 0;
    $new_text = ci_correction_rules_replace_word(
      $full_text,
      trim((string) $record->misspelled_word),
      (string) $record->correct_word,
      $count
    );
    if ($new_text === NULL || $new_text === $full_text || !$count) {
      continue;
    }

    $node->field_full_text[$langcode][$delta]['value'] = $new_text;
    $transaction = db_transaction();
    try {
      node_save($node);
      db_update('aa_corrections')->fields(array(
        'applied' => 1,
        'applied_at' => REQUEST_TIME,
        'applied_uid' => (int) $user->uid,
      ))->condition('id', (int) $record->id)->execute();
      $applied++;
      unset($transaction);
    }
    catch (Exception $e) {
      $transaction->rollback();
      unset($transaction);
      watchdog_exception('ci_correction_rules', $e);
    }
  }
  backdrop_set_message(t('Applied @count previously imported corrections.', array('@count' => $applied)));
  $form_state['redirect'] = 'correction-tools/history';
}

/**
 * Permanently removes stale, unapplied legacy records that cannot be applied.
 *
 * A pending record is stale if its chant no longer exists, has no full text,
 * or no longer contains the original misspelled word. The last case is treated
 * as a correction that has already been made manually.
 *
 * @param int|null $rule_nid
 *   Optional rule ID to limit cleanup to one rule.
 *
 * @return array
 *   Counts of deleted records, keyed by reason.
 */
function ci_correction_rules_cleanup_pending_records($rule_nid = NULL) {
  $deleted = array('missing_chant' => 0, 'word_not_present' => 0);
  if (!db_table_exists('aa_corrections') || !db_field_exists('aa_corrections', 'rule_nid')) {
    return $deleted;
  }

  $query = db_select('aa_corrections', 'c')
    ->fields('c', array('id', 'nid', 'misspelled_word', 'correct_word'))
    ->condition('c.applied', 0)
    ->orderBy('c.id', 'ASC');
  if ($rule_nid !== NULL) {
    $query->condition('c.rule_nid', (int) $rule_nid);
  }

  $chants = array();
  foreach ($query->execute() as $record) {
    $nid = (int) $record->nid;
    if (!array_key_exists($nid, $chants)) {
      $node = node_load($nid);
      $chants[$nid] = array(
        'exists' => (bool) $node,
        'text' => $node ? ci_correction_rules_get_full_text($node) : FALSE,
      );
    }

    $reason = NULL;
    if (!$chants[$nid]['exists']) {
      $reason = 'missing_chant';
    }
    else {
      $word_status = ci_correction_rules_pending_word_status(
        $chants[$nid]['text'],
        $record->misspelled_word
      );
      if (in_array($word_status, array('missing_text', 'word_not_present'), TRUE)) {
        $reason = 'word_not_present';
      }
    }

    if ($reason !== NULL) {
      db_delete('aa_corrections')
        ->condition('id', (int) $record->id)
        ->condition('applied', 0)
        ->execute();
      $deleted[$reason]++;
    }
  }

  $total = $deleted['missing_chant'] + $deleted['word_not_present'];
  if ($total) {
    watchdog('ci_correction_rules', 'Removed @total obsolete pending correction records (@missing missing chants, @absent original words).', array(
      '@total' => $total,
      '@missing' => $deleted['missing_chant'],
      '@absent' => $deleted['word_not_present'],
    ), WATCHDOG_NOTICE);
  }

  return $deleted;
}

/**
 * Checks whether a pending record's original word can still be found.
 *
 * @param string|false $full_text
 *   The current chant full text, or FALSE when it is unavailable.
 * @param string $misspelled_word
 *   The original word recorded by the import.
 *
 * @return string
 *   One of: missing_text, word_present, word_not_present, or uncheckable.
 */
function ci_correction_rules_pending_word_status($full_text, $misspelled_word) {
  if ($full_text === FALSE) {
    return 'missing_text';
  }
  $misspelled_word = trim((string) $misspelled_word);
  if ($misspelled_word === '') {
    return 'word_not_present';
  }

  $count = 0;
  $result = ci_correction_rules_replace_word($full_text, $misspelled_word, '', $count);
  if ($result === NULL) {
    return 'uncheckable';
  }
  return $count > 0 ? 'word_present' : 'word_not_present';
}

/**
 * Returns the first full-text field item and its language/delta keys.
 */
function ci_correction_rules_get_full_text($node, &$langcode = NULL, &$delta = NULL) {
  if (empty($node->field_full_text) || !is_array($node->field_full_text)) {
    return FALSE;
  }
  foreach ($node->field_full_text as $language => $items) {
    if (is_array($items)) {
      foreach ($items as $item_delta => $item) {
        if (isset($item['value']) && $item['value'] !== '') {
          $langcode = $language;
          $delta = $item_delta;
          return (string) $item['value'];
        }
      }
    }
  }
  return FALSE;
}

/**
 * Replaces case-sensitive whole-word matches without interpreting replacement tokens.
 */
function ci_correction_rules_replace_word($text, $old, $new, &$count = 0) {
  $count = 0;
  if ($old === '') {
    return $text;
  }
  $pattern = ci_correction_rules_word_pattern($old);
  return preg_replace_callback($pattern, function ($matches) use ($new) {
    return $new;
  }, $text, -1, $count);
}

/**
 * Fills rule_nid only where one correction rule uniquely matches a log row.
 */
function ci_correction_rules_backfill_rule_nids() {
  if (!db_table_exists('aa_corrections') || !db_field_exists('aa_corrections', 'rule_nid')) {
    return 0;
  }
  $rules = ci_correction_rules_load('spelling', FALSE);
  $unlinked = db_or()
    ->isNull('c.rule_nid')
    ->condition('c.rule_nid', 0);
  $query = db_select('aa_corrections', 'c')
    ->fields('c', array('id', 'misspelled_word', 'correct_word'))
    ->condition($unlinked);
  $updated = 0;
  foreach ($query->execute() as $record) {
    $matches = array();
    foreach ($rules as $rule) {
      if (ci_correction_rules_apply($record->misspelled_word, $rule) === $record->correct_word) {
        $matches[] = (int) $rule['nid'];
      }
    }
    if (count($matches) === 1) {
      db_update('aa_corrections')
        ->fields(array('rule_nid' => $matches[0]))
        ->condition('id', (int) $record->id)
        ->execute();
      $updated++;
    }
  }
  return $updated;
}

/**
 * Adds applied and current suggested changes below a correction-rule node.
 */
function ci_correction_rules_node_view($node, $view_mode, $langcode) {
  if ($node->type !== 'ci_correction_rule' || $view_mode !== 'full'
    || !ci_correction_rules_manage_access()) {
    return;
  }

  $node->content['ci_correction_rule_changes'] = ci_correction_rules_rule_changes_block($node);
  $node->content['ci_correction_rule_changes']['#weight'] = 100;
}

/**
 * Builds the applied and suggested change lists for one correction rule.
 */
function ci_correction_rules_rule_changes_block($node) {
  $build = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('ci-correction-rule-changes', 'block')),
    '#weight' => 100,
  );
  $build['title'] = array(
    '#markup' => '<h2>' . t('Changes for this correction rule') . '</h2>',
  );

  if (!db_table_exists('aa_corrections') || !db_field_exists('aa_corrections', 'rule_nid')) {
    $build['unavailable'] = array(
      '#markup' => '<p>' . t('Correction history is not available yet.') . '</p>',
    );
    return $build;
  }

  $rule = ci_correction_rules_get_rule($node);
  ci_correction_rules_cleanup_pending_records((int) $node->nid);
  $applied_total = (int) db_query(
    'SELECT COUNT(*) FROM {aa_corrections} WHERE rule_nid = :rule_nid AND applied = 1',
    array(':rule_nid' => (int) $node->nid)
  )->fetchField();
  $applied_query = db_select('aa_corrections', 'c')
    ->fields('c', array('nid', 'misspelled_word', 'correct_word', 'applied_at', 'applied_uid'))
    ->condition('c.rule_nid', (int) $node->nid)
    ->condition('c.applied', 1)
    ->orderBy('c.applied_at', 'DESC')
    ->range(0, 50);
  $applied_rows = array();
  foreach ($applied_query->execute() as $record) {
    $chant = node_load((int) $record->nid);
    $chant_link = $chant
      ? l($chant->title, 'node/' . (int) $chant->nid)
      : t('Missing chant @nid', array('@nid' => (int) $record->nid));
    $account = !empty($record->applied_uid) ? user_load((int) $record->applied_uid) : FALSE;
    $applied_rows[] = array(
      $chant_link,
      check_plain($record->misspelled_word),
      check_plain($record->correct_word),
      !empty($record->applied_at) ? format_date((int) $record->applied_at, 'short') : '-',
      $account ? check_plain($account->name) : '-',
    );
  }

  $build['applied_title'] = array(
    '#markup' => '<h3>' . t('Applied changes (@count)', array('@count' => $applied_total)) . '</h3>',
  );
  $build['applied_table'] = array(
    '#theme' => 'table',
    '#header' => array(t('Cantus chant'), t('Original'), t('Replacement'), t('Applied at'), t('Applied by')),
    '#rows' => $applied_rows,
    '#empty' => t('No changes have been applied for this rule.'),
  );
  if ($applied_total > 50) {
    $build['applied_more'] = array(
      '#markup' => '<p>' . t('Showing the latest 50 changes.') . ' '
        . l(t('View correction history'), 'correction-tools/history')
        . '</p>',
    );
  }
  elseif ($applied_total) {
    $build['applied_more'] = array(
      '#markup' => '<p>'
        . l(t('View correction history'), 'correction-tools/history')
        . '</p>',
    );
  }

  $suggestions = array();
  $pending_count = (int) db_query(
    'SELECT COUNT(*) FROM {aa_corrections} WHERE rule_nid = :rule_nid AND applied = 0',
    array(':rule_nid' => (int) $node->nid)
  )->fetchField();
  $pending_query = db_select('aa_corrections', 'c')
    ->fields('c', array('nid', 'misspelled_word', 'correct_word'))
    ->condition('c.rule_nid', (int) $node->nid)
    ->condition('c.applied', 0)
    ->orderBy('c.id', 'DESC')
    ->range(0, 50);
  foreach ($pending_query->execute() as $record) {
    $key = (int) $record->nid . "\0" . $record->misspelled_word . "\0" . $record->correct_word;
    $suggestions[$key] = array(
      'nid' => (int) $record->nid,
      'old' => $record->misspelled_word,
      'new' => $record->correct_word,
      'count' => 0,
      'legacy' => TRUE,
    );
  }

  $candidate_rows = array();
  if ($rule['old'] !== '' && db_table_exists('field_data_field_full_text')) {
    $candidate_query = db_select('field_data_field_full_text', 'f')
      ->fields('f', array('entity_id', 'field_full_text_value'))
      ->condition('f.bundle', 'master_chant')
      ->condition('f.deleted', 0)
      ->condition('f.field_full_text_value', '%' . db_like($rule['old']) . '%', 'LIKE')
      ->orderBy('f.entity_id', 'ASC')
      ->range(0, 101);
    $candidate_rows = $candidate_query->execute()->fetchAll();
  }
  $suggestions_truncated = count($candidate_rows) > 100 || $pending_count > 50;
  foreach (array_slice($candidate_rows, 0, 100) as $candidate) {
    foreach (ci_correction_rules_find_text_changes($candidate->field_full_text_value, $rule) as $change) {
      $key = (int) $candidate->entity_id . "\0" . $change['old'] . "\0" . $change['new'];
      if (!isset($suggestions[$key])) {
        $suggestions[$key] = array(
          'nid' => (int) $candidate->entity_id,
          'old' => $change['old'],
          'new' => $change['new'],
          'count' => 0,
          'legacy' => FALSE,
        );
      }
      $suggestions[$key]['count'] += $change['count'];
    }
  }
  ksort($suggestions, SORT_STRING);

  $suggestion_rows = array();
  foreach ($suggestions as $suggestion) {
    $chant = node_load($suggestion['nid']);
    $chant_link = $chant
      ? l($chant->title, 'node/' . $suggestion['nid'])
      : t('Missing chant @nid', array('@nid' => $suggestion['nid']));
    $status = $suggestion['count']
      ? ($suggestion['legacy'] ? t('Legacy suggestion and current match') : t('Current match'))
      : t('Previously imported suggestion');
    $suggestion_rows[] = array(
      $chant_link,
      check_plain($suggestion['old']),
      check_plain($suggestion['new']),
      $suggestion['count'] ? (int) $suggestion['count'] : '-',
      $status,
    );
  }

  $build['suggested_title'] = array(
    '#markup' => '<h3>' . t('Suggested changes shown (@count)', array('@count' => count($suggestions))) . '</h3>',
  );
  $build['suggested_intro'] = array(
    '#markup' => '<p>'
      . l(t('Review and apply suggestions for this rule'), 'correction-tools/review', array('query' => array('rule_nid' => (int) $node->nid)))
      . '</p>',
  );
  $build['suggested_table'] = array(
    '#theme' => 'table',
    '#header' => array(t('Cantus chant'), t('Current word'), t('Suggested word'), t('Matches'), t('Status')),
    '#rows' => $suggestion_rows,
    '#empty' => t('No current suggestions were found for this rule.'),
  );
  if ($suggestions_truncated) {
    $build['suggested_more'] = array(
      '#markup' => '<p>' . t('This list is limited to the first 100 matching chants and 50 pending legacy records. Use the review page to browse all current matches.') . '</p>',
    );
  }

  return $build;
}

/**
 * Finds the distinct word changes one rule would make in a chant text.
 */
function ci_correction_rules_find_text_changes($text, $rule) {
  $changes = array();
  $segments = preg_split('/\s+/u', (string) $text, -1, PREG_SPLIT_NO_EMPTY);
  foreach ($segments as $segment) {
    preg_match_all('/[\p{L}\p{M}]+/u', $segment, $matches);
    foreach ($matches[0] as $old_word) {
      $new_word = ci_correction_rules_apply($old_word, $rule);
      if ($new_word === $old_word) {
        continue;
      }
      $key = $old_word . "\0" . $new_word;
      if (!isset($changes[$key])) {
        $changes[$key] = array('old' => $old_word, 'new' => $new_word, 'count' => 0);
      }
      $changes[$key]['count']++;
    }
  }
  return array_values($changes);
}
