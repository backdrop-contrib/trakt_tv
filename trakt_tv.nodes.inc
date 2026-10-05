<?php

/**
 * @file
 * Contains node-related functions for the Trakt TV module.
 */


/**
 * Creates a node from a Trakt TV ID.
 */
function trakt_tv_create_node_from_trakt_id($trakt_id) {

  if (!user_access('save tv shows')) {
    backdrop_access_denied();
    return '<p>Access denied. You do not have permission to save TV shows.</p>';
  }
  
  $show = trakt_tv_fetch_show_details($trakt_id, 'full,images');

  if (empty($show) || !isset($show['title'])) {
    backdrop_set_message(t('Failed to fetch or invalid show data.'), 'error');
    return '<p>No data found for this TV show.</p>';
  }

  // Check if the show already exists.
  if (trakt_tv_show_exists($show['ids']['trakt'])) {
    backdrop_set_message(t('A node for this TV show already exists.'), 'warning');
    return '<p>A node for "' . check_plain($show['title']) . '" already exists.</p>';
  }

  // Create the new node.
  $node = new Node([
    'type' => 'tv_show', // Replace with your content type's machine name.
  ]);

  $node->title = $show['title'] ?? 'Unknown Title';
  $node->body = [
    'und' => [
      [
        'value' => $show['overview'] ?? 'No description available.',
        'format' => 'filtered_html',
      ],
    ],
  ];
  $node->field_rating = [
    'und' => [
      [
        'value' => $show['rating'] ?? 'None',
      ],
    ],
  ];
  $node->field_tagline = [
    'und' => [
      [
        'value' => $show['tagline'] ?? '',
      ],
    ],
  ];
  $node->field_year = [
    'und' => [
      [
        'value' => $show['year'] ?? 'Unknown',
      ],
    ],
  ];
  $node->field_status = [
    'und' => [
      [
        'value' => $show['status'] ?? '',
      ],
    ],
  ];
  $node->field_website = [
    'und' => [
      [
        'url' => $show['homepage'] ?? '',
      ],
    ],
  ];
  $node->field_certification = [
    'und' => [
      [
        'value' => $show['certification'] ?? 'None',
      ],
    ],
  ];
  $node->field_trakt_id = [
    'und' => [
      [
        'value' => $trakt_id,
      ],
    ],
  ];
  $genres = isset($show['genres']) && is_array($show['genres']) ? $show['genres'] : ['Unknown'];

  // Creates an array of all new taxonomy terms

  $new_terms = [];

  // Check if $genres is an array.
  if (isset($genres) && is_array($genres)) {
    foreach ($genres as $term_name) {
      // Check if the term already exists in the 'genres' vocabulary.
      $existing_terms = taxonomy_get_term_by_name($term_name, 'genres'); // Replace 'genres' with your vocabulary machine name.
      
      // If the term does not exist, add it to the $new_terms array.
      if (empty($existing_terms)) {
        if (!in_array($term_name, $new_terms)) {
          $new_terms[] = $term_name;
        }
      }
    }
  }

  // Add all new terms to the 'genres' vocabulary.
  foreach ($new_terms as $new_term_name) {
    $term = entity_create('taxonomy_term', [
      'name' => $new_term_name,
      'vocabulary' => 'genres',
    ]);
    taxonomy_term_save($term);
  }

  // Find tid for each new term and add term to node
  $term_key = 0;
  foreach ($genres as $genre) {
    $temp_tag = taxonomy_term_load_multiple_by_name($genre, 'genres');
    if (!empty($temp_tag)) {
      $term_tid = array_keys($temp_tag);
      $node->field_genres['und'][$term_key++]['tid'] = $term_tid[0];
    }
  }

  // Adding network term.
  $network = trim($show['network']); // Trim spaces to avoid duplicate terms.

  // Check if the term exists in the 'network' vocabulary.
  $existing_terms = taxonomy_get_term_by_name($network, 'network');

  if (!empty($existing_terms)) {
    // Term already exists, use its TID.
    $term2 = reset($existing_terms); // Get the first matching term.
    watchdog('trakt_tv', 'Found existing taxonomy term: @term', ['@term' => $network], WATCHDOG_DEBUG);
  } else {
    // Term does not exist, create it.
    $term2 = entity_create('taxonomy_term', [
      'name' => $network,
      'vocabulary' => 'network',
    ]);

    // Save the term only if it was successfully created.
    if ($term2) {
      taxonomy_term_save($term2);
      watchdog('trakt_tv', 'Created new taxonomy term: @term', ['@term' => $network], WATCHDOG_NOTICE);
    } else {
      watchdog('trakt_tv', 'Failed to create taxonomy term: @term', ['@term' => $network], WATCHDOG_ERROR);
    }
  }

  // Assign the TID to the node.
  if (!empty($term2->tid)) {
    $node->field_network['und'][0]['tid'] = $term2->tid;
  }
  // A missing or failed poster download should not stop the show being saved.
  trakt_tv_set_node_poster($node, $show);

  $node->status = 1; // Published.
  $node->promote = 1; // Promoted to the front page.
  $node->language = LANGUAGE_NONE;

  // Set default values for comment-related fields.
  $node->uid = 1; // Default author (user ID 1 is the admin).
  $node->last_comment_uid = 1; // Default to admin as the last commenter.
  $node->last_comment_timestamp = REQUEST_TIME; // Use the current time.
  $node->comment = 0; // Disable comments unless explicitly enabled.

  $node->save();


  watchdog('trakt_tv', 'Created node for TV show: @title', ['@title' => $node->title], WATCHDOG_NOTICE);
  backdrop_set_message(t('TV show "@title" has been successfully created.', ['@title' => $node->title]));
  backdrop_goto('node/' . $node->nid);
}


