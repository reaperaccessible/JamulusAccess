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

#include "accessibility.h"

#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QAccessible>
#include <QAccessibleWidget>
#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{


/* Registered widgets: their accessible interface comes from our factory ******/
struct SliderEntry
{
    std::function<QString ( int )> valueText;
};

QHash<const QObject*, SliderEntry>& sliders()
{
    static QHash<const QObject*, SliderEntry> registry;
    return registry;
}

QSet<const QObject*>& ignoredValues()
{
    static QSet<const QObject*> registry;
    return registry;
}

QSet<const QObject*>& panes()
{
    static QSet<const QObject*> registry;
    return registry;
}

class AccessiblePane : public QAccessibleWidget
{
public:
    explicit AccessiblePane ( QWidget* widget ) : QAccessibleWidget ( widget, QAccessible::Pane ) {}

    QString text ( QAccessible::Text t ) const override
    {
        if ( t == QAccessible::Name || t == QAccessible::Description )
        {
            return {};
        }
        return QAccessibleWidget::text ( t );
    }
};

// A slider whose value is said as text ("Center", "80 %") and whose plain keys are blocked
class AccessibleSlider : public QAccessibleWidget, public QAccessibleValueInterface
{
public:
    explicit AccessibleSlider ( QAbstractSlider* slider ) : QAccessibleWidget ( slider, QAccessible::Slider ) {}

    void* interface_cast ( QAccessible::InterfaceType type ) override
    {
        if ( type == QAccessible::ValueInterface )
        {
            return static_cast<QAccessibleValueInterface*> ( this );
        }
        return QAccessibleWidget::interface_cast ( type );
    }

    QString text ( QAccessible::Text t ) const override
    {
        if ( t == QAccessible::Value )
        {
            const auto it = sliders().constFind ( object() );
            if ( it != sliders().cend() && it->valueText )
            {
                return it->valueText ( slider()->value() );
            }
            return QString::number ( slider()->value() );
        }
        if ( t == QAccessible::Description && QAccessibleWidget::text ( t ).isEmpty() )
        {
            return Access::sliderHelp();
        }
        return QAccessibleWidget::text ( t );
    }

    QVariant currentValue() const override { return slider()->value(); }
    void     setCurrentValue ( const QVariant& value ) override { slider()->setValue ( value.toInt() ); }
    QVariant maximumValue() const override { return slider()->maximum(); }
    QVariant minimumValue() const override { return slider()->minimum(); }
    QVariant minimumStepSize() const override { return slider()->singleStep(); }

private:
    QAbstractSlider* slider() const { return static_cast<QAbstractSlider*> ( object() ); }
};

// A meter: a name, no value (it changes many times a second)
class AccessibleIgnoredValue : public QAccessibleWidget
{
public:
    explicit AccessibleIgnoredValue ( QWidget* widget ) : QAccessibleWidget ( widget, QAccessible::StaticText ) {}

    void* interface_cast ( QAccessible::InterfaceType type ) override
    {
        if ( type == QAccessible::ValueInterface )
        {
            return nullptr;
        }
        return QAccessibleWidget::interface_cast ( type );
    }

    QString text ( QAccessible::Text t ) const override
    {
        if ( t == QAccessible::Value )
        {
            return {};
        }
        return QAccessibleWidget::text ( t );
    }
};

bool isIgnored ( const QObject* object )
{
    for ( const QObject* o = object; o; o = o->parent() )
    {
        if ( ignoredValues().contains ( o ) )
        {
            return true;
        }
    }
    return false;
}

QAccessibleInterface* accessibleFactory ( const QString&, QObject* object )
{
    if ( !object || !object->isWidgetType() )
    {
        return nullptr;
    }
    if ( sliders().contains ( object ) )
    {
        return new AccessibleSlider ( static_cast<QAbstractSlider*> ( object ) );
    }
    if ( isIgnored ( object ) )
    {
        return new AccessibleIgnoredValue ( static_cast<QWidget*> ( object ) );
    }
    if ( panes().contains ( object ) )
    {
        return new AccessiblePane ( static_cast<QWidget*> ( object ) );
    }
    return nullptr;
}

// An interface created before the registration (a name set during construction creates one) would
// stay in Qt's cache: drop it, the next query goes through the factory
void dropCachedInterface ( QObject* object )
{
    if ( QAccessibleInterface* iface = QAccessible::queryAccessibleInterface ( object ) )
    {
        QAccessible::deleteAccessibleInterface ( QAccessible::uniqueId ( iface ) );
    }
}

/* Dialog labelling ***********************************************************/
bool isInput ( const QWidget* w )
{
    if ( qobject_cast<const QAbstractSpinBox*> ( w ) || qobject_cast<const QComboBox*> ( w ) || qobject_cast<const QAbstractItemView*> ( w ) ||
         qobject_cast<const QPlainTextEdit*> ( w ) || qobject_cast<const QTextEdit*> ( w ) || qobject_cast<const QAbstractSlider*> ( w ) )
    {
        return true;
    }
    if ( qobject_cast<const QLineEdit*> ( w ) )
    {
        // the inner line edit of a spin box or combo box is not a control of its own
        const QWidget* p = w->parentWidget();
        return !( qobject_cast<const QAbstractSpinBox*> ( p ) || qobject_cast<const QComboBox*> ( p ) );
    }
    return false;
}

