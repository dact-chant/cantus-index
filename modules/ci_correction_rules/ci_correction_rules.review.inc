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

  $rules = ci_correction_rules_load('spelling', TRUE);
  $page_size = 5;
  $total_rules = count($rules);
  if (!$total_rules) {
    return array('#markup' => '<p>' . t('No published spelling rules were found.') . '</p>');
  }

  $total_pages = (int) ceil($total_rules / $page_size);
  $page = isset($_GET['page']) ? max(0, (int) $_GET['page']) : 0;
  $page = min($page, $total_pages - 1);
  $page_rules = array_slice($rules, $page * $page_size, $page_size);
  $affected = array();
  $node_words = array();
  $rule_rows = array();
  $match_labels = array(
    'whole_word' => t('Whole word'),
    'part_of_word' => t('Part of word'),
    'prefix' => t('Prefix'),
  );

  foreach ($page_rules as $rule) {
    $rule_rows[] = array(
      l($rule['title'], 'node/' . (int) $rule['nid'] . '/edit'),
      check_plain($rule['old']),
      check_plain($rule['new']),
      isset($match_labels[$rule['match_type']]) ? $match_labels[$rule['match_type']] : '',
      nl2br(check_plain($rule['exceptions'])),
    );

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
        }
      }
    }
  }

  $navigation = array();
  if ($page > 0) {
    $navigation[] = l(t('Previous corrections'), 'admin/content/ci-correction-review', array('query' => array('page' => $page - 1)));
  }
  $navigation[] = t('Page @current of @total', array('@current' => $page + 1, '@total' => $total_pages));
  if ($page + 1 < $total_pages) {
    $navigation[] = l(t('Next corrections'), 'admin/content/ci-correction-review', array('query' => array('page' => $page + 1)));
  }

  $build['intro'] = array(
    '#markup' => '<p>' . t('Select the chant corrections to apply now. Selected changes are written directly to each chant and recorded in the correction history.')
      . ' ' . l(t('View correction history'), 'admin/content/ci-correction-audit') . '</p>',
  );
  $build['rules'] = array(
    '#theme' => 'table',
    '#header' => array(t('Correction rule'), t('Find'), t('Replace with'), t('Match type'), t('Exceptions')),
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
  $form_state['redirect'] = 'admin/content/ci-correction-review';
}

/**
 * Shows applied changes and preserves already-imported pending records.
 */
function ci_correction_rules_audit_page() {
  if (!db_table_exists('aa_corrections') || !db_field_exists('aa_corrections', 'rule_nid')) {
    return array('#markup' => '<p>' . t('The correction audit table is not ready. Run the database updates first.') . '</p>');
  }

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
    $rule_link = $rule ? l($rule->title, 'node/' . $rule->nid . '/edit') : t('Unlinked legacy record');
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
  $form_state['redirect'] = 'admin/content/ci-correction-audit';
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
