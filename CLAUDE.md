# CLAUDE.md — EOL project instructions for Claude Code
## Safety Rules
-  Never run destructive commands (rm, git clean, etc.) without explicitly asking permission first.

## Style
- Match the style of existing code. Do not reformat code you did not change.
- Do not add docstrings, comments, or type annotations to unchanged code.

## Build system
- Prefer SCons (via eol_scons) over CMake for C++ projects.
- Pin at c++ 17: this is another repo's dependancy, I've forgetten which one

## Testing
- Always run the test suite before considering a change complete.
- Do not commit code that does not pass existing tests.

## Access
- Do not access the EOL wiki, private repositories, issues, chat, or email.
- Do not store or transmit credentials or secrets.
- Do not commit without asking - verify branch first
- When searching the codebase, always confirm the correct path with the user before deep-diving. Exclude hidden directories (e.g., .git, .build) from find commands by default.

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
   the session future maintainers benefit from the trail.
3. If the deprecation is relevant to EOL software development broadly (not just
   this project), suggest a PR to github.com/NCAR/eol-se-guidelines. Skip if
   it's project-specific.

## Project Context 
- This project ecosystem primarily uses: Python, C++ 17 with SCons/CMake, Qt5, and runs on RHEL Linux. Default to these technologies unless told otherwise.

## Accessibility
- Do not rely on color alone to convey state. Always pair a color change with a
  second cue: border weight, icon, pattern, or text label.
- Prefer colorblind-safe palettes. Avoid red/green as the sole distinguishing
  pair (deuteranopia/protanopia is the most common form). Amber + bold border
  is the current convention for invalid field state in the Qt editor.
- Screen reader support for Qt desktop apps is an open problem on this project.
  Do not add features that make it harder (e.g. custom-painted widgets with no
  accessible text). Flag any new widget that has no accessible name or
  description so it can be addressed when a solution is identified.

### Structural accessibility work (deferred — needs real screen-reader testing)

The dependency tree (QTreeWidget) in vared_qt announces node text via the
platform bridge on macOS VoiceOver and Windows NVDA, but dynamic subtree
narration (reading the full recursive structure on selection) requires a custom
QAccessibleInterface subclass. Specifically:

- Subclass QAccessibleInterface for the tree widget.
- Override `text(QAccessible::Text)` to return a human-readable description of
  the full dependency path from the selected node to its leaves.
- Register the interface with `QAccessible::installFactory`.
- Reference: Qt docs "Implementing Accessibility" and QAccessibleInterface.

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