// A widget whose text can name the control next to it
QString labelText ( const QWidget* w )
{
    if ( !w || w->isHidden() )
    {
        return {};
    }
    if ( const auto* label = qobject_cast<const QLabel*> ( w ) )
    {
        return Access::cleanLabel ( label->text() );
    }
    if ( qobject_cast<const QCheckBox*> ( w ) || qobject_cast<const QRadioButton*> ( w ) )
    {
        return Access::cleanLabel ( static_cast<const QAbstractButton*> ( w )->text() );
    }
    return {};
}

QWidget* lastLabelIn ( QLayout* layout )
{
    for ( int i = layout->count() - 1; i >= 0; --i )
    {
        if ( QWidget* w = layout->itemAt ( i )->widget(); w && !labelText ( w ).isEmpty() )
        {
            return w;
        }
    }
    return nullptr;
}

struct Slot
{
    QLayout* layout = nullptr;
    int      index  = -1;
};

Slot findSlot ( QLayout* root, const QObject* target )
{
    for ( int i = 0; i < root->count(); ++i )
    {
        QLayoutItem* item = root->itemAt ( i );
        if ( ( item->widget() && ( item->widget() == target ) ) || ( item->layout() && ( item->layout() == target ) ) )
        {
            return { root, i };
        }
        if ( QLayout* sub = item->layout() )
        {
            if ( const Slot slot = findSlot ( sub, target ); slot.layout )
            {
                return slot;
            }
        }
    }
    return {};
}

// Label on the left of `slot` in the same row (or right above it in a column)
QWidget* labelBeside ( const Slot& slot )
{
    const auto check = [] ( QLayoutItem* item, QWidget*& found ) -> bool // true = stop scanning
    {
        if ( !item )
        {
            return false;
        }
        if ( QWidget* w = item->widget() )
        {
            if ( !labelText ( w ).isEmpty() )
            {
                found = w;
                return true;
            }
            return isInput ( w ) && !w->isHidden(); // another control in between: its label is not ours
        }
        if ( QLayout* sub = item->layout() )
        {
            if ( QWidget* w = lastLabelIn ( sub ) )
            {
                found = w;
                return true;
            }
        }
        return false;
    };

    QWidget* left = nullptr;
    if ( auto* form = qobject_cast<QFormLayout*> ( slot.layout ) )
    {
        int                    row  = -1;
        QFormLayout::ItemRole role = {};
        form->getItemPosition ( slot.index, &row, &role );
        if ( role == QFormLayout::FieldRole )
        {
            check ( form->itemAt ( row, QFormLayout::LabelRole ), left );
        }
    }
    else if ( auto* grid = qobject_cast<QGridLayout*> ( slot.layout ) )
    {
        int row = 0, column = 0, rowSpan = 0, columnSpan = 0;
        grid->getItemPosition ( slot.index, &row, &column, &rowSpan, &columnSpan );
        for ( int c = column - 1; c >= 0; --c )
        {
            if ( check ( grid->itemAtPosition ( row, c ), left ) )
            {
                break;
            }
        }
        if ( !left && row > 0 )
        {
            // label above the field
            check ( grid->itemAtPosition ( row - 1, column ), left );
        }
    }
    else if ( auto* box = qobject_cast<QBoxLayout*> ( slot.layout ) )
    {
        const bool horizontal = ( box->direction() == QBoxLayout::LeftToRight ) || ( box->direction() == QBoxLayout::RightToLeft );
        if ( horizontal )
        {
            for ( int i = slot.index - 1; i >= 0; --i )
            {
                if ( check ( box->itemAt ( i ), left ) )
                {
                    break;
                }
            }
        }
        else if ( slot.index > 0 )
        {
            if ( QWidget* w = box->itemAt ( slot.index - 1 )->widget(); qobject_cast<QLabel*> ( w ) && !labelText ( w ).isEmpty() )
            {
                left = w;
            }
        }
    }
    if ( left && ( left->isHidden() || labelText ( left ).isEmpty() || isInput ( left ) ) )
    {
        left = nullptr;
    }
    return left;
}

QWidget* findLabel ( QWidget* control )
{
    QWidget* parent = control->parentWidget();
    QLayout* top    = parent ? parent->layout() : nullptr;
    if ( !top )
    {
        return nullptr;
    }

    const QObject* target = control;
    for ( int depth = 0; depth < 8; ++depth )
    {
        const Slot slot = findSlot ( top, target );
        if ( !slot.layout )
        {
            break;
        }
        if ( QWidget* left = labelBeside ( slot ) )
        {
            return left;
        }
        if ( slot.layout == top )
        {
            break;
        }
        target = slot.layout; // the row may be a sub-layout inside a grid cell: look around it
    }
    return nullptr;
}

