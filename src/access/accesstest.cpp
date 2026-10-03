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

#include "accesstest.h"

#include <QAbstractItemView>
#include <QAccessible>
#include <QApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QFile>
#include <QKeySequence>
#include <QPointer>
#include <QTextStream>
#include <QTimer>
#include <QWidget>
#include <QtTest/QTest>

namespace
{
QFile         logFile;
QElapsedTimer runClock;
QStringList   script;
int           line = 0;

void write ( const QString& text )
{
    const QString stamped = QString ( "%1 %2\n" ).arg ( runClock.elapsed(), 6 ).arg ( text );
    logFile.write ( stamped.toUtf8() );
    logFile.flush();
}

QString roleName ( QAccessible::Role role )
{
    switch ( role )
    {
    case QAccessible::PushButton:
        return "button";
    case QAccessible::CheckBox:
        return "check box";
    case QAccessible::RadioButton:
        return "radio button";
    case QAccessible::ComboBox:
        return "combo box";
    case QAccessible::EditableText:
        return "edit";
    case QAccessible::Slider:
        return "slider";
    case QAccessible::SpinBox:
        return "spin box";
    case QAccessible::List:
        return "list";
    case QAccessible::ListItem:
        return "list item";
    case QAccessible::Tree:
        return "tree";
    case QAccessible::TreeItem:
        return "tree item";
    case QAccessible::PageTab:
        return "tab";
    case QAccessible::PageTabList:
        return "tab list";
    case QAccessible::MenuItem:
        return "menu item";
    case QAccessible::PopupMenu:
        return "menu";
    case QAccessible::MenuBar:
        return "menu bar";
    case QAccessible::StaticText:
        return "text";
    case QAccessible::Grouping:
        return "grouping";
    case QAccessible::Dialog:
        return "dialog";
    case QAccessible::Window:
        return "window";
    case QAccessible::ProgressBar:
        return "progress bar";
    case QAccessible::Table:
        return "table";
    case QAccessible::Cell:
        return "cell";
    default:
        return QString ( "role %1" ).arg ( static_cast<int> ( role ) );
    }
}

// What a screen reader says for an object: name, role, value, state, description
QString describe ( QAccessibleInterface* iface )
{
    if ( !iface || !iface->isValid() )
    {
        return "(none)";
    }
    QStringList parts { iface->text ( QAccessible::Name ), roleName ( iface->role() ) };
    const QString value = iface->text ( QAccessible::Value );
    if ( !value.isEmpty() )
    {
        parts << value;
    }
    const QAccessible::State state = iface->state();
    if ( state.checkable )
    {
        parts << ( state.checked ? "checked" : "not checked" );
    }
    if ( state.disabled )
    {
        parts << "unavailable";
    }
    const QString description = iface->text ( QAccessible::Description );
    if ( !description.isEmpty() )
    {
        parts << "[" + description + "]";
    }
    return parts.join ( " | " );
}

bool isFocused ( QAccessibleEvent* event )
{
    QWidget* focus = QApplication::focusWidget();
    return focus && event->object() == focus;
}

void updateHandler ( QAccessibleEvent* event )
{
    switch ( event->type() )
    {
    case QAccessible::Focus:
    {
        // UI Automation moves the focus to the current item of a list or tree (Qt's provider does)
        QAccessibleInterface* iface = event->accessibleInterface();
        QString               text  = describe ( iface );
        if ( iface && iface->childCount() )
        {
            if ( QAccessibleInterface* child = iface->focusChild(); child && child != iface )
            {
                text += "  >>  " + describe ( child );
            }
        }
        write ( "FOCUS " + text );
        break;
    }
    case QAccessible::NameChanged:
        if ( isFocused ( event ) )
        {
            write ( "NAME  " + describe ( event->accessibleInterface() ) );
        }
        break;
    case QAccessible::ValueChanged:
        if ( isFocused ( event ) )
        {
            write ( "VALUE " + describe ( event->accessibleInterface() ) );
        }
        break;
    case QAccessible::StateChanged:
        if ( isFocused ( event ) )
        {
            write ( "STATE " + describe ( event->accessibleInterface() ) );
        }
        break;
    case QAccessible::Announcement:
        write ( "SAY   " + static_cast<QAccessibleAnnouncementEvent*> ( event )->message() );
        break;
    case QAccessible::DialogStart:
    case QAccessible::PopupMenuStart:
        write ( "OPEN  " + describe ( event->accessibleInterface() ) );
        break;
    default:
        break;
    }
}

void logTabStops()
{
    QWidget* window = QApplication::activeWindow();
    if ( !window )
    {
        write ( "TAB   (no active window)" );
        return;
    }
    write ( "TAB   window: " + window->windowTitle() );
    int n = 0;
    for ( QWidget* w = window->nextInFocusChain(); w && w != window && n < 500; w = w->nextInFocusChain() )
    {
        if ( ( w->focusPolicy() & Qt::TabFocus ) && w->isVisible() && w->isEnabled() && w->window() == window && !w->focusProxy() )
        {
            write ( QString ( "TAB   %1. " ).arg ( ++n ) + describe ( QAccessible::queryAccessibleInterface ( w ) ) + "  <" +
                    w->metaObject()->className() + " " + w->objectName() + ">" );
        }
    }
}

void runNext();

void sendKeys ( const QString& sequence, int count )
{
    const QKeySequence keys ( sequence );
    if ( keys.isEmpty() )
    {
        write ( "ERROR unknown key " + sequence );
        return;
    }
    for ( int i = 0; i < count; i++ )
    {
        QWidget* target = QApplication::focusWidget();
        if ( QApplication::activeModalWidget() && ( !target || target->window() != QApplication::activeModalWidget() ) )
        {
            target = QApplication::activeModalWidget();
        }
        if ( !target )
        {
            target = QApplication::activeWindow();
        }
        if ( !target )
        {
            // the off-screen platform does not give the activation back when a box closes (Windows
            // does): activate the main window, as Windows would
            for ( QWidget* w : QApplication::topLevelWidgets() )
            {
                if ( w->isVisible() && !w->parentWidget() && qobject_cast<QDialog*> ( w ) && w->property ( "accessKeepOnEscape" ).toBool() )
                {
                    w->activateWindow();
                    QApplication::setActiveWindow ( w );
                    target = QApplication::focusWidget() ? QApplication::focusWidget() : w;
                    write ( "NOTE  main window activated again" );
                    break;
                }
            }
        }
        if ( !target )
        {
            write ( "ERROR no focus for key " + sequence );
            return;
        }
        // from the event loop: a key that opens a modal box does not block the script
        const QKeyCombination combination = keys[0];
        QTimer::singleShot ( 0, qApp, [target = QPointer<QWidget> ( target ), combination] {
            if ( target )
            {
                QTest::keyClick ( target, combination.key(), combination.keyboardModifiers() );
            }
        } );
    }
}

void runNext()
{
    while ( line < script.size() )
    {
        const QString text = script[line++].trimmed();
        if ( text.isEmpty() || text.startsWith ( '#' ) )
        {
            continue;
        }
        const QString command  = text.section ( ' ', 0, 0 );
        const QString argument = text.section ( ' ', 1 );

        if ( command == "wait" )
        {
            QTimer::singleShot ( argument.toInt(), qApp, runNext );
            return;
        }
        if ( command == "key" )
        {
            // "key Alt+Up 3": the last word is a count when it is a number
            QString sequence = argument;
            int     count    = 1;
            bool    ok       = false;
            const int last   = argument.section ( ' ', -1 ).toInt ( &ok );
            if ( ok && argument.contains ( ' ' ) )
            {
                count    = last;
                sequence = argument.section ( ' ', 0, -2 );
            }
            write ( "KEY   " + text.mid ( 4 ) );
            sendKeys ( sequence, count );
            QTimer::singleShot ( 150, qApp, runNext ); // let the events settle
            return;
        }
        if ( command == "type" )
        {
            write ( "TYPE  " + argument );
            QTimer::singleShot ( 0, qApp, [argument] {
                if ( QWidget* target = QApplication::focusWidget() )
                {
                    QTest::keyClicks ( target, argument );
                }
            } );
            QTimer::singleShot ( 150, qApp, runNext );
            return;
        }
        if ( command == "focus" )
        {
            write ( "IS    " + describe ( QAccessible::queryAccessibleInterface ( QApplication::focusWidget() ) ) );
            continue;
        }
        if ( command == "tab" )
        {
            logTabStops();
            continue;
        }
        if ( command == "rows" )
        {
            // the rows of the focused list or tree, as the screen reader reads them
            if ( auto* view = qobject_cast<QAbstractItemView*> ( QApplication::focusWidget() ) )
            {
                const QAbstractItemModel* model = view->model();
                const std::function<void ( const QModelIndex&, int )> walk = [&] ( const QModelIndex& parent, int depth ) {
                    for ( int r = 0; r < model->rowCount ( parent ); r++ )
                    {
                        const QModelIndex index = model->index ( r, 0, parent );
                        QString           text  = index.data ( Qt::AccessibleTextRole ).toString();
                        if ( text.isEmpty() )
                        {
                            text = index.data().toString();
                        }
                        write ( "ROW   " + QString ( depth * 2, ' ' ) + ( index == view->currentIndex() ? "> " : "  " ) + text );
                        walk ( index, depth + 1 );
                    }
                };
                walk ( view->rootIndex(), 0 );
            }
            else
            {
                write ( "ROW   (focus is not a list)" );
            }
            continue;
        }
        if ( command == "log" )
        {
            write ( "----- " + argument );
            continue;
        }
        if ( command == "quit" )
        {
            write ( "QUIT" );
            // straight out: closing the window would ask "Disconnect and quit?"
            QTimer::singleShot ( 0, qApp, [] { QApplication::exit ( 0 ); } );
            return;
        }
        write ( "ERROR unknown command " + text );
    }
}
} // namespace

void AccessTest::start()
{
    const QString scriptPath = qEnvironmentVariable ( "JAMULUSACCESS_SCRIPT" );
    if ( scriptPath.isEmpty() )
    {
        return;
    }
    logFile.setFileName ( qEnvironmentVariable ( "JAMULUSACCESS_LOG", "accesstest.log" ) );
    logFile.open ( QIODevice::WriteOnly | QIODevice::Truncate );
    runClock.start();

    QFile file ( scriptPath );
    if ( file.open ( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        script = QString::fromUtf8 ( file.readAll() ).split ( '\n' );
    }

    QAccessible::installUpdateHandler ( updateHandler );
    QAccessible::setActive ( true );
    write ( "START " + scriptPath );
    QTimer::singleShot ( 1000, qApp, runNext );
}
