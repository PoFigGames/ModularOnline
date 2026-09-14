# Contributing

Thank you for looking. Issues and pull requests are both welcome; this file says what makes them easy to
act on and how the code is written.

## Reporting a problem

Online problems are hard to reproduce from a description alone, so please include:

- Engine version and plugin version or commit.
- Which provider, and whether you were in the editor, in PIE, or in a packaged build.
- Platform, and whether more than one machine was involved.
- The relevant `[ModularOnline.*]` config, with any identifiers redacted if you prefer.
- `LogModularOnline` output around the failure, at `Verbose` if you can:

  ```ini
  [Core.Log]
  LogModularOnline=Verbose
  ```

- The output of `ModularOnline.Status`, which prints each role, its provider and the components it has.
  Most "this does nothing" reports turn out to be a component the provider does not implement, and that
  command says so in one line.

Please do not paste an auth ticket, an account id you would rather keep private, or a full session log
without reading it first.

## Pull requests

- One change per pull request, with a description saying what the old behaviour was and what the new one
  is. Say how you tested it, and against which provider — code written against one provider's behaviour
  and never run against another usually surprises somebody.
- **No dependency on any particular game or project.** The plugin builds inside whatever project takes
  it, so nothing in `Source/` may reference a host project's modules, target rules, gameplay tags or
  configuration. Anything platform specific is expressed as a trait tag the game passes in.
- **Name no provider in code.** Which provider answers which question is configuration. A branch on
  "if this is Steam" belongs in an ini file, not in a `.cpp`.
- Do not commit generated binaries or intermediates; `.gitignore` already excludes them.
- Say whether the change alters what is published as a lobby or session attribute, because that breaks
  compatibility with builds already in players' hands.
- An operation that a provider cannot perform answers `NotSupported` and names the missing feature. It
  does not assert, and it does not quietly succeed.

## Code style

The style is Unreal's, with a few settled choices. `.clang-format` and `.editorconfig` in the repository
root carry the mechanical part; the rest:

- Tabs, width 4. Allman braces. Line limit 150.
- Locals are declared with `auto`, `const auto`, `auto&` or `const auto&`. No `auto*` — a pointer is
  deduced by plain `auto`. Write an explicit type only where the initialiser does not name it.
- Members use brace initialisation with spaces: `bool bFlag { false };`, `FString Name { };`. An empty
  value is `FString { }`, never `FString()`.
- Every `class`, `struct`, `concept` and `enum class` carries a Doxygen block:

  ```cpp
  /**
   * @class UModularMatchSubsystem
   *
   * @brief What a game asks to host, find and join a match.
   *
   * A longer description where one is warranted.
   */
  ```

  `@class` for classes, `@struct` for structs and concepts, `@enum` for enums. Single-line comments on
  members need no tags.
- Includes are sorted alphabetically; the `.generated.h` include stays last.
- Overrides are grouped in `#pragma region <BaseClass>` / `#pragma endregion <BaseClass>`.
- Every file starts with `// Copyright PoFig Games Studio. All Rights Reserved.`
- Identifiers, comments and log messages are in English.
- A comment that states something about the world outside this repository — that an Engine class behaves
  a certain way, that an upstream branch lacks a fix — carries the date it was checked. A reader a year
  from now needs to know it is a snapshot.

Two diagnostics bite on clang before they bite on MSVC, and a Windows-only build will not catch either:
a parameter or local hiding a member of a base class, and a switch over an enum with no default label
that misses an enumerator. Write for the stricter compiler.

## Blueprint surface

Blueprint support lives in the runtime module, not in one of its own. A call is a `K2_` wrapper over the
C++ method, and an event has a dynamic twin broadcast beside the native one — **both, always**: a
listener that sees a sign in and never the sign out is worse than one that sees neither.

Asynchronous nodes exist only for what a menu genuinely waits on. Everything else is a call plus an
event, which is cheaper to write and easier to read in a graph.

## Tests

Automation tests live in `Source/ModularOnlineDeveloper/Private/Tests/` and cover the parts that are
pure rules — error mapping, role resolution, cross play policy, presence states. They run without a
provider and without a network, and a pull request that changes one of those rules is expected to change
or add a test.

Run them with:

```
UnrealEditor YourProject.uproject -unattended -nopause -nosplash -nullrhi -ExecCmds="Automation RunTests ModularOnline; Quit"
```
