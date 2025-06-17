# albert-plugin-github

## Features

- Provides query handlers for GitHub user, repository and issue search.
- Triggered queries perform a GitHub search.
- Global queries return customizable saved searches.
- The root trigger queryhandler returns the saved searches of the dedicated search handlers.
- Authentication allows for private access and higher rate limits.

## Note

GitHub has a complex [rate limiting system](https://docs.github.com/rest/using-the-rest-api/rate-limits-for-the-rest-api?apiVersion=2022-11-28). 
Limits of unauthenticated API queries are hit quickly. 
Use authentication to avoid getting limited (see the section below).

## Setup

1. Create an OAuth app on GitHub. ([GitHub docs](https://docs.github.com/en/apps/oauth-apps/building-oauth-apps/creating-an-oauth-app))
1. Give it a name.
1. Give it a random website.
1. Add Redirect URI `albert://github/`.
1. Click *Register application*.
1. Click *Generate a new client secret*.
1. Insert *Client ID* and *Client secret* in the plugin settings and click the authorize button.

## Technical notes

- Uses the [GitHub Web API](https://docs.github.com/en/rest) (API version: v2022-11-28).
- See the used endpoints and scopes in `github.h`.