// Icon-only buttons: their tooltip is their name, and stays so when the tooltip changes
class TooltipNameFilter final : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter ( QObject* watched, QEvent* event ) override
    {
        if ( event->type() == QEvent::ToolTipChange )
        {
            auto* button = static_cast<QWidget*> ( watched );
            button->setAccessibleName ( Access::cleanLabel ( button->toolTip() ) );
        }
        return QObject::eventFilter ( watched, event );
    }
};

/* Application-wide filters ***************************************************/
const char AUTO_DESCRIPTION[] = "accessAutoDescription";

// A widget's tooltip is its accessible description when it has none of its own: an HTML tooltip
// was read tag by tag by screen readers and on braille displays
void plainTooltipDescription ( QWidget* widget )
{
    const bool autoSet = widget->property ( AUTO_DESCRIPTION ).toBool();
    if ( !autoSet && !widget->accessibleDescription().isEmpty() )
    {
        return; // a description of our own wins
    }

    const QString tip = widget->toolTip();
    if ( tip.isEmpty() || !Qt::mightBeRichText ( tip ) )
    {
        if ( autoSet ) // the HTML tooltip is gone: let Qt use the new one again
        {
            widget->setAccessibleDescription ( {} );
            widget->setProperty ( AUTO_DESCRIPTION, false );
        }
        return;
    }
    QString text = QTextDocumentFragment::fromHtml ( tip ).toPlainText();
    text.replace ( QChar::LineSeparator, ' ' ).replace ( QChar::ParagraphSeparator, ' ' ).replace ( '\n', ' ' );
    // the end of Jamulus tooltips sends to the mouse "What's This" help: nothing for the keyboard
    text.remove ( QCoreApplication::translate ( "global", "For more information use the \"What's This\" help (help menu, right mouse button or Shift+F1)" ) );
    widget->setAccessibleDescription ( text.simplified() );
    widget->setProperty ( AUTO_DESCRIPTION, true );
}

class ApplicationFilter final : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter ( QObject* watched, QEvent* event ) override
    {
        switch ( event->type() )
        {
        case QEvent::Polish:
        case QEvent::ToolTipChange:
            if ( watched->isWidgetType() )
            {
                plainTooltipDescription ( static_cast<QWidget*> ( watched ) );
            }
            break;

        case QEvent::Show:
            if ( auto* dialog = qobject_cast<QDialog*> ( watched ) )
            {
                Access::labelControls ( dialog );
            }
            if ( auto* menu = qobject_cast<QMenu*> ( watched ) )
            {
                menuShown ( menu );
            }
            break;

        case QEvent::Hide:
            if ( auto* menu = qobject_cast<QMenu*> ( watched ) )
            {
                submenuClosed ( menu );
            }
            break;

        case QEvent::KeyPress:
        {
            // Enter presses the focused button, as everywhere in Windows (Jamulus turned it off on some
            // buttons, "Connect" among them, so that Enter elsewhere would not press them)
            auto*       button = qobject_cast<QPushButton*> ( watched );
            const auto* key    = static_cast<QKeyEvent*> ( event );
            const bool  enter  = ( key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter ) &&
                               ( key->modifiers() & ~Qt::KeypadModifier ) == Qt::NoModifier;
            if ( enter && button && button->hasFocus() && !button->autoDefault() && !button->isDefault() )
            {
                button->animateClick();
                return true;
            }

            // Enter switches a check box too, as Space does (Jamulus's Chat and Settings boxes open
            // things like buttons), unless the window has a default button that Enter presses
            auto* box = qobject_cast<QCheckBox*> ( watched );
            if ( enter && box && box->hasFocus() && box->isEnabled() )
            {
                bool bHasDefault = false;
                for ( auto* pb : box->window()->findChildren<QPushButton*>() )
                {
                    bHasDefault = bHasDefault || ( pb->isDefault() && pb->isVisible() );
                }
                if ( !bHasDefault )
                {
                    box->click();
                    return true;
                }
            }
            break;
        }

        default:
            break;
        }
        return QObject::eventFilter ( watched, event );
    }

private:
    // Qt opens a popup menu with nothing selected: the screen reader says nothing. Select the first
    // item, as in Windows, after popup() has finished positioning and possibly selecting one itself.
    static void menuShown ( QMenu* menu )
    {
        QTimer::singleShot ( 0, menu, [menu] {
            if ( !menu->isVisible() || menu->activeAction() )
            {
                return;
            }
            for ( QAction* action : menu->actions() )
            {
                if ( !action->isSeparator() && action->isVisible() && action->isEnabled() )
                {
                    menu->setActiveAction ( action );
                    break;
                }
            }
        } );
    }

    // Left arrow or Escape closes a submenu: the focus is back on its item in the parent menu, but
    // Qt sends no focus event (the parent's active item did not change); say it
    static void submenuClosed ( QMenu* menu )
    {
        for ( QObject* object : menu->menuAction()->associatedObjects() )
        {
            auto* parentMenu = qobject_cast<QMenu*> ( object );
            if ( !parentMenu || ( parentMenu == menu ) || !parentMenu->isVisible() )
            {
                continue;
            }
            if ( QAction* item = parentMenu->activeAction(); item && ( item == menu->menuAction() ) )
            {
                Access::announce ( parentMenu, QCoreApplication::translate ( "Access", "%1, submenu" ).arg ( Access::cleanLabel ( item->text() ) ) );
            }
            break;
        }
    }
};

