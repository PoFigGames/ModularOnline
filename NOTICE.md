# Notices

## Unreal Engine

This plugin is written for Unreal Engine and is of no use without it. Unreal Engine is licensed
separately by Epic Games under the Unreal Engine End User Licence Agreement; **using or redistributing
this plugin requires your own Engine licence.** The Engine itself is not included here.

The plugin is built against Engine headers and stands on Engine types — `UGameInstanceSubsystem`,
`IOnlineServices` and the Online Services v2 component interfaces, `FGameplayTag`, `UDeveloperSettings`,
`UBlueprintAsyncActionBase`. The shape of those is fixed by the Engine rather than chosen here.

What the Engine does **not** contain is the layer this plugin is: the Online Services interfaces are
provider facing, and the code that decides which provider answers a question, what an absent component
means, how a login is sequenced and how any of it reaches Blueprint is left to each project to write.
That code here is written from scratch against the interfaces.

Two Engine plugins were read while writing it, and it is worth saying what was read and what it gave:

- **`OnlineServicesNull`** is the Engine's reference implementation of Online Services v2. It shows what
  a component is expected to look like from the outside — the parameter and result structures, how an
  operation is started and answered. Nothing of it is a game facing layer.
- **`CommonUser`**, from the Lyra sample, solves the neighbouring problem: it is a game facing layer over
  the *older* Online Subsystem, and this plugin exists because that one could not be brought forward. It
  was read as a statement of the problem — what a game asks for when it asks to sign a player in, and
  which platform requirements a login has to satisfy. Its answers are v1 answers and none of them are
  reused: the state machine, the roles, the capability model and the whole match layer here are
  different in structure, not only in wording.

No Engine source file was copied into this repository. Every file here begins with the PoFig Games
Studio copyright line because every file here was written by PoFig Games Studio.

## Providers

The plugin ships no provider and depends on none. It is developed against:

- `OnlineServicesNull`, which ships with the Engine.
- `OnlineServicesEOS`, which ships with the Engine and requires an Epic Games account and its own terms.
- A Steam provider, which is a separate plugin and is not part of this repository.

Steam, Steamworks and the Steam logo are trademarks of Valve Corporation. Epic Games, Unreal and Unreal
Engine are trademarks of Epic Games, Inc. Neither company endorses this plugin.

## Gameplay tags

The plugin declares native gameplay tags under `Online.Feature.*` and `Platform.Trait.*`. The second
namespace is the Engine's own — it is what `CommonUI` reads platform traits from — and is used rather
than invented so that a project declares what is true of each of its platforms once, in the place the
Engine already looks.
