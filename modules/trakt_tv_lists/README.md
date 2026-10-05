# Trakt TV Lists

An optional submodule of Trakt TV. It marks the TV shows on your site that
appear on "best of" lists, such as the New York Times' 100 Best TV Shows of the
21st Century, IMDb's Top Rated TV Shows or the Emmy winners, and links visitors
to the original list.

Rankings are not copied. Each show simply says which lists it is on, and links
to the publication's own page for the full list.

Nothing is downloaded until you enable this module and add a list.

## How it works

Many publications' lists have been recreated as public lists on Trakt. This
module reads those Trakt lists through the Trakt API (Client ID only, no
sign-in needed) and matches them to your shows by Trakt ID.

- **TV Lists vocabulary:** each term is one list. It has:
  - **Trakt list:** the public Trakt list to sync from, e.g.
    `https://trakt.tv/users/USERNAME/lists/LIST-NAME`.
  - **Original list:** the publication's own page. This is what visitors see.
  - **Keep in sync:** re-sync daily on cron, for lists that change (IMDb).
- **On these lists** field on TV Show content: filled in automatically. Shows
  on the Trakt list are tagged; shows that drop off are untagged. Shows that
  are not on your site are ignored.
- **TV list links** formatter (the default for that field): each list name
  links to the list's page on your site, followed by a small "(original)" link
  to the publication. It can also link only to one or the other.
- **About this TV list** block: on a list's page, shows its description, a
  note on ordering, and a "See the original list" link. Place it at the top of
  the content region of the layout used for taxonomy term pages; it shows
  nothing on other pages. The note defaults to "These shows are on this list,
  but they are shown in this site's own order, not the list's ranking." Edit it
  in the block's settings (for example, "Sorted by my own score, not by the
  list's ranking."), or leave it empty to hide it.

## Adding a list

1. Find the list on Trakt (search for the publication's name) and check it is
   public and matches the original.
2. Go to Structure > Taxonomy > TV Lists > Add term. Enter a name, the Trakt
   list address, the original list's address and a short description crediting
   the source.
3. Save. The list syncs immediately and reports how many of your shows are on
   it. Use the **Sync with Trakt** tab on the list's page to sync again later.

Trakt lists are made by Trakt members, not the publications. Check a list
against the original before relying on it. Some lists are stored in reverse
order or have been updated since; that does not matter here, since only
membership is used.

## Requirements

- Trakt TV, with a Client ID configured.
- Core Taxonomy, Link and List modules.