class DigitKeyFilter final : public QObject
{
public:
    DigitKeyFilter ( QWidget* window, const Qt::KeyboardModifiers modifiers, std::function<bool ( int digit )> handler ) :
        QObject ( window ),
        m_window ( window ),
        m_modifiers ( modifiers ),
        m_handler ( std::move ( handler ) )
    {}

    bool eventFilter ( QObject* watched, QEvent* event ) override
    {
        if ( event->type() != QEvent::KeyPress )
        {
            return QObject::eventFilter ( watched, event );
        }

        const auto*                 key       = static_cast<QKeyEvent*> ( event );
        const Qt::KeyboardModifiers modifiers = key->modifiers() & ~Qt::KeypadModifier;
        QWidget*                    focus     = QApplication::focusWidget();

        // the physical key: Windows virtual-key codes '0'..'9' whatever the layout (AZERTY included),
        // numeric keypad digits too
        quint32 vk = key->nativeVirtualKey();
        if ( vk >= 0x60 && vk <= 0x69 )
        {
            vk = '0' + ( vk - 0x60 );
        }
        if ( vk == 0 && key->key() >= Qt::Key_0 && key->key() <= Qt::Key_9 )
        {
            vk = '0' + ( key->key() - Qt::Key_0 ); // no native key (synthetic event)
        }
        if ( ( modifiers != m_modifiers ) || ( vk < '0' ) || ( vk > '9' ) || key->isAutoRepeat() || !focus || ( focus->window() != m_window ) ||
             ( watched != focus ) )
        {
            return QObject::eventFilter ( watched, event );
        }

        // a digit or AltGr+digit typing a character ("&", "@"...) in a text field: leave the character
        // alone (Ctrl+digit types nothing: it stays a shortcut there too)
        const bool textField = qobject_cast<QLineEdit*> ( focus ) || qobject_cast<QTextEdit*> ( focus ) ||
                               qobject_cast<QPlainTextEdit*> ( focus ) || qobject_cast<QAbstractSpinBox*> ( focus );
        const bool typesText = ( modifiers == Qt::NoModifier ) || ( modifiers & Qt::AltModifier );
        if ( textField && typesText && !key->text().isEmpty() && key->text().at ( 0 ).isPrint() )
        {
            return QObject::eventFilter ( watched, event );
        }

        return m_handler ( static_cast<int> ( vk - '0' ) );
    }

private:
    QPointer<QWidget>               m_window;
    Qt::KeyboardModifiers           m_modifiers;
    std::function<bool ( int digit )> m_handler;
};

class ListEdgeFilter final : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter ( QObject* watched, QEvent* event ) override
    {
        if ( event->type() == QEvent::KeyPress )
        {
            if ( Access::announceListEdge ( static_cast<QAbstractItemView*> ( watched ), static_cast<QKeyEvent*> ( event ) ) )
            {
                return true;
            }
        }
        return QObject::eventFilter ( watched, event );
    }
};

class SliderKeyFilter final : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter ( QObject* watched, QEvent* event ) override
    {
        if ( event->type() != QEvent::KeyPress && event->type() != QEvent::ShortcutOverride )
        {
            return QObject::eventFilter ( watched, event );
        }

        auto*                       slider    = static_cast<QAbstractSlider*> ( watched );
        auto*                       key       = static_cast<QKeyEvent*> ( event );
        const Qt::KeyboardModifiers modifiers = key->modifiers() & ~Qt::KeypadModifier;
        const bool                  upDown    = ( key->key() == Qt::Key_Up ) || ( key->key() == Qt::Key_Down );
        const bool                  plainKey =
            ( modifiers == Qt::NoModifier ) && ( upDown || key->key() == Qt::Key_Left || key->key() == Qt::Key_Right || key->key() == Qt::Key_PageUp ||
                                                 key->key() == Qt::Key_PageDown || key->key() == Qt::Key_Home || key->key() == Qt::Key_End );
        const bool valueKey = upDown && ( ( modifiers == Qt::AltModifier ) || ( modifiers == ( Qt::AltModifier | Qt::ShiftModifier ) ) );

        if ( event->type() == QEvent::ShortcutOverride )
        {
            // Alt+arrows belong to the slider, not to a menu or window shortcut
            if ( valueKey )
            {
                event->accept();
                return true;
            }
            return QObject::eventFilter ( watched, event );
        }

        if ( plainKey )
        {
            Access::announce ( slider, Access::sliderHelp() );
            return true;
        }
        if ( !valueKey )
        {
            return QObject::eventFilter ( watched, event );
        }

        const int  range = slider->maximum() - slider->minimum();
        const int  step  = ( modifiers & Qt::ShiftModifier ) ? 1 : std::max ( 1, ( range * 5 + 50 ) / 100 );
        const bool up    = key->key() == Qt::Key_Up;
        const int  value = std::clamp ( slider->value() + ( up ? step : -step ), slider->minimum(), slider->maximum() );
        if ( value == slider->value() )
        {
            Access::announce ( slider, up ? QCoreApplication::translate ( "Access", "Maximum" ) : QCoreApplication::translate ( "Access", "Minimum" ) );
            return true;
        }
        slider->setValue ( value ); // the screen reader says the new value (value change event)
        return true;
    }
};

