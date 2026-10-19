<?php

function ci_correction_rules_network_scan_rule($rule_id) {

  $rule_id = (int) $rule_id;

  if ($rule_id <= 0) {
    return array(
      'success' => FALSE,
      'message' => 'Invalid correction rule ID.',
    );
  }

  /*
   * Load correction rule.
   */
  $rule = db_query("
    SELECT
      id,
      misspelled_word,
      correct_word
    FROM {aa_correction_rules}
    WHERE id = :id
      AND active = 1
  ", array(
    ':id' => $rule_id,
  ))->fetchObject();

  if (!$rule) {
    return array(
      'success' => FALSE,
      'message' => 'Correction rule not found or inactive.',
    );
  }

  $misspelled_word =
    trim((string) $rule->misspelled_word);

  $correct_word =
    trim((string) $rule->correct_word);

  /*
   * Get all databases represented in aaconcordances2.
   */
  $databases = db_query("
    SELECT DISTINCT db
    FROM {aaconcordances2}
    WHERE db <> ''
    ORDER BY db
  ")->fetchCol();

  $database_results = array();

  $total_candidates = 0;
  $total_affected = 0;

  foreach ($databases as $db) {

    /*
     * Case-sensitive pre-filter.
     *
     * The db condition uses the existing db index and limits
     * the scan to one database at a time.
     */
    $result = db_query("
      SELECT
        id,
        full_text
      FROM {aaconcordances2}
      WHERE db = :db
        AND full_text IS NOT NULL
        AND BINARY full_text LIKE BINARY :word
    ", array(
      ':db' => $db,
      ':word' => '%' . $misspelled_word . '%',
    ));

    $candidate_count = 0;
    $affected_chants = 0;

    foreach ($result as $row) {

      $candidate_count++;

      $full_text =
        (string) $row->full_text;

      /*
       * Final whole-word and case-sensitive check.
       */
      $occurrences =
        ci_correction_rules_word_occurrences(
          $full_text,
          $misspelled_word
        );

      /*
       * One concordance record counts as one affected chant,
       * regardless of how many matching words it contains.
       */
      if (
        $occurrences !== FALSE &&
        $occurrences > 0
      ) {
        $affected_chants++;
      }
    }

    /*
     * Store the result, including zero results.
     */
    db_merge('aa_correction_network_stats')
      ->key(
        array(
          'rule_id' => $rule_id,
          'db' => $db,
        )
      )
      ->fields(
        array(
          'affected_chants' => $affected_chants,
          'checked_at' => REQUEST_TIME,
        )
      )
      ->execute();

    $database_results[$db] = array(
      'candidates' => $candidate_count,
      'affected' => $affected_chants,
    );

    $total_candidates += $candidate_count;
    $total_affected += $affected_chants;
  }

  return array(
    'success' => TRUE,
    'rule_id' => (int) $rule->id,
    'misspelled_word' => $misspelled_word,
    'correct_word' => $correct_word,
    'total_candidates' => $total_candidates,
    'total_affected' => $total_affected,
    'databases' => $database_results,
  );
}


/**
 * Manually scans one correction rule across all concordance databases.
 *
 * Example:
 * /correction-tools/network-scan?rule_id=1
 */
function ci_correction_rules_network_scan() {

  $rule_id = isset($_GET['rule_id'])
    ? (int) $_GET['rule_id']
    : 0;

  if ($rule_id <= 0) {
    return array(
      '#markup' =>
        '<p>Use for example: '
        . '<code>/correction-tools/network-scan?rule_id=1</code>'
        . '</p>',
    );
  }

  $result =
    ci_correction_rules_network_scan_rule(
      $rule_id
    );

  if (empty($result['success'])) {
    return array(
      '#markup' =>
        '<p>'
        . check_plain($result['message'])
        . '</p>',
    );
  }

  $rows = array();

  foreach ($result['databases'] as $db => $data) {

    $rows[] = array(
      check_plain($db),
      (int) $data['candidates'],
      (int) $data['affected'],
    );
  }

  $build = array();

  $build['rule'] = array(
    '#markup' =>
      '<p>'
      . '<strong>Rule:</strong> '
      . check_plain($result['misspelled_word'])
      . ' &rarr; '
      . check_plain($result['correct_word'])
      . '</p>',
  );

  $build['table'] = array(
    '#theme' => 'table',
    '#header' => array(
      'Database',
      'Candidates',
      'Affected chants',
    ),
    '#rows' => $rows,
    '#empty' => 'No concordance databases found.',
  );

  $build['summary'] = array(
    '#markup' =>
      '<p>'
      . '<strong>Total candidates:</strong> '
      . (int) $result['total_candidates']
      . '<br>'
      . '<strong>Total affected chants:</strong> '
      . (int) $result['total_affected']
      . '</p>'
      . '<p>Results saved to '
      . '<code>aa_correction_network_stats</code>.'
      . '</p>',
  );

  return $build;
}


