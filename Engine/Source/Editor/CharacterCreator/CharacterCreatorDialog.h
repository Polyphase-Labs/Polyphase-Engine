#pragma once

#if EDITOR

#include "CharacterRigBuilder.h"

/**
 * @file CharacterCreatorDialog.h
 * @brief Tools > Create > First / Third Person Character dialog.
 */

/** Opens the dialog for the given rig type (resets state, prefills defaults). */
void OpenCreateCharacterDialog(CharacterRigType type);

/** Renders the dialog. Call once per frame from EditorImguiDraw(), outside any popup. */
void DrawCharacterCreatorDialogs();

#endif // EDITOR