class ShortcutHelpDialog final : public QDialog
{
public:
    ShortcutHelpDialog ( QWidget* parent, const QString& title, const QStringList& rows ) : QDialog ( parent )
    {
        setWindowTitle ( title );
        auto* layout = new QVBoxLayout ( this );
        list         = new QListWidget ( this );
        list->setAccessibleName ( title ); // short and fixed: the braille line keeps the row
        list->addItems ( rows );
        list->setCurrentRow ( 0 );
        Access::installListEdges ( list );
        auto* buttons = new QDialogButtonBox ( QDialogButtonBox::Close, this );
        connect ( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
        layout->addWidget ( list );
        layout->addWidget ( buttons );
        resize ( 520, 420 );
    }

    QListWidget* list;
};
} // namespace

void Access::announce ( QObject* source, const QString& text, const bool assertive )
{
    if ( !source || text.isEmpty() || !QAccessible::isActive() )
    {
        return;
    }

    QAccessibleAnnouncementEvent event ( source, text );
    event.setPoliteness ( assertive ? QAccessible::AnnouncementPoliteness::Assertive : QAccessible::AnnouncementPoliteness::Polite );
    QAccessible::updateAccessibility ( &event );
}

void Access::installApplicationFixes ( QObject* owner )
{
    QAccessible::installFactory ( accessibleFactory );
    qApp->installEventFilter ( new ApplicationFilter ( owner ) );
    for ( QWidget* widget : QApplication::allWidgets() ) // widgets already created
    {
        plainTooltipDescription ( widget );
    }
}

void Access::installDigitKeys ( QWidget* window, const Qt::KeyboardModifiers modifiers, std::function<bool ( int digit )> handler )
{
    qApp->installEventFilter ( new DigitKeyFilter ( window, modifiers, std::move ( handler ) ) );
}

QString Access::cleanLabel ( const QString& text )
{
    QString result = Qt::mightBeRichText ( text ) ? QTextDocumentFragment::fromHtml ( text ).toPlainText() : text;
    result.replace ( "&&", "\x01" ).remove ( '&' ).replace ( '\x01', '&' );
    result = result.simplified();
    if ( result.endsWith ( "..." ) ) // screen readers say "dot dot dot"
    {
        result.chop ( 3 );
    }
    if ( result.endsWith ( QChar ( 0x2026 ) ) )
    {
        result.chop ( 1 );
    }
    while ( !result.isEmpty() && ( result.endsWith ( ':' ) || result.back().isSpace() ) )
    {
        result.chop ( 1 );
    }
    return result;
}

void Access::labelControls ( QWidget* root )
{
    if ( !root )
    {
        return;
    }

    for ( auto* scroll : root->findChildren<QScrollArea*>() )
    {
        scroll->setFocusPolicy ( Qt::NoFocus );
    }
    // the scroll arrows of a tab bar are for the mouse (Ctrl+Tab changes the tab)
    for ( auto* tabBar : root->findChildren<QTabBar*>() )
    {
        for ( auto* arrow : tabBar->findChildren<QToolButton*>() )
        {
            arrow->setFocusPolicy ( Qt::NoFocus );
        }
    }

    // A group box is named after its own title (Qt names a nested one after its parent's title);
    // this matters for checkable group boxes, which are Tab stops
    for ( auto* group : root->findChildren<QGroupBox*>() )
    {
        if ( group->accessibleName().isEmpty() && !group->title().isEmpty() )
        {
            group->setAccessibleName ( cleanLabel ( group->title() ) );
        }
    }

    // Icon-only buttons are named after their tooltip
    for ( auto* button : root->findChildren<QAbstractButton*> () )
    {
        if ( button->property ( "accessTooltipName" ).toBool() || !button->accessibleName().isEmpty() || button->toolTip().isEmpty() )
        {
            continue;
        }
        const QString text = cleanLabel ( button->text() );
        if ( !text.isEmpty() && text.at ( 0 ).isLetterOrNumber() )
        {
            continue; // a real text
        }
        button->setAccessibleName ( cleanLabel ( button->toolTip() ) );
        button->setProperty ( "accessTooltipName", true );
        button->installEventFilter ( new TooltipNameFilter ( button ) );
    }

    // Input controls without a name: the label on their left (or above)
    for ( QWidget* control : root->findChildren<QWidget*>() )
    {
        if ( ( control->window() != root->window() ) || !isInput ( control ) || !control->accessibleName().isEmpty() )
        {
            continue;
        }
        QWidget* label = findLabel ( control );
        if ( !label )
        {
            continue;
        }
        control->setAccessibleName ( labelText ( label ) );
        if ( auto* qlabel = qobject_cast<QLabel*> ( label ); qlabel && !qlabel->buddy() )
        {
            qlabel->setBuddy ( control );
        }
    }

    // Still nameless, a text alone on a tab page ("About", "Translation"...): the tab's name
    for ( auto* text : root->findChildren<QTextEdit*>() )
    {
        if ( !text->accessibleName().isEmpty() )
        {
            continue;
        }
        for ( QWidget* page = text->parentWidget(); page && page != root; page = page->parentWidget() )
        {
            auto* stack = qobject_cast<QStackedWidget*> ( page->parentWidget() );
            auto* tabs  = stack ? qobject_cast<QTabWidget*> ( stack->parentWidget() ) : nullptr;
            if ( tabs && tabs->indexOf ( page ) >= 0 )
            {
                text->setAccessibleName ( cleanLabel ( tabs->tabText ( tabs->indexOf ( page ) ) ) );
                break;
            }
        }
    }
}

