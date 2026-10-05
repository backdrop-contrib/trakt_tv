<?php
/**
 * @file
 * Keeps imported TV shows up to date with Trakt.
 *
 * Some show details change after a show is imported: its rating, status,
 * network, certification and website, the next episode's air date, and where
 * it can be streamed. These are refreshed a few shows at a time on cron, and
 * all at once from the settings page. Details that people edit on the site
 * (title, description, tagline, genres) are never touched.
 */

/**
 * The country used for streaming availability.
 */
define('TRAKT_TV_WATCHNOW_COUNTRY', 'us');

/**
 * How many shows each cron run refreshes. Each show takes three API calls.
 */
define('TRAKT_TV_REFRESH_PER_CRON', 20);

/**
 * Fetches the current details for a show from Trakt.
 *
 * Uses only public endpoints, so no signed-in Trakt account is needed.
 *
 * @param int $trakt_id
 *   The show's Trakt ID.
 *
 * @return array|false
 *   FALSE if the show itself could not be fetched. Otherwise an array with:
 *   - show: The show summary (extended=full).
 *   - next_episode: The next episode, NULL if none is scheduled, or FALSE if
 *     the request failed.
 *   - watchnow: Streaming sources for TRAKT_TV_WATCHNOW_COUNTRY, keyed by
 *     type (subscription, free, ...), or FALSE if the request failed.
 */
function trakt_tv_refresh_fetch($trakt_id) {
  module_load_include('inc', 'trakt_tv', 'trakt_tv.api');
  $trakt_id = (int) $trakt_id;

  $show = trakt_tv_api_get('shows/' . $trakt_id, ['extended' => 'full']);
  if ($show->code != 200 || empty($show->data['ids'])) {
    return FALSE;
  }

  // Trakt answers 204 No Content when no episode is scheduled.
  $next = trakt_tv_api_get('shows/' . $trakt_id . '/next_episode', ['extended' => 'full']);
  if ($next->code == 200) {
    $next_episode = $next->data;
  }
  elseif ($next->code == 204) {
    $next_episode = NULL;
  }
  else {
    $next_episode = FALSE;
  }

  $watchnow = trakt_tv_api_get('shows/' . $trakt_id . '/watchnow/' . TRAKT_TV_WATCHNOW_COUNTRY);
  if ($watchnow->code == 200) {
    $watchnow = $watchnow->data[TRAKT_TV_WATCHNOW_COUNTRY] ?? [];
  }
  else {
    $watchnow = FALSE;
  }

  return [
    'show' => $show->data,
    'next_episode' => $next_episode,
    'watchnow' => $watchnow,
  ];
}

/**
 * Copies fetched Trakt details onto a show node, without saving it.
 *
 * Status and rating always follow Trakt. Network, certification and website
 * are only changed when Trakt has a value, so a gap on Trakt's side does not
 * blank them. Fields that are missing from the content type are skipped.
 *
 * @param Node $node
 *   The tv_show node.
 * @param array $data
 *   Details from trakt_tv_refresh_fetch().
 */
function trakt_tv_refresh_apply(Node $node, array $data) {
  $show = $data['show'];
  $instances = field_info_instances('node', $node->type);
  $set = function ($field_name, array $items) use ($node, $instances) {
    if (isset($instances[$field_name])) {
      $node->{$field_name}[LANGUAGE_NONE] = $items;
    }
  };

  $set('field_status', isset($show['status']) && $show['status'] !== '' ? [['value' => $show['status']]] : []);
  $set('field_rating', isset($show['rating']) ? [['value' => round((float) $show['rating'], 2)]] : []);

  if (!empty($show['certification'])) {
    $set('field_certification', [['value' => $show['certification']]]);
  }
  if (!empty($show['homepage'])) {
    $set('field_website', [['url' => $show['homepage']]]);
  }
  if (!empty($show['network']) && ($tid = trakt_tv_term_tid($show['network'], 'network'))) {
    $set('field_network', [['tid' => $tid]]);
  }

  if ($data['next_episode'] !== FALSE) {
    $aired = !empty($data['next_episode']['first_aired']) ? strtotime($data['next_episode']['first_aired']) : FALSE;
    $set('field_next_episode', $aired ? [['value' => $aired]] : []);
  }

  if ($data['watchnow'] !== FALSE) {
    $items = [];
    foreach (trakt_tv_streaming_service_names($data['watchnow']) as $name) {
      if ($tid = trakt_tv_term_tid($name, 'streaming_services')) {
        $items[] = ['tid' => $tid];
      }
    }
    $set('field_streaming', $items);
  }
}

/**
 * Refreshes one show from Trakt and saves it if anything changed.
 *
 * Changes are saved without a new revision or a new "updated" date.
 *
 * @param Node $node
 *   The tv_show node.
 *
 * @return string
 *   'updated', 'unchanged', or 'failed' (no Trakt ID, or Trakt did not
 *   answer).
 */