/**
 * Check to see if node already exists and display appropriate links.
 *
 * @param array $show
 *   The TV show data from the Trakt API.
 *
 * @return string
 *   HTML output indicating the status of the node.
 */
function trakt_tv_node_check($show) {
  $trakt_id = $show['show']['ids']['trakt'];

  $nid = trakt_tv_show_exists($trakt_id);

  if (!empty($nid)) {
    return '<b>Copied <a style="font-size: 105%;" href="/node/' . $nid . '">(View details)</a></b>';
  }
  elseif (user_access('create tv show content')) {
    return '<em><a class="add-to-db" style="font-size: 95%;" href="/trakt-tv-create-node-from-id/' . $trakt_id . '">Add to database</a></em>';
  }
  else {
    return '<em style="font-size: 95%;">No local entry</em>';
  }
}

/**
 * Check if a TV show already exists based on Trakt ID.
 *
 * @param int $trakt_id
 *   The Trakt ID of the TV show.
 *
 * @return bool
 *   TRUE if the show exists, FALSE otherwise.
 */
function trakt_tv_show_exists($trakt_id) {
  $nid = db_query("
    SELECT n.nid 
    FROM {node} n
    INNER JOIN {field_data_field_trakt_id} t ON n.nid = t.entity_id
    WHERE t.field_trakt_id_value = :trakt_id
      AND n.type = :type",
    [
      ':trakt_id' => $trakt_id,
      ':type' => 'tv_show',
    ]
  )->fetchField();

  return $nid;
}

/**
 * Download a show's poster from Trakt and save it as a managed file.
 *
 * Trakt requires images to be stored locally; linking to media.trakt.tv is
 * blocked. The medium size (600x900 WebP) is used.
 *
 * @param array $show
 *   Show data from the Trakt API, requested with extended=images.
 *
 * @return object|false
 *   The saved file entity, or FALSE if there is no poster or it failed.
 */
function trakt_tv_download_poster(array $show) {
  if (empty($show['images']['poster'][0]) || empty($show['ids']['trakt'])) {
    return FALSE;
  }

  // Trakt returns the URL without a scheme.
  $url = 'https://' . preg_replace('#^https?://#', '', $show['images']['poster'][0]);
  if (parse_url($url, PHP_URL_HOST) !== 'media.trakt.tv') {
    watchdog('trakt_tv', 'Ignored poster from unexpected host: @url', ['@url' => $url], WATCHDOG_WARNING);
    return FALSE;
  }

  $response = backdrop_http_request($url, ['timeout' => 20]);
  if ($response->code != 200 || empty($response->data)) {
    watchdog('trakt_tv', 'Poster download failed (@code) for @url', [
      '@code' => $response->code,
      '@url' => $url,
    ], WATCHDOG_WARNING);
    return FALSE;
  }

  $directory = 'public://trakt_tv/posters';
  if (!file_prepare_directory($directory, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS)) {
    watchdog('trakt_tv', 'Poster directory @dir is not writable.', ['@dir' => $directory], WATCHDOG_ERROR);
    return FALSE;
  }

  $extension = strtolower(pathinfo(parse_url($url, PHP_URL_PATH), PATHINFO_EXTENSION));
  if (!in_array($extension, ['webp', 'jpg', 'jpeg', 'png'])) {
    $extension = 'webp';
  }
  $destination = $directory . '/' . (int) $show['ids']['trakt'] . '.' . $extension;

  $file = file_save_data($response->data, $destination, FILE_EXISTS_REPLACE);
  if (!$file) {
    return FALSE;
  }

  // Make sure what we saved really is an image before attaching it.
  if (!image_get_info($file->uri)) {
    watchdog('trakt_tv', 'Downloaded poster is not a valid image: @url', ['@url' => $url], WATCHDOG_WARNING);
    file_delete($file->fid);
    return FALSE;
  }

  return $file;
}

