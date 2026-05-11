# Agent Rules

- **Language**: Respond in Korean by default. Write code comments in Korean if the file already has Korean comments, otherwise in English. Only add comments when the intent is non-obvious — never restate what the code already expresses.
- **Plan before implement**: Unless explicitly asked to implement, only create a plan.
- **Ask when ambiguous**: Ask the user before proceeding if anything is unclear.
- **Non-trivial plans**: Write plans to `Docs/Plan-**.md` so the user can review and refine them before implementation.
- **Design principles**: Follow SOLID principles. Prefer ActorComponent-based design for modularity.
- **Null checks**: After a null check, never silent-fail. Log an explicit error describing what was null and where.
- **Naming conventions**: Follow UE5 standard naming conventions.
- **Forward declarations**: Declare classes inline rather than in separate blocks or headers.

## Environments

- UE 5.7 (Source Build), VC++ 2022
- Multiplayer Game with Dedicated Server
- Each user has their own directory; `Source/PUBG_HotMode/USERNAME` and `Content/USERNAME`
