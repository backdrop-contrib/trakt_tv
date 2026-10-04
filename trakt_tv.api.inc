<?php

/**
 * @file
 * Contains API-related functions for the Trakt TV module.
 */

function trakt_tv_token_store_get() {
  $file = 'private://trakt_tv_tokens.json';

  if (!file_exists($file)) {
    watchdog('trakt_tv', 'Token file not found at @path.', ['@path' => $file], WATCHDOG_WARNING);
    return NULL;
  }

  $json = file_get_contents($file);
  $data = json_decode($json, true);

  if (json_last_error() !== JSON_ERROR_NONE) {
    watchdog('trakt_tv', 'Invalid token JSON: @error', ['@error' => json_last_error_msg()], WATCHDOG_ERROR);
    return NULL;
  }

  return $data;
}

function trakt_tv_token_store_save(array $tokens) {
  $path = 'private://trakt_tv_tokens.json';
  $json = json_encode($tokens, JSON_PRETTY_PRINT);

  if (file_unmanaged_save_data($json, $path, FILE_EXISTS_REPLACE)) {
    watchdog('trakt_tv', 'Access token and refresh token saved to file.', [], WATCHDOG_INFO);
    return TRUE;
  }
  else {
    watchdog('trakt_tv', 'Failed to write token data to @path.', ['@path' => $path], WATCHDOG_ERROR);
    return FALSE;
  }
}

/**
 * Fetch trending TV shows from Trakt API.
 */
