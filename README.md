# Trakt TV API Module

This module has been created to integrate with the [Trakt TV](https://trakt.tv/) API in a limited
manner. This is a new module, custom made for Backdrop CMS.

This module should be fully functional, but with very limted features. I will release it once I 
know that one or two other people have tested it out. It's a very niche module, but may be useful
to some folks interested in learning how to access API's. 

It currently, does the following. 

1) Allows a site vistor to view a list of the top 20 trending shows on Trakt TV.
2) Allows a site vistor to search for a specific TV show in the Trakt TV database.
3) Allows a site vistor to create a node with the following data for any TV show.
   - Title
   - Trakt ID
   - Desciption / Overview
   - Rating
   - Tagline
   - Year
   - Status
   - Website
   - Genres
   - Network
   - Poster (downloaded and stored on your site; see "Posters" below)
4) Provides a "Recently watched on Trakt" block showing the site owner's most
   recently scrobbled episodes, for placement in any layout region. The number
   of episodes shown is configurable, and results are cached for 15 minutes to
   avoid hammering the Trakt API.
5) Provides an "Upcoming on Trakt" block showing episodes airing soon for shows
   on the site owner's Trakt watchlist. The time window (3 days to 1 month) is
   configurable, and results are cached for 15 minutes.

This module creates a content type called TV Show with all of the fields required by
this module. 

![image](https://github.com/user-attachments/assets/655b077f-beb0-407e-bae4-51668a62e908)

We are open to idea for how to expand this module.


## Requirements

- This module requires a Trakt TV API Key. You will need to create an account on
  Trakt TV and create an application for an APP API key.
  https://trakt.tv/oauth/applications/new

## Installation

- Install this module using the official [Backdrop CMS instructions](https://backdropcms.org/user-guide/modules).

- You will need to create an account with Trakt TV and create an app here to the necessary credentials for this module. https://trakt.tv/oauth/applications

These fields are required.
![image](https://github.com/user-attachments/assets/5a64fcef-0a63-498b-bf14-709f2b676fdf)

Copy the Client ID and Redirect URI into `admin/config/media/trakt_tv`. New Trakt
apps sign in with PKCE and do not issue a Client Secret, so leave the
"Legacy Trakt apps" section empty unless your app was created before Trakt
switched to PKCE.

Tip: use your site's exchange page as the Redirect URI, e.g.
`https://example.com/admin/config/media/trakt_tv/auth-exchange`. Trakt will then
send you straight back to the form with the code filled in. (If you use another
address, such as your home page, administrators are redirected to the form
automatically.)

To finish, go to `admin/config/media/trakt_tv/auth-exchange`:

1. Click **Authorize this site with Trakt** and approve access.
2. Trakt sends you to your Redirect URI with a `code` in the address bar. Paste
   the code (or the whole address) into the form and submit. Codes expire after
   a few minutes.

![image](https://github.com/user-attachments/assets/5f2cd152-3065-49b9-bd1e-4ebab55ef7f2)

Your code will be here after you authorize your site:
![image](https://github.com/user-attachments/assets/b9c6308f-669e-4d14-a4d2-0cf5beaf2c8d)

The access token is refreshed automatically when it expires. If refreshing
fails (for example, the Trakt app was deleted), repeat the authorization steps.

## Best-of lists (optional)

Enable the included **Trakt TV Lists** submodule to mark shows that appear on
"best of" lists (NYT, IMDb, Emmys and more) and link to the original lists.
See `modules/trakt_tv_lists/README.md`.

## Posters

TV Show content has a **Poster** image field. When a show is added from Trakt,
its poster (600x900, WebP) is downloaded to `files/trakt_tv/posters/` and
attached. Trakt requires images to be stored on your site: linking directly to
`media.trakt.tv` is not allowed and is blocked.

For shows added before this feature, go to `admin/config/media/trakt_tv` and
click **Fetch missing posters**. It runs as a batch and skips shows that
already have a poster, so it is safe to run again. A few shows may have no
poster on Trakt; they are listed when the batch finishes.

The poster uses core image styles ("large" on the show page, "medium" in
teasers). Change this under Manage Display for the TV Show content type, or
add the field to your Views.


## Issues

Bugs and feature requests should be reported in the [Issue Queue](https://github.com/backdrop-contrib/trakt_tv/issues).

## Current Maintainer

- [Tim Erickson](https://github.com/stpaultim)

## Credits

- Sponsored by Simplo

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.
