# 1/2 Hotkey Prone Trigger Fix Plan

## Context
- `ABG_PlayerController::BindPawnInput()` binds temporary weapon equip to `EKeys::One` and `EKeys::Two`.
- The same controller also binds `InputConfig.ProneAction` through Enhanced Input.
- In code, `1` and `2` do not directly call `ToggleProneFromInput()`.
- Therefore the most likely cause is an overlapping key assignment inside the active input mapping context asset such as `Content/WON/Input/IMC_BG.uasset`, where `ProneAction` is also mapped to `1` or `2`.

## Goal
- Pressing `1` and `2` should only trigger the temporary equip logic.
- Pressing the actual prone key should still trigger prone normally.

## Proposed Changes
1. Inspect the active mapping context asset bound to `InputConfig.DefaultMappingContext` and remove `1` / `2` from the `ProneAction` mapping.
2. Keep the existing temporary equip bindings in `ABG_PlayerController`.
3. Add a lightweight validation pass in the player controller after mapping refresh or during QA, so accidental overlap between temporary equip keys and stance input can be caught early.
4. Verify that no other stance action such as crouch is also sharing `1` / `2`.

## Implementation Notes
- Preferred fix location: input asset mapping, because the issue appears to be configuration-level rather than gameplay-logic-level.
- Optional fallback: if the team expects frequent temporary key experiments, migrate `1` / `2` equip input into Enhanced Input action assets as well, so all keyboard ownership is managed in one place.
- Keep UE5 naming conventions intact.
- If runtime guard code is added, do not silently ignore conflicts; emit an explicit log describing the overlapping action and key.

## Verification
1. Launch PIE as client.
2. Press `1`: weapon pose changes to pistol and prone does not trigger.
3. Press `2`: weapon pose changes to rifle and prone does not trigger.
4. Press the intended prone key: prone still toggles correctly.
5. Repeat in dedicated server multiplayer PIE to confirm local input still replicates stance and weapon pose as expected.

## Risks
- If the overlap exists only in a Blueprint subclass default rather than the shared IMC asset, fixing the asset alone may not be sufficient.
- If multiple mapping contexts are added at runtime, another context could still reintroduce the same collision.
