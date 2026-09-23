# Modular Online

A game facing layer over Unreal Engine's **Online Services (OSSv2)**: signing in, hosting and finding
matches, and the rest of the online features — assembled from whatever the provider on the current
platform actually implements, and asking the game to know nothing about which provider that is.

The engine gives you fifteen interfaces and a provider that may or may not implement each of them.
What it does not give you is the layer above: who is signed in, which of two providers answers a
question, what happens when a component is simply absent, and how any of that reaches Blueprint. That
layer is usually written once per project, against one platform, and rewritten for the next one. This
plugin is that layer, written once against the interfaces rather than against a platform.

> **Status: 0.1, early.** Built daily against a game in development. The API is not stable below 1.0 and
> nothing is deprecated before it changes. [CHANGELOG.md](CHANGELOG.md) lists what has and has not been
> verified.

---

## Contents

- [Requirements](#requirements)
- [What it does](#what-it-does)
- [Roles](#roles)
- [Capabilities](#capabilities)
- [Modules](#modules)
- [Installation](#installation)
- [Configuration](#configuration)
- [Using it from C++](#using-it-from-c)
- [Using it from Blueprint](#using-it-from-blueprint)
- [Localisation](#localisation)
- [What it does not do](#what-it-does-not-do)
- [Licence](#licence)

---

## Requirements

- Unreal Engine 5.8. That is the version it is built and tested against; no older one has been tried.
- The engine's `OnlineServices` and `OnlineSubsystemUtils` plugins, which this one enables for you.
- At least one Online Services provider. The engine ships `OnlineServicesNull` and `OnlineServicesEOS`;
  Steam is available through a separate plugin.

The plugin never names a provider in code. Which provider answers which question is configuration.

---

## What it does

**Identity.** A login is a state machine of steps — platform sign in, transferring that authentication
to a backend, signing in there, checking privileges — where each step is required, optional or skipped
according to what the platform says about itself. A second player on the same machine can be a guest
where the platform allows one, and only where the platform said "this player has no account" rather
than "this account may not play".

**Matches.** One interface with two implementations behind it: a match lives in a lobby or in a
registered session, and the game asks the same way either way. Which one carries it is configuration,
not guesswork. A project that publishes on two providers at once — a console lobby beside a
cross-platform one — gets the second publication, the rollback when it fails, and a search that merges
both without showing the same match twice.

A match publishes its name and its map, and beyond that only what the game names: the plugin invents no
attributes of its own, so nothing appears in your schema that you did not put there. Hosting and
travelling to a match are one call, so the address a match is reached by is built in one place.

**Everything else.** Presence, friends, profiles, achievements, statistics, leaderboards, cloud saves,
title files and the store each have a facade with a cache, events and Blueprint access. A provider that
does not implement one answers `NotSupported` — not a crash, not silence.

**Dedicated servers.** A machine with no players signs in as itself and hosts with the account the
services give it. Proving that a joining player is the account they claim is not this layer's job: it
belongs to the connection, before there is a login to refuse, and so to whatever carries that connection
— a packet handler or the provider's own token exchange, depending on the platform.

---

## Roles

Every question the plugin asks goes to a *role*, and roles are where two providers stop being a special
case:

| Role | What it means |
| --- | --- |
| `Platform` | The machine the game is running on — the console, or the store the player launched from. |
| `Service` | The project's own backend, when it has one. |
| `Default` | Whatever the engine resolves as the default provider. |

A project with one provider resolves every role to it and never thinks about this again. A project with
two says which role answers what:

| Setting | Default | Answers |
| --- | --- | --- |
| `MatchRole` | `Service` | where matches are published |
| `CompanionMatchRole` | `Platform` | the second publication, when cross play asks for one |
| `FeatureRole` | `Platform` | presence, friends, profiles — what is *about people* |
| `StoreRole` | `Platform` | the storefront |

---

## Capabilities

A provider implements what it implements. Rather than discovering that at the call site, the plugin
asks once and publishes the answer as gameplay tags — fifteen of them, one per component:

```cpp
if (Online->HasFeature(ModularOnlineTags::Feature_Lobbies))
{
    // ...
}
```

Blueprint sees the same through `Has Feature`. An operation on a component the provider lacks returns
a result whose category is `NotSupported` and which names the missing feature, so a caller that did not
ask first still gets a sentence rather than a crash.

---

## Modules

| Module | Type | Contents |
| --- | --- | --- |
| `ModularOnline` | Runtime | contexts, capabilities, identity, matches, features — and the Blueprint surface |
| `ModularOnlineDeveloper` | DeveloperTool | console commands and automation tests |

Blueprint support lives in the runtime module rather than in one of its own: a separate module and a
relay subsystem to feed it were more plumbing than the problem is worth.

The plugin does not reference a host project — no project tags, no project configuration, no project
target rules. Everything platform specific is expressed as trait tags the game passes in.

---

## Installation

Copy the plugin into your project's `Plugins/` directory and enable it:

```
YourProject/Plugins/ModularOnline/ModularOnline.uplugin
```

It enables `OnlineServices` and `OnlineSubsystemUtils` itself. Add `ModularOnline` to the dependencies
of any module that calls into it:

```csharp
PublicDependencyModuleNames.AddRange(new[] { "ModularOnline" });
```

---

## Configuration

Each subject has a section of its own, named for what it configures rather than for where the code
lives:

```ini
[ModularOnline.Providers]
ServiceProvider=Epic
MatchRole=Service
FeatureRole=Platform

[ModularOnline.Matches]
MatchBackend=Lobbies
LobbySchemaId=GameLobby
MatchNameAttribute=Name
MatchMapAttribute=Map
; Attributes a provider publishes about the match itself, which an update must not take off.
ReservedAttributePrefix=__

[ModularOnline.CrossPlay]
CrossPlayPolicy=SingleRole

[ModularOnline.Accounts]
DefaultAvatarAttribute=AvatarUrl

[ModularOnline.Presence]
+States=(State=(TagName="Presence.MainMenu"), Status="Status_MainMenu")
+States=(State=(TagName="Presence.InRun"), Status="Status_InGame", Joinability=FriendsOnly)

[ModularOnline.DedicatedServer]
DefaultServerCredentialsType=Auto
```

Schema and attribute names are configuration because they differ per provider and per project, and the
plugin ships none of them as a default: the names above have to exist in your lobby or session schema, a
name the schema does not know is refused by the service rather than ignored, and a match layer with no
schema id refuses to host rather than guess one.

---

## Using it from C++

```cpp
const auto Users = GetGameInstance()->GetSubsystem<UModularUserSubsystem>();

FModularLoginParams Params;
Params.LocalPlayerIndex = 0;
Params.bAllowLoginUI = true;

Users->LoginLocalUser(Params, FModularUserLoginCompleteDelegate::CreateWeakLambda(this,
    [](const UModularUserInfo* User, const FModularOnlineResult& Result)
    {
        if (!Result.bWasSuccessful)
        {
            // Result.ErrorText is a sentence a player can be shown.
        }
    }));
```

Hosting and searching read the same way through `UModularMatchSubsystem`. Every answer is a
`FModularOnlineResult`: a flag, a provider error id, a localised sentence, and a category the interface
can branch on.

---

## Using it from Blueprint

Four asynchronous nodes cover what a menu actually waits on: signing in, hosting and joining each answer
with one result on `On Success`, `On Failure` and `On Not Supported` — three pins rather than two,
because "this platform cannot do that at all" asks the graph to hide a button for good while "it failed"
asks it to show a message. Finding matches answers with the list instead, on two pins, and the list is
carried on both so a browser can bind one function to each. Every node answers exactly once.

Everything else is a call plus an event on the subsystem: `Query Friends` answers on `On Friends Queried`,
and so on for each facade — every request says when it finished and how, and what it fetched is then read
from the cache.

---

## Localisation

Every sentence the plugin shows a player comes from one string table asset,
`/ModularOnline/StringTables/ModularOnline`, edited in the editor's String Table editor. Call sites read it by
its id with `FText::FromStringTable`; none of them carries text of its own. The engine loads the table the
first time a sentence is asked for, and because only code refers to it, the plugin's `Config/Game.ini` adds
its folder to the directories the cooker always cooks.

English and Russian ship with the plugin in `Content/Localization/ModularOnline` and load on their own:
the plugin declares a localisation target, so the engine finds the translations and packaging stages
them. A packaged game still only carries the cultures its project stages (`CulturesToStage`).

To change a sentence or add a language, edit the table, add the culture to
`Config/Localization/ModularOnline.ini`, and run the engine's pipeline over that file:

```
UnrealEditor-Cmd YourProject.uproject -run=GatherText -config="Plugins/Online/ModularOnline/Config/Localization/ModularOnline.ini" -unattended
```

The first run writes a `.po` per culture; translate it and run again to compile. The paths in that config
name where the plugin sits inside the project, so a project that mounts it elsewhere adjusts them before
regenerating. An English sentence changed without regenerating shows in English in every other language,
and the `ModularOnline.Core.StringTable` test fails until the translations catch up.

---

## What it does not do

- **It is not a provider.** It calls Online Services; it does not implement them. Bring a provider.
- **It does not do matchmaking.** It publishes, searches and joins; deciding who plays with whom is a
  backend's job.
- **It does not do moderation, parties or voice.** Where a provider offers them, reach the component
  directly through `GetInterface<T>()`; the plugin does not stand in front of it.
- **It does not do reservations.** The engine's party beacons match a request against a session looked
  up through the older Online Subsystem, which a project on OSSv2 does not have; checked against Engine
  5.8.3 on 2026-09-13.

---

## Licence

MIT, over the parts authored by PoFig Games Studio. See [LICENSE](LICENSE), and [NOTICE.md](NOTICE.md)
for what this is built against and what that means for you.
