# Albert plugin: GitHub

## Features

- Provided query handlers:
  - GitHub [user search](https://docs.github.com/search-github/searching-on-github/searching-users) (users and organizations)
  - GitHub [repository search](https://docs.github.com/search-github/searching-on-github/searching-for-repositories)
  - GitHub [issue search](https://docs.github.com/search-github/searching-on-github/searching-issues-and-pull-requests) (issues and pull requests)
  - Saved searches
- Item actions
  - User / Organization
    - Show on GitHub.
  - Repository
    - Show on GitHub.
    - Show issues on GitHub.
    - Show pull requests on GitHub.
    - Show discussions on GitHub.
    - Show wiki on GitHub.
  - Issue / Pull request
    - Show on GitHub.
  - Saved search
    - Run.
    - Run on GitHub.
- Authentication allows for private access and higher rate limits.

## Note

GitHub has a complex [rate limiting system](https://docs.github.com/rest/using-the-rest-api/rate-limits-for-the-rest-api?apiVersion=2022-11-28). 
Authenticated access has higher rate limts.
For Albert this means you get faster average response times.

## Authenticated access

1. Create an OAuth app on GitHub. ([GitHub docs](https://docs.github.com/en/apps/oauth-apps/building-oauth-apps/creating-an-oauth-app))
1. Give it a name.
1. Give it a random website.
1. Add Redirect URI `albert://github/`.
1. Click *Register application*.
1. Click *Generate a new client secret*.
1. Insert *Client ID* and *Client secret* in the plugin settings and click the authorize button.

### Why that complicated?

1. Shipping embedded credentials introduces an external dependency (that may break functionality).
1. Shipping embedded credentials in an open source project is difficult to distribute. 

## Technical notes

- Uses the [GitHub Web API](https://docs.github.com/en/rest) (API version: v2022-11-28).
- See the used endpoints and scopes in `github.h`.