void Access::installListEdges ( QAbstractItemView* view ) { view->installEventFilter ( new ListEdgeFilter ( view ) ); }

bool Access::announceListEdge ( QAbstractItemView* view, const QKeyEvent* event )
{
    if ( !view || !view->model() || ( ( event->modifiers() & ~Qt::KeypadModifier ) != Qt::NoModifier ) )
    {
        return false;
    }

    const bool up   = ( event->key() == Qt::Key_Up );
    const bool down = ( event->key() == Qt::Key_Down );
    if ( !up && !down )
    {
        return false;
    }

    const QModelIndex current = view->currentIndex();
    if ( !current.isValid() )
    {
        return false;
    }

    bool atEdge = false;
    if ( const auto* tree = qobject_cast<const QTreeView*> ( view ) )
    {
        atEdge = !( up ? tree->indexAbove ( current ) : tree->indexBelow ( current ) ).isValid();
    }
    else
    {
        const int rows = view->model()->rowCount ( current.parent() );
        int       row  = current.row();
        // hidden rows of a list widget do not count
        const auto* list = qobject_cast<const QListWidget*> ( view );
        do
        {
            row += up ? -1 : 1;
        } while ( list && row >= 0 && row < rows && list->item ( row )->isHidden() );
        atEdge = ( row < 0 ) || ( row >= rows );
    }

    if ( atEdge )
    {
        QString text = current.data ( Qt::AccessibleTextRole ).toString();
        if ( text.isEmpty() )
        {
            text = current.data ( Qt::DisplayRole ).toString();
        }
        announce ( view, ( up ? QCoreApplication::translate ( "Access", "First" ) : QCoreApplication::translate ( "Access", "Last" ) ) + ", " + text );
        return true;
    }
    return false;
}

QString Access::sliderHelp() { return QCoreApplication::translate ( "Access", "Alt+Up or Down to change the value, Alt+Shift for a small step" ); }

void Access::installSliderKeys ( QAbstractSlider* slider, std::function<QString ( int value )> valueText )
{
    sliders().insert ( slider, { std::move ( valueText ) } );
    QObject::connect ( slider, &QObject::destroyed, qApp, [slider] { sliders().remove ( slider ); } );
    dropCachedInterface ( slider );
    slider->installEventFilter ( new SliderKeyFilter ( slider ) );
}

void Access::presentAsPane ( QWidget* widget )
{
    panes().insert ( widget );
    QObject::connect ( widget, &QObject::destroyed, qApp, [widget] { panes().remove ( widget ); } );
    dropCachedInterface ( widget );
}

void Access::ignoreValue ( QWidget* widget )
{
    ignoredValues().insert ( widget );
    QObject::connect ( widget, &QObject::destroyed, qApp, [widget] { ignoredValues().remove ( widget ); } );
    dropCachedInterface ( widget );
    for ( QWidget* child : widget->findChildren<QWidget*>() )
    {
        dropCachedInterface ( child );
    }
}

void Access::keepTreeRowTexts ( QTreeWidget* tree, std::function<QString ( const QModelIndex& row )> rowText )
{
    QAbstractItemModel* model    = tree->model();
    auto                updating = std::make_shared<bool> ( false );

    // the screen reader text of one row, on every column; never the current row (see header)
    const auto updateRow = [tree, model, rowText, updating] ( const QModelIndex& index, const bool evenCurrent ) {
        const QModelIndex row = index.siblingAtColumn ( 0 );
        // the current row keeps its text, unless it has none yet (a row is inserted empty, then filled)
        const bool bIsCurrent = tree->currentIndex().isValid() && tree->currentIndex().siblingAtColumn ( 0 ) == row;
        if ( !row.isValid() || ( !evenCurrent && bIsCurrent && !row.data ( Qt::AccessibleTextRole ).toString().isEmpty() ) )
        {
            return;
        }
        const QString text = rowText ( row );
        *updating          = true;
        for ( int column = 0; column < model->columnCount ( row.parent() ); ++column )
        {
            const QModelIndex cell = row.siblingAtColumn ( column );
            if ( cell.data ( Qt::AccessibleTextRole ).toString() != text )
            {
                model->setData ( cell, text, Qt::AccessibleTextRole );
            }
        }
        *updating = false;
    };

    const auto updateAll = [model, updateRow] {
        const std::function<void ( const QModelIndex& )> walk = [&] ( const QModelIndex& parent ) {
            for ( int row = 0; row < model->rowCount ( parent ); ++row )
            {
                const QModelIndex index = model->index ( row, 0, parent );
                updateRow ( index, false );
                walk ( index );
            }
        };
        walk ( {} );
    };

    QObject::connect ( model, &QAbstractItemModel::dataChanged, tree, [model, updateRow, updating] ( const QModelIndex& topLeft, const QModelIndex& bottomRight ) {
        if ( *updating )
        {
            return;
        }
        for ( int row = topLeft.row(); row <= bottomRight.row(); ++row )
        {
            updateRow ( model->index ( row, 0, topLeft.parent() ), false );
        }
    } );
    QObject::connect ( model, &QAbstractItemModel::rowsInserted, tree, [model, updateRow] ( const QModelIndex& parent, int first, int last ) {
        for ( int row = first; row <= last; ++row )
        {
            updateRow ( model->index ( row, 0, parent ), false );
        }
    } );
    QObject::connect ( model, &QAbstractItemModel::modelReset, tree, updateAll );
    QObject::connect ( tree, &QTreeWidget::currentItemChanged, tree, [tree, updateRow] ( QTreeWidgetItem*, QTreeWidgetItem* previous ) {
        if ( previous )
        {
            // the row left behind catches up with the values that changed while it was current
            QTimer::singleShot ( 0, tree, [tree, updateRow, previous = QPersistentModelIndex ( tree->indexFromItem ( previous ) )] {
                if ( previous.isValid() )
                {
                    updateRow ( previous, false );
                }
            } );
        }
    } );
    updateAll();
}

