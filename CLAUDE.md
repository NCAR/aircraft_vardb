# CLAUDE.md — EOL project instructions for Claude Code

## Style
- No emojis in code, comments, or documentation.
- Match the style of existing code. Do not reformat code you did not change.
- Do not add docstrings, comments, or type annotations to unchanged code.

## Build system
- Prefer SCons (via eol_scons) over CMake for C++ projects.

## Testing
- Always run the test suite before considering a change complete.
- Do not commit code that does not pass existing tests.

## Access
- Do not access the EOL wiki, private repositories, issues, chat, or email.
- Do not store or transmit credentials or secrets.

## Code quality
- Favor simple, readable solutions over clever ones.
- Do not add error handling for scenarios that cannot happen.
- Do not introduce abstractions for one-time use.

## Handling bad code, dark patterns, and security issues
- Do not fix bad code unilaterally. Propose a plan first.
- Check whether existing tests verify the current behavior before proposing a change.
- Present findings with: affected files (all if <5, five representative + count
  if more), scope (trivial/moderate/structural), severity, and any existing
  explanation in the code.
- If there is no documented reason for a code oddity, ask the team for history,
  then document what you learn in a comment. Do not silently normalize it.
- Do not delete code just because it looks odd or rarely runs. Only remove code
  confirmed with the team to be unreachable or superseded.

## Deprecated practices
When you find a deprecated practice:
1. Propose a fix in the current repo (follow bad-code handling rule).
2. Document the deprecation and reason in the README or relevant project docs.
   Include references to external docs, specs, or resources consulted during
   the session — future maintainers benefit from the trail.
3. If the deprecation is relevant to EOL software development broadly (not just
   this project), suggest a PR to github.com/NCAR/eol-se-guidelines. Skip if
   it's project-specific.

## Licensing and cost
- Never introduce a dependency whose license is incompatible with the project,
  or change the project license, without a new release and team sign-off.
- When suggesting or auditing libraries, check and state the license. Flag
  anything incompatible, unexpectedly restrictive, or ambiguous.
- Prefer stable, well-established libraries over the newest or most fashionable
  option. EOL software runs in field deployments where unexpected breakage has
  monetary or novel data loss consequences.
- When suggesting any tool, state whether it is free/open source, has a
  free tier, requires a subscription, or has hidden ongoing costs (API
  pricing, seat licenses, data egress, etc.).
