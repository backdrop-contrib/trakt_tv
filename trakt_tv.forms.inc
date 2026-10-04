<?php 

function trakt_tv_auth_code_exchange_form($form, &$form_state) {
  // Token status
  $tokens = trakt_tv_token_store_get();

  if ($tokens && isset($tokens['access_token'])) {
    $created = format_date($tokens['created_at'], 'short');
    $expires_at = $tokens['created_at'] + $tokens['expires_in'];
    $expires = format_date($expires_at, 'short');
    $remaining = $expires_at - time();

    $form['token_status'] = [
      '#type' => 'markup',
      '#markup' => '<div class="messages status"><strong>Access Token Info:</strong><br>' .
        'Created: ' . check_plain($created) . '<br>' .
        'Expires: ' . check_plain($expires) . '<br>' .
        'Time remaining: ' . ($remaining > 0 ? format_interval($remaining) : 'Expired') .
        '</div>',
    ];
  }
  else {
    $form['token_status'] = [
      '#type' => 'markup',
      '#markup' => '<div class="messages warning">No access token stored yet.</div>',
    ];
  }

  $form['authorize'] = [
    '#type' => 'markup',
    '#markup' => '<p>' . t('Step 1: <a href="@url">Authorize this site with Trakt</a>. After you approve, Trakt sends you to your redirect URI with a <code>code</code> in the address bar.', [
      '@url' => url('admin/config/media/trakt_tv/authorize'),
    ]) . '</p>',
  ];

  // Auth code input
  $form['auth_code'] = [
    '#type' => 'textfield',
    '#title' => t('Step 2: Authorization code'),
    '#description' => t('Paste the code, or the whole address you were sent to. Codes expire after a few minutes.'),
    '#required' => TRUE,
    '#size' => 80,
    '#maxlength' => 1024,
  ];

  // Prefill the code when Trakt (or trakt_tv_init()) sends the person here.
  if (!empty($_GET['code']) && empty($form_state['input'])) {
    $form['auth_code']['#default_value'] = $_GET['code'];
    backdrop_set_message(t('Trakt sent back an authorization code. Click "Exchange Code for Token" to finish.'), 'status', FALSE);
  }

  $form['submit'] = [
    '#type' => 'submit',
    '#value' => t('Exchange Code for Token'),
  ];

  return $form;
}

/**
 * Validation handler: accepts a bare code or the full redirect URL.
 */
function trakt_tv_auth_code_exchange_form_validate($form, &$form_state) {
  $code = trim($form_state['values']['auth_code']);

  if (strpos($code, 'code=') !== FALSE) {
    $query = [];
    parse_str((string) parse_url($code, PHP_URL_QUERY), $query);
    if (empty($query['code'])) {
      // Pasted only part of the URL, e.g. "code=abc&iss=...".
      parse_str($code, $query);
    }
    $code = isset($query['code']) ? $query['code'] : '';
  }

  if (!preg_match('/^[A-Za-z0-9._~-]+$/', $code)) {
    form_set_error('auth_code', t('That does not look like a Trakt authorization code.'));
    return;
  }

  form_set_value($form['auth_code'], $code, $form_state);
}

/**
 * Submit handler: exchanges the authorization code for tokens.
 */
function trakt_tv_auth_code_exchange_form_submit($form, &$form_state) {
  $code = $form_state['values']['auth_code'];
  $success = trakt_tv_exchange_auth_code($code);

  if ($success) {
    backdrop_set_message(t('Access token successfully retrieved and saved.'));
  }
  else {
    backdrop_set_message(t('Failed to retrieve access token. Check the logs for details.'), 'error');
  }

  // Rebuild the form so updated token info is shown
  $form_state['rebuild'] = TRUE;
}


/**
 * Page callback: starts a PKCE authorization and redirects to Trakt.
 *
 * A new code verifier is stored each time, so only the most recently opened
 * authorize link can be exchanged for tokens.
 */
function trakt_tv_authorize_redirect() {
  $creds = trakt_tv_get_credentials();

  if (empty($creds['client_id']) || empty($creds['redirect_uri'])) {
    backdrop_set_message(t('Enter your Trakt Client ID and Redirect URI before authorizing.'), 'error');
    backdrop_goto('admin/config/media/trakt_tv');
  }

  $pkce = trakt_tv_pkce_generate();
  state_set('trakt_tv_pkce_verifier', $pkce['verifier']);

  backdrop_goto('https://trakt.tv/oauth/authorize', [
    'query' => [
      'response_type' => 'code',
      'client_id' => $creds['client_id'],
      'redirect_uri' => $creds['redirect_uri'],
      'code_challenge' => $pkce['challenge'],
      'code_challenge_method' => 'S256',
    ],
    'external' => TRUE,
  ]);
}
