#pragma once

// post a system-defined media key (an NX_KEYTYPE_* value) as a down or up event
// media keys are not regular keyboard events, so remaps to keys like sound_up
// go through here instead of CGEventCreateKeyboardEvent
void postMediaKey(int nxKeyType, bool keyDown);
