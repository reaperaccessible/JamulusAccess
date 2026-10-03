/******************************************************************************\
 * Copyright (c) 2026
 *
 * Author(s):
 *  JamulusAccess
 *
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
\******************************************************************************/

#pragma once

// Off-screen accessibility test driver (test builds only, CONFIG+=accesstest).
// JAMULUSACCESS_SCRIPT=<file> runs a script of keys once the GUI is up, and JAMULUSACCESS_LOG=<file>
// receives what a screen reader would be told: focus, names, values, announcements. Run with
// "-platform offscreen" so nothing appears on the screen and no key reaches another window.
//
// Script commands, one per line ('#' starts a comment):
//   wait <ms>              let the application run
//   key <sequence> [count] press keys on the focused widget, e.g. "key Tab", "key Alt+Up 3"
//   type <text>            type text on the focused widget
//   focus                  log the focused widget as the screen reader would read it
//   tab                    log every Tab stop of the active window, in order
//   log <text>             write a line to the log
//   quit                   close the application
namespace AccessTest
{
void start();
}