/**
 * Set a TV show node's poster from Trakt show data. Does not save the node.
 *
 * @param Node $node
 *   The tv_show node.
 * @param array $show
 *   Show data from the Trakt API, requested with extended=images.
 *
 * @return bool
 *   TRUE if a poster was attached.
 */
function trakt_tv_set_node_poster(Node $node, array $show) {
  if (!field_info_instance('node', 'field_poster', $node->type)) {
    return FALSE;
  }

  $file = trakt_tv_download_poster($show);
  if (!$file) {
    return FALSE;
  }

  $node->field_poster[LANGUAGE_NONE][0] = [
    'fid' => $file->fid,
    'alt' => t('@title poster', ['@title' => $node->title]),
    'title' => '',
  ];
  return TRUE;
}

/**
 * Get node IDs of TV shows that have a Trakt ID but no poster yet.
 *
 * @param int $after_nid
 *   Only return nodes with a higher nid (for batch paging).
 * @param int|null $limit
 *   Maximum number of IDs, or NULL for all.
 *
 * @return array
 *   Node IDs, ascending.
 */
function trakt_tv_nids_missing_poster($after_nid = 0, $limit = NULL) {
  $query = db_select('node', 'n')
    ->fields('n', ['nid'])
    ->condition('n.type', 'tv_show')
    ->condition('n.nid', $after_nid, '>')
    ->orderBy('n.nid');
  $query->innerJoin('field_data_field_trakt_id', 't', "t.entity_type = 'node' AND t.entity_id = n.nid");
  $query->leftJoin('field_data_field_poster', 'p', "p.entity_type = 'node' AND p.entity_id = n.nid");
  $query->isNull('p.entity_id');
  if ($limit) {
    $query->range(0, $limit);
  }
  return $query->execute()->fetchCol();
}

/**
 * Batch definition: fetch posters for every TV show that lacks one.
 */
function trakt_tv_poster_backfill_batch() {
  return [
    'title' => t('Fetching TV show posters from Trakt'),
    'operations' => [
      ['trakt_tv_poster_backfill_operation', []],
    ],
    'finished' => 'trakt_tv_poster_backfill_finished',
    'file' => backdrop_get_path('module', 'trakt_tv') . '/trakt_tv.nodes.inc',
  ];
}

/**
 * Batch operation: fetch posters for a few shows per request.
 */
function trakt_tv_poster_backfill_operation(&$context) {
  module_load_include('inc', 'trakt_tv', 'trakt_tv.api');

  if (!isset($context['sandbox']['total'])) {
    $context['sandbox']['total'] = count(trakt_tv_nids_missing_poster());
    $context['sandbox']['done'] = 0;
    $context['sandbox']['last_nid'] = 0;
    $context['results'] = ['added' => 0, 'missing' => []];
  }
  if (empty($context['sandbox']['total'])) {
    $context['finished'] = 1;
    return;
  }

  foreach (trakt_tv_nids_missing_poster($context['sandbox']['last_nid'], 5) as $nid) {
    $context['sandbox']['last_nid'] = $nid;
    $context['sandbox']['done']++;

    $node = node_load($nid);
    $trakt_id = $node->field_trakt_id[LANGUAGE_NONE][0]['value'] ?? NULL;
    $show = $trakt_id ? trakt_tv_fetch_show_details($trakt_id, 'images') : [];

    // File fields compare against the unchanged entity to track file usage.
    $node->original = clone $node;
    if ($show && trakt_tv_set_node_poster($node, $show)) {
      // Save only the fields, so the show's "updated" date and revisions are
      // left alone.
      field_attach_presave('node', $node);
      field_attach_update('node', $node);
      entity_get_controller('node')->resetCache([$nid]);
      $context['results']['added']++;
    }
    else {
      $context['results']['missing'][] = $node->title;
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
 * Batch finished callback for the poster backfill.
 */
function trakt_tv_poster_backfill_finished($success, $results, $operations) {
  if (!$success) {
    backdrop_set_message(t('Fetching posters stopped with an error. Run it again to continue where it left off.'), 'error');
    return;
  }
  backdrop_set_message(format_plural($results['added'] ?? 0, 'Added 1 poster.', 'Added @count posters.'));
  if (!empty($results['missing'])) {
    backdrop_set_message(t('No poster available for: @titles', [
      '@titles' => implode(', ', $results['missing']),
    ]), 'warning');
  }
  cache_clear_all();
}