/**
 * Displays a progressive updater for all active correction rules.
 *
 * The page starts automatically. One rule is processed at a time.
 * Five seconds after one request finishes, the next rule is started.
 */
function ci_correction_rules_network_update_page() {

  if (!db_table_exists('aa_correction_rules')) {
    return array(
      '#markup' =>
        '<p>The spelling correction rules table is not available.</p>',
    );
  }

  if (!db_table_exists('aa_correction_network_stats')) {
    return array(
      '#markup' =>
        '<p>The correction network statistics table is not available.</p>',
    );
  }

  if (!db_table_exists('aaconcordances2')) {
    return array(
      '#markup' =>
        '<p>The concordances table is not available.</p>',
    );
  }

  /*
   * Load all active rules once.
   *
   * Each rule is then processed by a separate AJAX request.
   */
  $rule_ids = db_query("
    SELECT id
    FROM {aa_correction_rules}
    WHERE active = 1
    ORDER BY id
  ")->fetchCol();

  $rule_ids = array_map(
    'intval',
    $rule_ids
  );

  $total = count($rule_ids);

  if ($total === 0) {
    return array(
      '#markup' =>
        '<p>No active correction rules found.</p>',
    );
  }

  /*
   * Pass updater settings to JavaScript.
   */
  backdrop_add_js(
    array(
      'ciCorrectionRulesNetworkUpdate' => array(
        'ruleIds' => $rule_ids,
        'url' => url(
          'correction-tools/network-update/run',
          array(
            'absolute' => TRUE,
          )
        ),
        'token' => backdrop_get_token(
          'ci_correction_rules_network_update'
        ),
        'delay' => 5000,
      ),
    ),
    'setting'
  );

  /*
   * Sequential updater.
   *
   * The next timeout is created only after the previous AJAX request
   * has completed, so heavy concordance scans cannot overlap.
   */
  $javascript = <<<'JS'
(function ($) {

  $(document).ready(function () {

    var settings =
      Backdrop.settings.ciCorrectionRulesNetworkUpdate;

    if (!settings) {
      return;
    }

    var ruleIds =
      settings.ruleIds || [];

    var position = 0;
    var total = ruleIds.length;
    var running = false;

    function escapeHtml(value) {
      return $('<div>').text(value).html();
    }

    function updateProgress() {
      $('#correction-network-progress').text(
        position + ' / ' + total
      );
    }

    function finishUpdate() {

      running = false;

      $('#correction-network-status').html(
        '<strong>Update completed.</strong>'
      );

      $('#correction-network-current').text('');

      $('#correction-network-reload').show();
    }

    function runNextRule() {

      if (running) {
        return;
      }

      if (position >= total) {
        finishUpdate();
        return;
      }

      running = true;

      var ruleId =
        ruleIds[position];

      $('#correction-network-status').text(
        'Updating network statistics...'
      );

      $('#correction-network-current').text(
        'Processing rule ID ' + ruleId
      );

      $.ajax({
        url: settings.url,
        type: 'POST',
        dataType: 'json',

        data: {
          rule_id: ruleId,
          token: settings.token
        },

        success: function (data) {

          if (data.success) {

            var row =
              '<tr>'
              + '<td>' + data.rule_id + '</td>'
              + '<td>' + escapeHtml(data.misspelled_word) + '</td>'
              + '<td>' + escapeHtml(data.correct_word) + '</td>'
              + '<td>' + data.total_candidates + '</td>'
              + '<td>' + data.total_affected + '</td>'
              + '<td>Updated</td>'
              + '</tr>';

            $('#correction-network-results tbody')
              .append(row);
          }
          else {

            var row =
              '<tr>'
              + '<td>' + ruleId + '</td>'
              + '<td colspan="4">'
              + escapeHtml(data.message || 'Unknown error')
              + '</td>'
              + '<td>Error</td>'
              + '</tr>';

            $('#correction-network-results tbody')
              .append(row);
          }

          position++;

          updateProgress();

          running = false;

          if (position < total) {

            $('#correction-network-current').text(
              'Next rule in 5 seconds...'
            );

            window.setTimeout(
              runNextRule,
              settings.delay
            );
          }
          else {
            finishUpdate();
          }
        },

        error: function (xhr, status, error) {

          var message =
            'Rule ID ' + ruleId + ': ' + error;

          var row =
            '<tr>'
            + '<td>' + ruleId + '</td>'
            + '<td colspan="4">'
            + escapeHtml(message)
            + '</td>'
            + '<td>Request error</td>'
            + '</tr>';

          $('#correction-network-results tbody')
            .append(row);

          $('#correction-network-status').html(
            '<strong>Update stopped because of a request error.</strong>'
          );

          $('#correction-network-current').text(
            message
          );

          running = false;

          $('#correction-network-reload').show();
        }
      });
    }

    updateProgress();

    /*
     * Start automatically one second after the page is ready.
     */
    window.setTimeout(
      runNextRule,
      1000
    );

  });

})(jQuery);
JS;

  backdrop_add_js(
    $javascript,
    array(
      'type' => 'inline',
      'scope' => 'footer',
    )
  );

  $markup = '';

  $markup .=
    '<p>'
    . 'This page progressively updates concordance spelling statistics '
    . 'for all active correction rules. '
    . 'One rule is processed at a time.'
    . '</p>';

  $markup .=
    '<p>'
    . '<strong>Progress:</strong> '
    . '<span id="correction-network-progress">0 / '
    . (int) $total
    . '</span>'
    . '</p>';

  $markup .=
    '<p id="correction-network-status">'
    . 'Update will start automatically.'
    . '</p>';

  $markup .=
    '<p id="correction-network-current"></p>';

  $markup .=
    '<p id="correction-network-reload" style="display:none;">'
    . l(
      'Start again',
        'correction-tools/network-update',
      array(
        'attributes' => array(
          'class' => array('button'),
        ),
      )
    )
    . '</p>';

  $markup .=
    '<table id="correction-network-results">'
    . '<thead>'
    . '<tr>'
    . '<th>ID</th>'
    . '<th>Misspelled word</th>'
    . '<th>Correct word</th>'
    . '<th>Candidates</th>'
    . '<th>Affected chants</th>'
    . '<th>Status</th>'
    . '</tr>'
    . '</thead>'
    . '<tbody>'
    . '</tbody>'
    . '</table>';

  return array(
    '#markup' => $markup,
  );
}


/**
 * AJAX callback for progressively updating one correction rule.
 */
function ci_correction_rules_network_update_run() {

  /*
   * Only POST requests are accepted.
   */
  if (
    empty($_SERVER['REQUEST_METHOD']) ||
    strtoupper($_SERVER['REQUEST_METHOD']) !== 'POST'
  ) {

    backdrop_json_output(
      array(
        'success' => FALSE,
        'message' => 'POST request required.',
      )
    );

    backdrop_exit();
  }

  /*
   * Validate the user-session token.
   */
  $token = isset($_POST['token'])
    ? (string) $_POST['token']
    : '';

  if (
    !backdrop_valid_token(
      $token,
      'ci_correction_rules_network_update'
    )
  ) {

    backdrop_json_output(
      array(
        'success' => FALSE,
        'message' => 'Invalid security token.',
      )
    );

    backdrop_exit();
  }

  $rule_id = isset($_POST['rule_id'])
    ? (int) $_POST['rule_id']
    : 0;

  if ($rule_id <= 0) {

    backdrop_json_output(
      array(
        'success' => FALSE,
        'message' => 'Invalid correction rule ID.',
      )
    );

    backdrop_exit();
  }

  /*
   * Scan exactly one rule and save its statistics.
   */
  $result =
    ci_correction_rules_network_scan_rule(
      $rule_id
    );

  backdrop_json_output($result);

  backdrop_exit();
}


/**
 * Public overview of spelling correction rules for the CI network.
 *
 * Concordance statistics are read only from stored scan results.
 * No concordance scan is performed during the page request.
 */
function ci_correction_rules_network_page() {

  if (!db_table_exists('aa_correction_rules')) {
    return array(
      '#markup' => '<p>The spelling correction rules table is not available.</p>',
    );
  }

  if (!db_table_exists('aa_correction_network_stats')) {
    return array(
      '#markup' => '<p>The correction network statistics table is not available.</p>',
    );
  }

  /*
   * Get database codes from already stored scan results.
   *
   * This page does not scan aaconcordances2.
   */
  $databases = db_query("
    SELECT DISTINCT db
    FROM {aa_correction_network_stats}
    WHERE db <> ''
    ORDER BY db
  ")->fetchCol();

  /*
   * Load all stored statistics into an array indexed by rule ID and DB.
   */
  $stats = array();

  $stats_result = db_query("
    SELECT
      rule_id,
      db,
      affected_chants,
      checked_at
    FROM {aa_correction_network_stats}
  ");

  foreach ($stats_result as $stat) {

    $rule_id = (int) $stat->rule_id;
    $db = (string) $stat->db;

    if (!isset($stats[$rule_id])) {
      $stats[$rule_id] = array();
    }

    $stats[$rule_id][$db] = array(
      'affected_chants' => (int) $stat->affected_chants,
      'checked_at' => (int) $stat->checked_at,
    );
  }

  /*
   * Build table header dynamically from available network databases.
   */
  $header = array(
    'Misspelled word',
    'Correct word',
  );

  foreach ($databases as $db) {
    $header[] = check_plain($db);
  }

  $header[] = 'Total';

  $rows = array();

  /*
   * Load active spelling correction rules.
   */
  $rules = db_query("
    SELECT
      id,
      misspelled_word,
      correct_word
    FROM {aa_correction_rules}
    WHERE active = 1
    ORDER BY misspelled_word, correct_word
  ");

  foreach ($rules as $rule) {

    $rule_id = (int) $rule->id;

    $row = array(
      check_plain($rule->misspelled_word),
      check_plain($rule->correct_word),
    );

    $total = 0;

    foreach ($databases as $db) {

      if (isset($stats[$rule_id][$db])) {

        $count =
          (int) $stats[$rule_id][$db]['affected_chants'];

        if ($count > 0) {

         $row[] = l(
            '<strong>' . (int) $count . '</strong>',
            'correction-tools/concordance/' . $rule_id . '/' . $db,
            array(
              'html' => TRUE,
            )
          );
        }
        else {

          $row[] = '0';
        }

        $total += $count;
      }
      else {

        $row[] = '-';
      }
    }

    $row[] = '<strong>' . (int) $total . '</strong>';

    $rows[] = $row;
  }

  $build = array();

  $build['intro'] = array(
    '#markup' =>
      '<p>'
      . 'Spelling correction suggestions for databases connected '
      . 'to the Cantus Index network. '
      . 'The numbers show how many chant records in each database '
      . 'contain the case-sensitive misspelled word. '
      . 'A dash means that the rule has not yet been checked for that database.'
      . '</p>',
  );

  $build['table'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => 'No active spelling correction rules found.',
  );

  return $build;
}

/**
 * Displays matching concordance records for one correction rule and database.
 *
 * Example:
 * /correction-tools/concordance/1/CD
 *
 * Matching uses exactly the same case-sensitive whole-word logic as the
 * correction scanner. The detail page scans only the selected database.
 */
function ci_correction_rules_network_detail_page($rule_id, $db) {

  $rule_id = (int) $rule_id;
  $db = strtoupper(trim((string) $db));

  if (
    $rule_id <= 0 ||
    $db === '' ||
    !preg_match('/^[A-Z0-9_-]+$/', $db)
  ) {
    return array(
      '#markup' => '<p>Invalid correction rule or database.</p>',
    );
  }

  if (!db_table_exists('aa_correction_rules')) {
    return array(
      '#markup' => '<p>The spelling correction rules table is not available.</p>',
    );
  }

  if (!db_table_exists('aaconcordances2')) {
    return array(
      '#markup' => '<p>The concordances table is not available.</p>',
    );
  }

  /*
   * Load correction rule.
   */
  $rule = db_query("
    SELECT
      id,
      misspelled_word,
      correct_word
    FROM {aa_correction_rules}
    WHERE id = :id
      AND active = 1
  ", array(
    ':id' => $rule_id,
  ))->fetchObject();

  if (!$rule) {
    return array(
      '#markup' => '<p>Correction rule not found or inactive.</p>',
    );
  }

  $misspelled_word =
    trim((string) $rule->misspelled_word);

  $correct_word =
    trim((string) $rule->correct_word);

  /*
   * Case-sensitive pre-filter.
   *
   * Only the selected database is scanned.
   */
  $result = db_query("
    SELECT
      id,
      nid,
      chantlink,
      full_text
    FROM {aaconcordances2}
    WHERE db = :db
      AND full_text IS NOT NULL
      AND BINARY full_text LIKE BINARY :word
    ORDER BY id
  ", array(
    ':db' => $db,
    ':word' => '%' . $misspelled_word . '%',
  ));

  $rows = array();

  $affected_chants = 0;

  foreach ($result as $record) {

    $full_text =
      (string) $record->full_text;

    /*
     * Final whole-word and case-sensitive check.
     */
    $occurrences =
      ci_correction_rules_word_occurrences(
        $full_text,
        $misspelled_word
      );

    if (
      $occurrences === FALSE ||
      $occurrences < 1
    ) {
      continue;
    }

    $affected_chants++;

    /*
     * Old -> new.
     */
    $correction =
      check_plain($misspelled_word)
      . ' &rarr; '
      . check_plain($correct_word);

    /*
     * Highlight the word that should be corrected.
     */
    $highlighted_full_text =
      ci_correction_rules_highlight_word(
        $full_text,
        $misspelled_word
      );

    /*
     * Link to the original chant record.
     */
    $chant_url = '-';

    if (!empty($record->chantlink)) {

      $url =
        trim((string) $record->chantlink);

      if ($url !== '') {

        $chant_url = l(
          $url,
          $url,
          array(
            'attributes' => array(
              'target' => '_blank',
              'rel' => 'noopener noreferrer',
            ),
            'external' => TRUE,
          )
        );
      }
    }

    $rows[] = array(
      $correction,
      $highlighted_full_text,
      $chant_url,
    );
  }

  $build = array();

  $build['summary'] = array(
    '#markup' =>
      '<p>'
      . '<strong>Database:</strong> '
      . check_plain($db)
      . '<br>'
      . '<strong>Correction:</strong> '
      . check_plain($misspelled_word)
      . ' &rarr; '
      . check_plain($correct_word)
      . '<br>'
      . '<strong>Affected chants:</strong> '
      . (int) $affected_chants
      . '</p>',
  );

  $build['table'] = array(
    '#theme' => 'table',
    '#header' => array(
      'Correction',
      'Full text',
      'URL',
    ),
    '#rows' => $rows,
    '#empty' => 'No matching chant records found.',
  );

  $build['back'] = array(
    '#markup' =>
      '<p>'
      . l(
        'Back to concordance spelling suggestions',
        'correction-tools/concordance'
      )
      . '</p>',
  );

  return $build;
}

function ci_correction_rules_word_pattern($word) {

  return
    '~(?<![\p{L}\p{M}])'
    . preg_quote($word, '~')
    . '(?![\p{L}\p{M}])~u';
}


/**
 * Counts case-sensitive occurrences of a complete word in a text.
 *
 * @param string $text
 *   Full chant text.
 * @param string $word
 *   Word to search for.
 *
 * @return int|false
 *   Number of occurrences, or FALSE on regex error.
 */
function ci_correction_rules_word_occurrences($text, $word) {

  if ($text === '' || $word === '') {
    return 0;
  }

  $pattern = ci_correction_rules_word_pattern($word);

  $count = preg_match_all(
    $pattern,
    $text,
    $matches
  );

  return $count;
}


/**
 * Highlights occurrences of the misspelled word in the full text.
 *
 * Matching is case-sensitive.
 *
 * @param string $text
 *   Full chant text.
 * @param string $word
 *   Misspelled word to highlight.
 *
 * @return string
 *   Safe HTML with highlighted occurrences.
 */
function ci_correction_rules_highlight_word($text, $word) {

  if ($text === '' || $word === '') {
    return check_plain($text);
  }

  $pattern = ci_correction_rules_word_pattern($word);

  $matches = array();

  $count = preg_match_all(
    $pattern,
    $text,
    $matches,
    PREG_OFFSET_CAPTURE
  );

  if ($count === FALSE || $count === 0) {
    return check_plain($text);
  }

  $output = '';
  $position = 0;

  foreach ($matches[0] as $match) {

    $matched_word = $match[0];
    $offset = $match[1];

    /*
     * Text before the matched word.
     */
    $output .= check_plain(
      substr(
        $text,
        $position,
        $offset - $position
      )
    );

    /*
     * Highlight the matched misspelled word.
     */
    $output .=
      '<mark><strong>'
      . check_plain($matched_word)
      . '</strong></mark>';

    $position =
      $offset + strlen($matched_word);
  }

  /*
   * Remaining text.
   */
  $output .= check_plain(
    substr($text, $position)
  );

  return $output;
}