namespace
{
struct CellReading
{
    std::function<QStringList ( const QTreeWidgetItem* )> cells;
    int                                                   column = -1; // -1: the whole row
    int                                                   depth  = 0;  // kind of row the column belongs to (server 0, musician 1...)
};

int depthOf ( const QTreeWidgetItem* item )
{
    int depth = 0;
    for ( const QTreeWidgetItem* p = item->parent(); p; p = p->parent() )
    {
        ++depth;
    }
    return depth;
}

QHash<const QTreeWidget*, CellReading>& cellReadings()
{
    static QHash<const QTreeWidget*, CellReading> registry;
    return registry;
}

void notifyCurrentRowName ( QTreeWidget* tree ) { Access::notifyCurrentItemTextChanged ( tree ); }

class CellReadingFilter final : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter ( QObject* watched, QEvent* event ) override
    {
        if ( event->type() != QEvent::KeyPress )
        {
            return QObject::eventFilter ( watched, event );
        }
        auto*      tree = static_cast<QTreeWidget*> ( watched );
        const auto* key = static_cast<QKeyEvent*> ( event );
        const int  code = key->key();
        auto       it   = cellReadings().find ( tree );
        if ( it == cellReadings().end() || ( key->modifiers() & ~Qt::KeypadModifier ) != Qt::NoModifier ||
             !( code == Qt::Key_Left || code == Qt::Key_Right ) )
        {
            return QObject::eventFilter ( watched, event );
        }

        QTreeWidgetItem* item = tree->currentItem();
        if ( !item )
        {
            return true;
        }
        const QStringList cells  = it->cells ( item );
        const int         count  = static_cast<int> ( cells.size() );
        // another kind of row (a musician under a server) has other columns: start from its whole row
        int column = ( depthOf ( item ) == it->depth ) ? std::min ( it->column, count - 1 ) : -1;
        it->depth  = depthOf ( item );

        switch ( code )
        {
        case Qt::Key_Left:
            if ( column < 0 )
            {
                // never a silent edge
                Access::announce ( tree, QCoreApplication::translate ( "Access", "First" ) + ", " + cells.join ( ", " ) );
                return true;
            }
            --column;
            break;
        default: // Right
            if ( column + 1 >= count )
            {
                Access::announce ( tree, QCoreApplication::translate ( "Access", "Last" ) + ", " + ( count > 0 ? cells.last() : QString() ) );
                return true;
            }
            ++column;
            break;
        }
        it->column = column;
        notifyCurrentRowName ( tree );
        return true;
    }
};
} // namespace

namespace
{
// the accessible object of the current row that the screen reader last received with the focus
QHash<const QAbstractItemView*, QAccessible::Id>& focusedItemIds()
{
    static QHash<const QAbstractItemView*, QAccessible::Id> registry;
    return registry;
}

QAccessibleInterface* currentItemInterface ( QAbstractItemView* view )
{
    QAccessibleInterface* iface = QAccessible::queryAccessibleInterface ( view );
    QAccessibleInterface* item  = iface ? iface->focusChild() : nullptr;
    return ( item && item != iface && item->isValid() ) ? item : nullptr;
}

void rememberFocusedItem ( QAbstractItemView* view )
{
    if ( !QAccessible::isActive() || !view->hasFocus() )
    {
        return;
    }
    if ( QAccessibleInterface* item = currentItemInterface ( view ) )
    {
        focusedItemIds()[view] = QAccessible::uniqueId ( item );
    }
}

class ItemFocusFilter final : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter ( QObject* watched, QEvent* event ) override
    {
        if ( event->type() == QEvent::FocusIn )
        {
            // after Qt has told the screen reader about the row
            auto* view = static_cast<QAbstractItemView*> ( watched );
            QTimer::singleShot ( 0, view, [view] { rememberFocusedItem ( view ); } );
        }
        return QObject::eventFilter ( watched, event );
    }
};
} // namespace

