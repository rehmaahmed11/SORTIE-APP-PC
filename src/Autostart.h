#pragma once

// "Start with Windows" for the current user (HKCU Run key, no admin needed).
namespace Autostart {

bool isEnabled();
void setEnabled(bool on);

} // namespace Autostart
