# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). While the version is below
1.0 the API is not stable and nothing is deprecated before it changes.

## [0.1]

First public shape of the plugin. Everything below is implemented and builds; see
[Not yet verified](#not-yet-verified) for how much of it has been seen working.

### Added

- **Contexts and roles.** One context per provider per world, built lazily, rebuilt when the world
  changes. `Platform`, `Service` and `Default` roles resolve from configuration, and a project with one
  provider resolves all of them to it.
- **Capabilities.** Fifteen `Online.Feature.*` tags, one per Online Services component, published from
  what the provider actually implements. `HasFeature` in C++ and in Blueprint; an operation on a missing
  component answers `NotSupported` and names the feature.
- **Identity.** A login state machine of `PlatformLogin → TransferAuth → ServiceLogin → PrivilegeCheck`,
  each step required, optional or skipped according to the platform's own trait tags. Guest players,
  press start, user switching, input device assignment, and reaction to the player signing out behind
  the game's back.
- **A refused login says why.** The online services answer a refused privilege with one generic
  sentence, so the reason a player could act on would reach a screen only as the name of an enum.
  `DescribePrivilegeRefusal` gives one localised sentence per result — parental controls, ownership, a
  pending update, no network, the type or standing of the account — and a login that failed its
  privilege check carries it in `ErrorText` beside the code in `ErrorId`. The privilege the login asked
  for stays on the user, so a game can tell a refused `CanPlay` from a refused `CanPlayOnline` and
  answer each where it matters rather than showing one dialog for both.
- **Matches.** `IModularMatchBackend` with a lobby implementation and a session implementation, chosen
  by configuration. Hosting, searching, joining, inviting, kicking, updating settings, travel. A match
  publishes its name and its map; everything else it says about itself is named by the game, so the
  plugin puts nothing in your schema that you did not put there. Hosting and travelling are one call, so
  a match is reached by an address built in one place rather than by each caller that needs one.
- **Cross play.** A second publication on a companion role, rolled back if it fails, searches merged
  across both roles, and the platform's own cross play privilege honoured.
- **Feature facades.** Presence, social, user info, achievements, statistics, leaderboards, cloud saves,
  title files and store — each with a cache, events, and Blueprint access.
- **Dedicated servers.** Signing in as the machine, with the credentials the project names per provider.
  Proving who a joining player is is deliberately not here; see [Known gaps](#known-gaps).
- **A connection that fails is a match the local player is out of.** A server that crashes, times out or
  refuses a player ends the connection and not the lobby, so the online layer would otherwise hear
  nothing and the game would be left standing in a world nobody was talking to any more.
  `UModularMatchSubsystem` listens to the engine's `OnNetworkFailure` and `OnTravelFailure` and answers
  with `OnMatchLeft`: a failure the server sent is `Refused`, because it is a decision it made about this
  player, and everything else is `Disconnected`. `Kicked` belongs to the lobby's own kick and to nothing
  else. Only the game net driver of a client is listened to, so
  a beacon's troubles and a host losing one client are not mistaken for the local player being thrown
  out.
- **Presence says whether a friend can join it.** Each configured state carries its own joinability, so
  being in the front end and being in a run answer differently without the game saying anything.
- **Blueprint surface.** `K2_*` calls and dynamic event twins throughout, and asynchronous nodes for
  signing in, hosting, finding and joining. Every node answers a graph exactly once and then retires;
  three answer with one result on three pins, and the search answers with the list on two.
- **Configuration.** Six sections named for their subject — `Providers`, `Matches`, `CrossPlay`,
  `Accounts`, `Presence`, `DedicatedServer` — rather than for the class that reads them. Attributes a
  provider publishes about a match itself are recognised by a configured prefix and left alone when the
  game updates its own. No schema id, attribute name or account key ships as a default: a project names
  them or the match layer refuses to publish, rather than guessing at somebody else's schema.
- **Console commands.** `ModularOnline.Status`, `.Refresh`, `.HasFeature`.
- **Automation tests.** Sixteen, over the parts that are pure rules: error mapping, role resolution,
  cross play policy, presence states, match handles, settings, published text, reserved attributes, the
  merge of two searches, and which steps a login walks when nobody signs anybody in.

### Not yet verified

Built and exercised against a single provider on a single machine. **Nothing multi-machine has been
confirmed.** In particular:

- Hosting and joining between two machines, on any provider.
- Cross play with two providers publishing the same match.
- A dedicated server: signing in and publishing end to end.
- The session backend. The lobby backend is what a game has actually run on.
- Achievements, statistics, leaderboards, cloud saves, title files and the store against a live backend
  rather than against Null.
- Guest players and user switching on a platform that has more than one account.

### Known gaps

- No debug overlay. The rules a login and a search walk by are tested; the flows themselves, end to end
  against a provider, are not — that needs a services double this plugin does not carry.
- Proving who a joining player is has no place here. It belongs to the connection, before there is a
  login to refuse, and so to whatever carries that connection — on Steam a packet handler, on Epic an ID
  token the client copies and the server verifies. Online Services v2 declares operations for it, but
  every one of them is an unimplemented stub in the Epic provider; checked against Engine 5.8.3 on
  2026-09-13.
- Reservations are deliberately absent: the Engine's party beacons match a request against a session
  looked up through the older Online Subsystem, which a project on Online Services v2 does not have.
  Checked against Engine 5.8.3 on 2026-09-13.
- Moderation, parties and voice have no facade. Where a provider offers them, reach the component
  through `GetInterface<T>()`.
