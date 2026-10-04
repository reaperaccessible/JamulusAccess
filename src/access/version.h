// JamulusAccess's own version (1.00, 1.01...): the updater compares it with the latest GitHub release,
// the installer and the Windows file information (src/res/win-mainicon.rc) take it from here.
// APP_VERSION stays the Jamulus version: it is what the servers and the other musicians see.
// To publish a new version: these three lines and jamulusaccess/docs/changelog_*.html (see CLAUDE.md).
// Plain defines only: this file is also read by the Windows resource compiler.
#ifndef JAMULUSACCESS_VERSION_H
#define JAMULUSACCESS_VERSION_H

#define JAMULUSACCESS_VERSION       "1.00"
#define JAMULUSACCESS_VERSION_MAJOR 1
#define JAMULUSACCESS_VERSION_MINOR 0
#define JAMULUSACCESS_GITHUB_REPO   "reaperaccessible/JamulusAccess"

#endif
