# albert-plugin-github

## Features

- The empty query shows notifications.
- Prefix a query with `i` to search issues and pull requests.
- Prefix a query with `u` to search users and organizations.
- Prefix a query with `r` to search repositories.

## Setup

1. Create an OAuth app on GitHub. ([GitHub docs](https://docs.github.com/en/apps/oauth-apps/building-oauth-apps/creating-an-oauth-app))
1. Give it a name.
1. Give it a random website.
1. Add Redirect URI `albert://github/`.
1. Click *Register application*.
1. Click *Generate a new client secret*.
1. Insert *Client ID* and *Client secret* in the plugin settings and click the authorize button.

## Technical notes

- Uses the [GitHub Web API](https://docs.github.com/en/rest).
- See the used endpoints and scopes in `github.h`.