function trakt_tv_fetch_trending() {
  $access_token = trakt_tv_get_valid_token();
  $creds = trakt_tv_get_credentials();

  if (!$creds) {
    backdrop_set_message(t('You have not entered API credentials for Trakt TV yet. Please <a href="@url">go to the settings page</a> to configure them.', [
      '@url' => url('admin/config/media/trakt_tv'),
    ]), 'info');
  }

  if (!$access_token) {
    watchdog('trakt_tv', '002 - Missing Trakt access token.', [], WATCHDOG_ERROR);
    return [];
  }

  $ch = curl_init('https://api.trakt.tv/shows/trending?limit=20');
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
  curl_setopt($ch, CURLOPT_HTTPHEADER, [
    "Accept: application/json",
    "Content-Type: application/json",
    "trakt-api-version: 2",
    "trakt-api-key: " . $creds['client_id'],
    "Authorization: Bearer $access_token",
  ]);

  // Set browser-like User-Agent to try bypassing Cloudflare bot detection
  curl_setopt($ch, CURLOPT_USERAGENT, 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36');

  $response = curl_exec($ch);
  $error = curl_error($ch);
  $status = curl_getinfo($ch, CURLINFO_HTTP_CODE);
  curl_close($ch);

  if ($response === false) {
    watchdog('trakt_tv', 'cURL failed: @error', ['@error' => $error], WATCHDOG_ERROR);
    return [];
  }

  if ($status !== 200) {
    watchdog('trakt_tv', 'Trakt API returned status: @status - @body', [
      '@status' => $status,
      '@body' => $response,
    ], WATCHDOG_ERROR);
    return [];
  }

  $data = json_decode($response, TRUE);
  if (json_last_error() !== JSON_ERROR_NONE) {
    watchdog('trakt_tv', 'JSON decode error: @error', [
      '@error' => json_last_error_msg(),
    ], WATCHDOG_ERROR);
    return [];
  }

  return $data;
}

/**
 * Fetch recent show-watching history from Trakt for the authenticated user.
 *
 * @param int $limit
 *   Maximum number of history items to return (1-100).
 *
 * @return array
 *   Array of history items (episode scrobbles), or empty array on failure.
 */
function trakt_tv_fetch_history($limit = 10) {
  $access_token = trakt_tv_get_valid_token();
  $creds = trakt_tv_get_credentials();

  if (!$access_token) {
    watchdog('trakt_tv', 'Missing Trakt access token for history fetch.', [], WATCHDOG_ERROR);
    return [];
  }

  $limit = max(1, min(100, (int) $limit));
  $url = 'https://api.trakt.tv/sync/history/shows?limit=' . $limit;

  $ch = curl_init($url);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
  curl_setopt($ch, CURLOPT_HTTPHEADER, [
    'Accept: application/json',
    'Content-Type: application/json',
    'trakt-api-version: 2',
    'trakt-api-key: ' . $creds['client_id'],
    'Authorization: Bearer ' . $access_token,
  ]);
  curl_setopt($ch, CURLOPT_USERAGENT, 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36');

  $response = curl_exec($ch);
  $error = curl_error($ch);
  $status = curl_getinfo($ch, CURLINFO_HTTP_CODE);
  curl_close($ch);

  if ($response === FALSE) {
    watchdog('trakt_tv', 'History fetch cURL failed: @error', ['@error' => $error], WATCHDOG_ERROR);
    return [];
  }

  if ($status !== 200) {
    watchdog('trakt_tv', 'Trakt history API returned status: @status - @body', [
      '@status' => $status,
      '@body' => $response,
    ], WATCHDOG_ERROR);
    return [];
  }

  $data = json_decode($response, TRUE);
  if (json_last_error() !== JSON_ERROR_NONE) {
    watchdog('trakt_tv', 'History JSON decode error: @error', ['@error' => json_last_error_msg()], WATCHDOG_ERROR);
    return [];
  }

  return is_array($data) ? $data : [];
}

/**
 * Fetch upcoming episodes for shows on the authenticated user's watchlist.
 *
 * @param int $days
 *   Number of days ahead to fetch, starting today (1-60).
 *
 * @return array
 *   Array of calendar items, or empty array on failure.
 */
function trakt_tv_fetch_upcoming($days = 7) {
  $access_token = trakt_tv_get_valid_token();
  $creds = trakt_tv_get_credentials();

  if (!$access_token) {
    watchdog('trakt_tv', 'Missing Trakt access token for upcoming fetch.', [], WATCHDOG_ERROR);
    return [];
  }

  $days = max(1, min(60, (int) $days));
  $start = gmdate('Y-m-d');
  $url = 'https://api.trakt.tv/calendars/my/shows/' . $start . '/' . $days;

  $ch = curl_init($url);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
  curl_setopt($ch, CURLOPT_HTTPHEADER, [
    'Accept: application/json',
    'Content-Type: application/json',
    'trakt-api-version: 2',
    'trakt-api-key: ' . $creds['client_id'],
    'Authorization: Bearer ' . $access_token,
  ]);
  curl_setopt($ch, CURLOPT_USERAGENT, 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36');

  $response = curl_exec($ch);
  $error = curl_error($ch);
  $status = curl_getinfo($ch, CURLINFO_HTTP_CODE);
  curl_close($ch);

  if ($response === FALSE) {
    watchdog('trakt_tv', 'Upcoming fetch cURL failed: @error', ['@error' => $error], WATCHDOG_ERROR);
    return [];
  }

  if ($status !== 200) {
    watchdog('trakt_tv', 'Trakt upcoming API returned status: @status - @body', [
      '@status' => $status,
      '@body' => $response,
    ], WATCHDOG_ERROR);
    return [];
  }

  $data = json_decode($response, TRUE);
  if (json_last_error() !== JSON_ERROR_NONE) {
    watchdog('trakt_tv', 'Upcoming JSON decode error: @error', ['@error' => json_last_error_msg()], WATCHDOG_ERROR);
    return [];
  }

  return is_array($data) ? $data : [];
}


/**
 * Returns a valid access token, refreshing it first if it is about to expire.
 *
 * @param int $refresh_margin
 *   Refresh the token if it expires within this many seconds. Cron passes a
 *   larger margin so tokens are renewed before anyone needs them.
 *
 * @return string|null
 *   The access token, or NULL if none is available.
 */
function trakt_tv_get_valid_token($refresh_margin = 60) {
  $tokens = trakt_tv_token_store_get();
  $creds = trakt_tv_get_credentials();

  if (!$tokens || !isset($tokens['access_token'])) {
    watchdog('trakt_tv', '001 - Missing Trakt token data.', [], WATCHDOG_ERROR);
    return NULL;
  }

  $expires_at = $tokens['created_at'] + $tokens['expires_in'];
  if (time() < $expires_at - $refresh_margin) {
    return $tokens['access_token']; // still valid
  }

  // Trakt issues a new refresh token on every refresh and invalidates the old
  // one, so only one request may refresh at a time.
  if (!lock_acquire('trakt_tv_token_refresh', 30)) {
    lock_wait('trakt_tv_token_refresh');
    $tokens = trakt_tv_token_store_get();
    return isset($tokens['access_token']) ? $tokens['access_token'] : NULL;
  }

  // Another request may have refreshed while we waited for the lock.
  $tokens = trakt_tv_token_store_get();
  if (time() < $tokens['created_at'] + $tokens['expires_in'] - $refresh_margin) {
    lock_release('trakt_tv_token_refresh');
    return $tokens['access_token'];
  }

  // Refresh the token. PKCE apps have no client secret, so only send one
  // when it has been configured (legacy Trakt apps).
  $payload = [
    'refresh_token' => $tokens['refresh_token'],
    'client_id' => $creds['client_id'],
    'redirect_uri' => $creds['redirect_uri'],
    'grant_type' => 'refresh_token',
  ];
  if (!empty($creds['client_secret'])) {
    $payload['client_secret'] = $creds['client_secret'];
  }

  $data = trakt_tv_request_token($payload);
  lock_release('trakt_tv_token_refresh');
  if (!$data) {
    watchdog('trakt_tv', 'Failed to refresh Trakt token. Re-authorize at <a href="@url">Trakt Auth Code Exchange</a>.', [
      '@url' => url('admin/config/media/trakt_tv/auth-exchange'),
    ], WATCHDOG_ERROR);
    // An early (cron) refresh can fail while the current token still works.
    return time() < $expires_at ? $tokens['access_token'] : NULL;
  }

  return $data['access_token'];
}

/**
 * Requests tokens from the Trakt OAuth endpoint and saves them.
 *
 * Existing tokens are only replaced when Trakt returns a complete token set,
 * so a failed request never wipes out a working refresh token.
 *
 * @param array $payload
 *   The request body for https://api.trakt.tv/oauth/token.
 *
 * @return array|false
 *   The decoded token response, or FALSE on failure.
 */
function trakt_tv_request_token(array $payload) {
  $response = backdrop_http_request('https://api.trakt.tv/oauth/token', [
    'method' => 'POST',
    'data' => json_encode($payload),
    'headers' => [
      'Content-Type' => 'application/json',
    ],
  ]);

  $data = isset($response->data) ? json_decode($response->data, TRUE) : NULL;
  if ($response->code != 200 || empty($data['access_token']) || empty($data['refresh_token'])) {
    watchdog('trakt_tv', 'Trakt token request (@grant) failed with status @code: @data', [
      '@grant' => $payload['grant_type'],
      '@code' => $response->code,
      '@data' => isset($response->data) ? $response->data : '',
    ], WATCHDOG_ERROR);
    return FALSE;
  }

  trakt_tv_token_store_save([
    'access_token' => $data['access_token'],
    'refresh_token' => $data['refresh_token'],
    'expires_in' => $data['expires_in'],
    'created_at' => time(),
  ]);

  return $data;
}

/**
 * Generates a PKCE code verifier and its S256 code challenge.
 *
 * @return array
 *   An array with 'verifier' and 'challenge' keys.
 */
function trakt_tv_pkce_generate() {
  $verifier = rtrim(strtr(base64_encode(random_bytes(48)), '+/', '-_'), '=');
  $challenge = rtrim(strtr(base64_encode(hash('sha256', $verifier, TRUE)), '+/', '-_'), '=');

  return [
    'verifier' => $verifier,
    'challenge' => $challenge,
  ];
}



/**
 * Fetch TV show data based on search query.
 */
function trakt_tv_search_api($query) {
  $access_token = trakt_tv_get_valid_token();
  $creds = trakt_tv_get_credentials();

  if (!$access_token) {
    watchdog('trakt_tv', 'Unable to fetch valid Trakt access token.', [], WATCHDOG_ERROR);
    backdrop_set_message(t('Unable to authenticate with Trakt. Please check token settings.'), 'error');
    return [];
  }

  $query = trim(check_plain($query));
  $api_url = 'https://api.trakt.tv/search/show?query=' . urlencode($query);

  $ch = curl_init($api_url);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
  curl_setopt($ch, CURLOPT_HTTPHEADER, [
    "Accept: application/json",
    "Content-Type: application/json",
    "trakt-api-version: 2",
    "trakt-api-key: " . $creds['client_id'],
    "Authorization: Bearer $access_token",
  ]);
  curl_setopt($ch, CURLOPT_USERAGENT, 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)');

  $response = curl_exec($ch);
  $error = curl_error($ch);
  $status = curl_getinfo($ch, CURLINFO_HTTP_CODE);
  curl_close($ch);

  if ($response === false) {
    watchdog('trakt_tv', 'Search API cURL failed: @error', ['@error' => $error], WATCHDOG_ERROR);
    return [];
  }

  if ($status !== 200) {
    watchdog('trakt_tv', 'Search API returned status: @status - @body', [
      '@status' => $status,
      '@body' => $response,
    ], WATCHDOG_ERROR);
    return [];
  }

  $data = json_decode($response, TRUE);
  if (json_last_error() !== JSON_ERROR_NONE) {
    watchdog('trakt_tv', 'Search JSON decode error: @error', [
      '@error' => json_last_error_msg(),
    ], WATCHDOG_ERROR);
    return [];
  }

  return $data;
}



/**
 * Fetch full TV show details by Trakt ID.
 */
function trakt_tv_fetch_show_details($trakt_id) {
  $access_token = trakt_tv_get_valid_token();
  $creds = trakt_tv_get_credentials();

  if (!$access_token) {
    watchdog('trakt_tv', 'Unable to fetch valid Trakt access token.', [], WATCHDOG_ERROR);
    return [];
  }

  $api_url = 'https://api.trakt.tv/shows/' . intval($trakt_id) . '?extended=full';

  $ch = curl_init($api_url);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
  curl_setopt($ch, CURLOPT_HTTPHEADER, [
    "Accept: application/json",
    "Content-Type: application/json",
    "trakt-api-version: 2",
    "trakt-api-key: " . $creds['client_id'],
    "Authorization: Bearer $access_token",
  ]);
  curl_setopt($ch, CURLOPT_USERAGENT, 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)');

  $response = curl_exec($ch);
  $error = curl_error($ch);
  $status = curl_getinfo($ch, CURLINFO_HTTP_CODE);
  curl_close($ch);

  if ($response === false) {
    watchdog('trakt_tv', 'Show details cURL failed: @error', ['@error' => $error], WATCHDOG_ERROR);
    return [];
  }

  if ($status !== 200) {
    watchdog('trakt_tv', 'Show details API returned status: @status - @body', [
      '@status' => $status,
      '@body' => $response,
    ], WATCHDOG_ERROR);
    return [];
  }

  $data = json_decode($response, TRUE);
  if (json_last_error() !== JSON_ERROR_NONE) {
    watchdog('trakt_tv', 'Show details JSON decode error: @error', [
      '@error' => json_last_error_msg(),
    ], WATCHDOG_ERROR);
    return [];
  }

  return $data;
}

function trakt_tv_exchange_auth_code($code) {
  $creds = trakt_tv_get_credentials(); // Load from your private:// JSON

  if (!$creds) {
    watchdog('trakt_tv', 'Unable to access credentials.', [], WATCHDOG_ERROR);
    return [];
  }

  $data = [
    'code' => $code,
    'client_id' => $creds['client_id'],
    'redirect_uri' => $creds['redirect_uri'],
    'grant_type' => 'authorization_code',
  ];
  // Legacy Trakt apps use a client secret; PKCE apps send the code verifier
  // created when the authorize link was followed.
  if (!empty($creds['client_secret'])) {
    $data['client_secret'] = $creds['client_secret'];
  }
  $verifier = state_get('trakt_tv_pkce_verifier');
  if ($verifier) {
    $data['code_verifier'] = $verifier;
  }

  if (!trakt_tv_request_token($data)) {
    return FALSE;
  }

  // Each verifier is only valid for a single authorization.
  state_del('trakt_tv_pkce_verifier');
  return TRUE;
}

function trakt_tv_get_credentials() {
  $path = 'private://credentials.json';
  
  if (file_exists($path)) {
    $json = file_get_contents($path);
    $data = json_decode($json, TRUE);
    
    if (json_last_error() === JSON_ERROR_NONE) {
      return $data;
    }
    else {
      watchdog('trakt_tv', 'Invalid JSON in credentials file: @error', [
        '@error' => json_last_error_msg(),
      ], WATCHDOG_ERROR);
    }
  }
  else {
    watchdog('trakt_tv', 'Credentials file not found at @path.', ['@path' => $path], WATCHDOG_ERROR);
  }

  return [];
}




