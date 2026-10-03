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

#include <functional>
#include <QString>
#include <QStringList>
#include <QKeySequence>

class QAbstractItemView;
class QAbstractSlider;
class QKeyEvent;
class QModelIndex;
class QObject;
class QTreeWidget;
class QTreeWidgetItem;
class QVariant;
class QWidget;

// Screen reader helpers shared by the accessibility work (JamulusAccess)
namespace Access
{
// Spoken by NVDA, JAWS and Narrator through the platform accessibility API (Qt 6.8+), queued after
// what the screen reader is saying (it never cuts it). Assertive is for alerts.
void announce ( QObject* source, const QString& text, bool assertive = false );

// A value that follows another (a meter, the Peak Watcher): it cuts what the screen reader is
// saying, so only the latest one is heard. UI Automation notification "most recent" (NVDA, JAWS);
// Qt only offers the queued kind.
void announceLatest ( QWidget* window, const QString& text );

// Application-wide filters, installed once at start-up (`owner` keeps them alive):
// - popup menus open with their first item selected, as in Windows (Qt opens them silent);
// - every dialog names its fields after their labels when it is shown;
// - HTML tooltips become plain-text accessible descriptions;
// - accessible interfaces of the widgets registered below (slider value texts, ignored meters).
void installApplicationFixes ( QObject* owner );

// <modifiers>+<digit key>, matched on the physical digit key (any keyboard layout, AZERTY included),
// for the widgets of `window` only: `handler` gets the digit and returns true when it used it.
// AltGr+digit typing a character in a text field is left alone.
void installDigitKeys ( QWidget* window, Qt::KeyboardModifiers modifiers, std::function<bool ( int digit )> handler );

// Give the focus to `widget`; when it already has it, announce what the screen reader says on arrival
// (name, then current item or value), so a shortcut pressed twice speaks the same thing twice
void focusAndSpeak ( QWidget* widget );

// Keyboard shortcut to a place: exactly one screen reader message, the same whether the user comes
// from elsewhere or is already there. An item view gets a current item; an empty one says "Empty".
void goTo ( QWidget* target );

// Label text as a screen reader should say it: no '&' mnemonic, no HTML, no trailing ':' or "..."
QString cleanLabel ( const QString& text );

// Dialog pass: every input control without an explicit accessible name is named after the label on its
// left in the same layout row (or above it), and the label becomes its buddy. Icon-only buttons are
// named after their tooltip. Scroll areas stop being nameless Tab stops.
void labelControls ( QWidget* root );

// Up on the first row / Down on the last row: announce "First, <row>" / "Last, <row>" instead of
// staying silent (also with one row). Installed as an event filter on `view`.
void installListEdges ( QAbstractItemView* view );
bool announceListEdge ( QAbstractItemView* view, const QKeyEvent* event );

// A value slider of the family: Alt+Up/Down change the value by 5 % of its range, Alt+Shift+Up/Down by
// the smallest step; "Minimum" / "Maximum" at the ends. The plain keys (arrows, Page, Home, End) do
// nothing and say how to change the value, which is also the slider's accessible description.
// `valueText` (optional) is what the screen reader says for a value ("Center", "Left 20", "80 %").
// Call before the slider is shown.
void installSliderKeys ( QAbstractSlider* slider, std::function<QString ( int value )> valueText = {} );

// The slider help, as spoken
QString sliderHelp();

// A plain container for the screen reader (no name, no "dialog"): a dialog shown as a view inside
// another window. Call before the widget is shown.
void presentAsPane ( QWidget* widget );

// A widget the screen reader must ignore as a value (level meter bars change many times a second:
// NVDA would beep or speak them). It keeps its name. Call before the widget is shown.
void ignoreValue ( QWidget* widget );

// A tree of rows with columns: every row's screen reader text is the whole row ("Name, Ping 25 ms,
// Musicians 3/10..."), built by `rowText`. The text of the current row is only refreshed when the
// focus leaves it, so values changing every second (ping) are not spoken again and again.
void keepTreeRowTexts ( QTreeWidget* tree, std::function<QString ( const QModelIndex& row )> rowText );

// Reading a tree row cell by cell. `cells` gives the informations of a row, each as "Header value"
// ("Ping Time 25 ms"). Left/Right go from one to the next ("First," / "Last," at the ends; Left before the
// first information is the whole row again); Up/Down keep the information chosen, so the next row says
// the same column; Home/End stay the first and last rows, as in every list. The row items must answer Qt::AccessibleTextRole with cellReadingText(): the text is
// computed when the screen reader asks, so values changing every second are never spoken by themselves.
void     installCellReading ( QTreeWidget* tree, std::function<QStringList ( const QTreeWidgetItem* item )> cells );
QVariant cellReadingText ( const QTreeWidgetItem* item );

// The current row of `view` has a new text: make the screen reader say it, speech and braille (Qt does
// not tell it). Qt drops its accessible objects when the rows are sorted again (pings, a musician who
// joins): the screen reader then still holds the old object and would ignore a name change of the new
// one, so the row is given the focus again instead. The view must be registered with trackItemFocus().
void trackItemFocus ( QAbstractItemView* view );
void notifyCurrentItemTextChanged ( QAbstractItemView* view );

// Shift+F1: the "What's This" explanation of `control` (or of the nearest parent that has one) in a
// small read-only box, readable with the arrows and on braille; Escape closes it. Jamulus shows it on
// a mouse click only. Installed application-wide by installApplicationFixes().
void showControlHelp ( QWidget* control );

// Help list of the family (Ctrl+Shift+H): a dialog with a list of "Shortcut: action" rows
void showShortcutHelp ( QWidget* parent, const QString& title, const QStringList& rows );
} // namespace Access