void Access::trackItemFocus ( QAbstractItemView* view )
{
    view->installEventFilter ( new ItemFocusFilter ( view ) );
    QObject::connect ( view, &QObject::destroyed, qApp, [view] { focusedItemIds().remove ( view ); } );
    // the view's own slot told the screen reader first (it was connected first)
    QObject::connect ( view->selectionModel(), &QItemSelectionModel::currentChanged, view, [view] { rememberFocusedItem ( view ); } );
}

void Access::notifyCurrentItemTextChanged ( QAbstractItemView* view )
{
    if ( !QAccessible::isActive() || !view->hasFocus() )
    {
        return;
    }
    QAccessibleInterface* iface = QAccessible::queryAccessibleInterface ( view );
    QAccessibleInterface* item  = currentItemInterface ( view );
    if ( !iface || !item )
    {
        return;
    }

    const QAccessible::Id id = QAccessible::uniqueId ( item );
    if ( focusedItemIds().value ( view ) == id )
    {
        QAccessibleEvent event ( view, QAccessible::NameChanged );
        event.setChild ( iface->indexOfChild ( item ) );
        QAccessible::updateAccessibility ( &event );
        return;
    }

    // Qt made a new object for the row since the screen reader got the focus: give it the focus
    QAccessibleEvent event ( view, QAccessible::Focus );
    event.setChild ( iface->indexOfChild ( item ) );
    QAccessible::updateAccessibility ( &event );
    focusedItemIds()[view] = id;
}

void Access::installCellReading ( QTreeWidget* tree, std::function<QStringList ( const QTreeWidgetItem* item )> cells )
{
    trackItemFocus ( tree );
    cellReadings().insert ( tree, { std::move ( cells ), -1 } );
    QObject::connect ( tree, &QObject::destroyed, qApp, [tree] { cellReadings().remove ( tree ); } );
    tree->installEventFilter ( new CellReadingFilter ( tree ) );
}

QVariant Access::cellReadingText ( const QTreeWidgetItem* item )
{
    const QTreeWidget* tree = item ? item->treeWidget() : nullptr;
    const auto         it   = cellReadings().constFind ( tree );
    if ( it == cellReadings().cend() )
    {
        return {};
    }
    const QStringList cells = it->cells ( item );
    if ( cells.isEmpty() )
    {
        return {};
    }
    if ( it->column >= 0 && item == tree->currentItem() && depthOf ( item ) == it->depth )
    {
        return cells[std::min ( it->column, static_cast<int> ( cells.size() ) - 1 )];
    }
    return cells.join ( ", " );
}

void Access::focusAndSpeak ( QWidget* widget )
{
    if ( !widget )
    {
        return;
    }
    if ( auto* itemView = qobject_cast<QAbstractItemView*> ( widget ); itemView && !itemView->currentIndex().isValid() && itemView->model() &&
                                                                       ( itemView->model()->rowCount ( itemView->rootIndex() ) > 0 ) )
    {
        itemView->setCurrentIndex ( itemView->model()->index ( 0, 0, itemView->rootIndex() ) );
    }
    if ( !widget->hasFocus() )
    {
        widget->setFocus ( Qt::ShortcutFocusReason );
        return;
    }
    if ( !QAccessible::isActive() )
    {
        return;
    }

    // Already focused: no focus change, so nothing is said, and screen readers ignore a focus event for
    // the object they already have. Say what they say on arrival: the name, then the current item
    // (or the value of a field).
    QAccessibleInterface* iface = QAccessible::queryAccessibleInterface ( widget );
    if ( !iface )
    {
        return;
    }
    QStringList parts { iface->text ( QAccessible::Name ) };
    if ( qobject_cast<QAbstractItemView*> ( widget ) )
    {
        if ( QAccessibleInterface* item = iface->focusChild(); item && ( item != iface ) )
        {
            parts.append ( item->text ( QAccessible::Name ) );
        }
    }
    else
    {
        parts.append ( iface->text ( QAccessible::Value ) );
    }
    parts.removeAll ( QString() );
    announce ( widget, parts.join ( ", " ) );
}

void Access::goTo ( QWidget* target )
{
    if ( !target )
    {
        return;
    }
    auto* view = qobject_cast<QAbstractItemView*> ( target );
    focusAndSpeak ( target );

    // an empty list must not be silent
    if ( view && view->model() && ( view->model()->rowCount ( view->rootIndex() ) == 0 ) )
    {
        announce ( view, QCoreApplication::translate ( "Access", "Empty" ) );
    }
}

void Access::showShortcutHelp ( QWidget* parent, const QString& title, const QStringList& rows )
{
    ShortcutHelpDialog dialog ( parent, title, rows );
    // said once, before the list takes the focus (the list keeps a short, fixed name)
    announce ( parent, QCoreApplication::translate ( "Access", "%1 items, Escape to close" ).arg ( rows.size() ) );
    dialog.exec();
}