function trakt_tv_refresh_node(Node $node) {
  module_load_include('inc', 'trakt_tv', 'trakt_tv.nodes');
  $trakt_id = $node->field_trakt_id[LANGUAGE_NONE][0]['value'] ?? NULL;
  $data = $trakt_id ? trakt_tv_refresh_fetch($trakt_id) : FALSE;
  if (!$data) {
    return 'failed';
  }

  $original = clone $node;
  trakt_tv_refresh_apply($node, $data);
  if (!trakt_tv_refresh_changed($original, $node)) {
    return 'unchanged';
  }
  trakt_tv_save_node_fields($node, $original);
  return 'updated';
}

/**
 * Checks whether the refresh changed any of the fields it manages.
 *
 * @param Node $original
 *   The node before trakt_tv_refresh_apply().
 * @param Node $node
 *   The node after it.
 *
 * @return bool
 *   TRUE if at least one refreshed field differs.
 */
function trakt_tv_refresh_changed(Node $original, Node $node) {
  $columns = [
    'field_status' => 'value',
    'field_rating' => 'value',
    'field_certification' => 'value',
    'field_website' => 'url',
    'field_network' => 'tid',
    'field_next_episode' => 'value',
    'field_streaming' => 'tid',
  ];
  foreach ($columns as $field_name => $column) {
    $before = $after = [];
    foreach ($original->{$field_name}[LANGUAGE_NONE] ?? [] as $item) {
      $before[] = (string) ($item[$column] ?? '');
    }
    foreach ($node->{$field_name}[LANGUAGE_NONE] ?? [] as $item) {
      $after[] = (string) ($item[$column] ?? '');
    }
    // Decimals load as "8.50" but are set as 8.5.
    if ($field_name == 'field_rating') {
      $before = array_map('floatval', $before);
      $after = array_map('floatval', $after);
    }
    if ($before != $after) {
      return TRUE;
    }
  }
  return FALSE;
}

/**
 * Picks the streaming services worth listing from Trakt's watchnow data.
 *
 * Only subscription and free services are kept: rent/buy and cinema listings
 * are not "streaming on". Add-on channels sold through another service
 * ("Acorn TV (on Amazon)") are left out in favour of the service itself.
 *
 * @param array $watchnow
 *   Trakt watchnow data for one country, keyed by type.
 *
 * @return string[]
 *   Service display names, in Trakt's order, without duplicates.
 */
function trakt_tv_streaming_service_names(array $watchnow) {
  $sources = trakt_tv_watchnow_sources();
  $names = [];
  foreach (['subscription', 'free'] as $type) {
    foreach ($watchnow[$type] ?? [] as $entry) {
      $source = $sources[$entry['source']] ?? NULL;
      if (!$source || !empty($source['images']['channel'])) {
        continue;
      }
      $names[$source['name']] = $source['name'];
    }
  }
  return array_values($names);
}

/**
 * Returns Trakt's streaming sources for the country, keyed by source ID.
 *
 * Cached for a day; the list rarely changes.
 *
 * @return array
 *   Source definitions (name, color, images, ...) keyed by source ID.
 */
function trakt_tv_watchnow_sources() {
  $sources = &backdrop_static(__FUNCTION__);
  if (isset($sources)) {
    return $sources;
  }
  $cid = 'trakt_tv:watchnow_sources:' . TRAKT_TV_WATCHNOW_COUNTRY;
  if ($cache = cache_get($cid)) {
    $sources = $cache->data;
    return $sources;
  }

  module_load_include('inc', 'trakt_tv', 'trakt_tv.api');
  $sources = [];
  $response = trakt_tv_api_get('watchnow/sources/' . TRAKT_TV_WATCHNOW_COUNTRY);
  if ($response->code == 200) {
    foreach ($response->data as $group) {
      foreach ($group[TRAKT_TV_WATCHNOW_COUNTRY] ?? [] as $source) {
        $sources[$source['source']] = $source;
      }
    }
    cache_set($cid, $sources, 'cache', REQUEST_TIME + 86400);
  }
  return $sources;
}

/**
 * Finds a term by name in a vocabulary, creating it if needed.
 *
 * @param string $name
 *   The term name.
 * @param string $vocabulary
 *   The vocabulary machine name.
 *
 * @return int|null
 *   The term ID, or NULL if the name is empty.
 */
function trakt_tv_term_tid($name, $vocabulary) {
  $name = trim($name);
  if ($name === '') {
    return NULL;
  }
  if ($terms = taxonomy_term_load_multiple_by_name($name, $vocabulary)) {
    return key($terms);
  }
  // Without a parent the term is missing from the hierarchy table and does
  // not appear on the vocabulary's admin page.
  $term = entity_create('taxonomy_term', [
    'name' => $name,
    'vocabulary' => $vocabulary,
    'parent' => [0],
  ]);
  taxonomy_term_save($term);
  return $term->tid;
}

/**
 * Returns the IDs of shows to refresh next on cron, and moves the cursor.
 *
 * Works through all shows in nid order, a batch per run, and starts over
 * once it reaches the end.
 *
 * @return array
 *   Node IDs.
 */
