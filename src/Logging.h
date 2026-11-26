// File logging + a last-resort Windows crash writer.
//
// The v1 dial died silently when a huge move blew up; every run now leaves a
// readable log in %LOCALAPPDATA%\AMS\AMS Sortie\logs so a bad job can be
// diagnosed instead of guessed at.
#pragma once

#include <QString>

namespace Log {

void init();
QString filePath();
QString folderPath();

} // namespace Log
