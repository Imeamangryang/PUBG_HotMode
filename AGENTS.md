# Agent Rules

## Workflow

- Plan only unless explicitly asked to implement; write non-trivial plans to `Docs/Plan-**.md`
- Ask only when ambiguity affects outcome; keep edits scoped and preserve unrelated changes

## Code

- Follow UE5 naming and SOLID; prefer ActorComponent-based modularity
- Null checks must log explicit errors; never silent-fail

## Project

- UE 5.7 (Source Build), VC++ 2022
- Dedicated-server multiplayer game
- User-owned dirs: `Source/PUBG_HotMode/USERNAME`, `Content/USERNAME`

## Build Safety

- Never touch Engine source/build outputs or run commands that can compile/relink Engine modules
- Verify C++ only with project module build: `-Module=PUBG_HotMode -NoEngineChanges`
- Do not use Rider Build/Rebuild if it triggers full target or Engine builds
- If Editor/Live Coding blocks command-line build, report failure and stop; no workaround retries