function trakt_tv_refresh_next_nids() {
  $cursor = state_get('trakt_tv_refresh_cursor', 0);
  $nids = trakt_tv_nids_with_trakt_id($cursor, TRAKT_TV_REFRESH_PER_CRON);
  if (count($nids) < TRAKT_TV_REFRESH_PER_CRON) {
    // End of the list: next run starts from the beginning.
    state_set('trakt_tv_refresh_cursor', 0);
    state_set('trakt_tv_refresh_cycle_finished', REQUEST_TIME);
  }
  else {
    state_set('trakt_tv_refresh_cursor', end($nids));
  }
  return $nids;
}

/**
 * Returns IDs of TV shows that have a Trakt ID.
 *
 * @param int $after_nid
 *   Only return nodes with a higher nid.
 * @param int|null $limit
 *   Maximum number of IDs, or NULL for all.
 *
 * @return array
 *   Node IDs, ascending.
 */
function trakt_tv_nids_with_trakt_id($after_nid = 0, $limit = NULL) {
  $query = db_select('node', 'n')
    ->fields('n', ['nid'])
    ->condition('n.type', 'tv_show')
    ->condition('n.nid', $after_nid, '>')
    ->orderBy('n.nid');
  $query->innerJoin('field_data_field_trakt_id', 't', "t.entity_type = 'node' AND t.entity_id = n.nid");
  if ($limit) {
    $query->range(0, $limit);
  }
  return $query->execute()->fetchCol();
}

/**
 * Refreshes the next batch of shows. Called from hook_cron().
 */
function trakt_tv_refresh_cron() {
  $counts = ['updated' => 0, 'unchanged' => 0, 'failed' => 0];
  foreach (node_load_multiple(trakt_tv_refresh_next_nids()) as $node) {
    $counts[trakt_tv_refresh_node($node)]++;
  }
  if ($counts['updated'] || $counts['failed']) {
    watchdog('trakt_tv', 'Show refresh: @updated updated, @unchanged unchanged, @failed failed.', [
      '@updated' => $counts['updated'],
      '@unchanged' => $counts['unchanged'],
      '@failed' => $counts['failed'],
    ], $counts['failed'] ? WATCHDOG_WARNING : WATCHDOG_INFO);
  }
}

/**
 * Batch definition: refresh every show from Trakt.
 */
function trakt_tv_refresh_batch() {
  return [
    'title' => t('Refreshing TV shows from Trakt'),
    'operations' => [
      ['trakt_tv_refresh_batch_operation', []],
    ],
    'finished' => 'trakt_tv_refresh_batch_finished',
    'file' => backdrop_get_path('module', 'trakt_tv') . '/trakt_tv.refresh.inc',
  ];
}

/**
 * Batch operation: refresh a few shows per request.
 */
function trakt_tv_refresh_batch_operation(&$context) {
  if (!isset($context['sandbox']['total'])) {
    $context['sandbox']['total'] = count(trakt_tv_nids_with_trakt_id());
    $context['sandbox']['done'] = 0;
    $context['sandbox']['last_nid'] = 0;
    $context['results'] = ['updated' => 0, 'unchanged' => 0, 'failed' => []];
  }
  if (empty($context['sandbox']['total'])) {
    $context['finished'] = 1;
    return;
  }

  foreach (node_load_multiple(trakt_tv_nids_with_trakt_id($context['sandbox']['last_nid'], 5)) as $node) {
    $context['sandbox']['last_nid'] = $node->nid;
    $context['sandbox']['done']++;
    $result = trakt_tv_refresh_node($node);
    if ($result == 'failed') {
      $context['results']['failed'][] = $node->title;
    }
    else {
      $context['results'][$result]++;
    }
    $context['message'] = t('Checked @title (@done of @total)', [
      '@title' => $node->title,
      '@done' => $context['sandbox']['done'],
      '@total' => $context['sandbox']['total'],
    ]);
  }

  $context['finished'] = $context['sandbox']['done'] >= $context['sandbox']['total']
    ? 1
    : $context['sandbox']['done'] / $context['sandbox']['total'];
}

/**
 * Batch finished callback for the refresh.
 */
function trakt_tv_refresh_batch_finished($success, $results, $operations) {
  if (!$success) {
    backdrop_set_message(t('The refresh stopped with an error. Run it again to start over.'), 'error');
    return;
  }
  backdrop_set_message(t('Shows refreshed from Trakt: @updated updated, @unchanged already up to date.', [
    '@updated' => $results['updated'] ?? 0,
    '@unchanged' => $results['unchanged'] ?? 0,
  ]));
  if (!empty($results['failed'])) {
    backdrop_set_message(t('Trakt did not return details for: @titles', [
      '@titles' => implode(', ', $results['failed']),
    ]), 'warning');
  }
  state_set('trakt_tv_refresh_cycle_finished', REQUEST_TIME);
  cache_clear_all();
}
