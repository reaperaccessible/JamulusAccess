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

#include "mixerlist.h"

#include <QAccessible>
#include <QCheckBox>
#include <QDial>
#include <QKeyEvent>
#include <QSlider>
#include "../audiomixerboard.h"
#include "accessibility.h"

namespace
{
const int ROLE_CHANNEL     = Qt::UserRole;     // channel index of the fader
const int ROLE_ORDER       = Qt::UserRole + 1; // position on screen
const int ROLE_REMOTE_MUTE = Qt::UserRole + 2; // last known "has muted you"

// Rows sort by their position on screen, so reordering keeps the current row (no focus event)
class CMixerItem : public QListWidgetItem
{
public:
    explicit CMixerItem ( int iChannel )
    {
        setData ( ROLE_CHANNEL, iChannel );
        setData ( ROLE_ORDER, 0 );
        setData ( ROLE_REMOTE_MUTE, false );
    }

    bool operator<( const QListWidgetItem& other ) const override { return data ( ROLE_ORDER ).toInt() < other.data ( ROLE_ORDER ).toInt(); }
};
} // namespace

CMixerAccessList::CMixerAccessList ( CVector<CChannelFader*>& faders, QWidget* parent ) :
    QListWidget ( parent ),
    vecpChanFader ( faders ),
    eParam ( P_VOLUME ),
    eMode ( M_FULL )
{
    setAccessibleName ( tr ( "Mixer" ) ); // short and fixed: the braille line keeps the row
    setSelectionMode ( QAbstractItemView::SingleSelection );
    setEditTriggers ( QAbstractItemView::NoEditTriggers );
    setSortingEnabled ( false );
    setUniformItemSizes ( true );

    for ( size_t i = 0; i < vecpChanFader.Size(); i++ )
    {
        CChannelFader* fader    = vecpChanFader[i];
        const int      iChannel = static_cast<int> ( i );
        connect ( fader->GetFaderWidget(), &QSlider::valueChanged, this, [this, iChannel] { Refresh ( iChannel ); } );
        connect ( fader->GetPanWidget(), &QDial::valueChanged, this, [this, iChannel] { Refresh ( iChannel ); } );
        connect ( fader->GetMuteBox(), &QCheckBox::toggled, this, [this, iChannel] { Refresh ( iChannel ); } );
        connect ( fader->GetSoloBox(), &QCheckBox::toggled, this, [this, iChannel] { Refresh ( iChannel ); } );
        connect ( fader, &CChannelFader::accessStateChanged, this, [this, iChannel] { Refresh ( iChannel ); } );
    }

    // the row left behind says everything again; the new current row already does
    connect ( this, &QListWidget::currentItemChanged, this, [this] ( QListWidgetItem*, QListWidgetItem* previous ) {
        eMode = M_FULL;
        strPrefix.clear();
        strBase.clear();
        if ( previous && row ( previous ) >= 0 )
        {
            UpdateItem ( previous );
        }
        if ( CChannelFader* fader = FaderOf ( currentItem() ) )
        {
            strBase = RowText ( fader, M_FULL );
        }
    } );

    // levels change all the time: the rows that are not current follow them quietly, the current
    // row says its level when asked (Enter)
    TimerLevels.setInterval ( 1000 );
    connect ( &TimerLevels, &QTimer::timeout, this, [this] {
        if ( eParam != P_LEVEL )
        {
            return;
        }
        for ( int i = 0; i < count(); i++ )
        {
            if ( item ( i ) != currentItem() )
            {
                UpdateItem ( item ( i ) );
            }
        }
    } );
    TimerLevels.start();
}

QStringList CMixerAccessList::ShortcutRows()
{
    return { tr ( "Up and Down: musicians" ),
             tr ( "Left and Right: Volume, Pan, Mute, Solo, Group, Level, Profile" ),
             tr ( "Alt+Up and Alt+Down: change the value" ),
             tr ( "Alt+Shift+Up and Alt+Shift+Down: small step" ),
             tr ( "Enter or Space: switch Mute or Solo, read the Level again" ),
             tr ( "M: mute the musician" ),
             tr ( "S: solo the musician" ) };
}

QListWidgetItem* CMixerAccessList::ItemFor ( int iChannel ) const
{
    for ( int i = 0; i < count(); i++ )
    {
        if ( item ( i )->data ( ROLE_CHANNEL ).toInt() == iChannel )
        {
            return item ( i );
        }
    }
    return nullptr;
}

CChannelFader* CMixerAccessList::FaderOf ( const QListWidgetItem* item ) const
{
    if ( !item )
    {
        return nullptr;
    }
    const int iChannel = item->data ( ROLE_CHANNEL ).toInt();
    return ( iChannel >= 0 && iChannel < static_cast<int> ( vecpChanFader.Size() ) ) ? vecpChanFader[iChannel] : nullptr;
}

void CMixerAccessList::Sync ( const QList<int>& visibleChannels )
{
    const bool bWasEmpty = ( count() == 0 );

    // musicians who left (not when everybody goes: that is a disconnection)
    for ( int i = count() - 1; i >= 0; i-- )
    {
        QListWidgetItem* it = item ( i );
        if ( visibleChannels.contains ( it->data ( ROLE_CHANNEL ).toInt() ) )
        {
            continue;
        }
        if ( !visibleChannels.isEmpty() )
        {
            // the fader is already reset: the name is the one shown on the row
            const QString name = it->data ( Qt::UserRole + 3 ).toString();
            Access::announce ( this, tr ( "%1 left" ).arg ( name ) );
        }
        delete takeItem ( i );
    }

    // musicians who joined, and the order on screen
    for ( int i = 0; i < visibleChannels.size(); i++ )
    {
        QListWidgetItem* it = ItemFor ( visibleChannels[i] );
        if ( !it )
        {
            it = new CMixerItem ( visibleChannels[i] );
            addItem ( it );
            if ( !bWasEmpty )
            {
                Access::announce ( this, tr ( "%1 joined" ).arg ( Identity ( FaderOf ( it ) ) ) );
            }
        }
        it->setData ( ROLE_ORDER, i );
        UpdateItem ( it );
    }
    sortItems();

    if ( !currentItem() && count() > 0 )
    {
        setCurrentRow ( 0 );
    }
}

void CMixerAccessList::Refresh ( int iChannel )
{
    QListWidgetItem* it    = ItemFor ( iChannel );
    CChannelFader*   fader = FaderOf ( it );
    if ( !it || !fader )
    {
        return;
    }

    // another musician muted you in their mix, or stopped: say it once (your own row lights up when
    // you mute yourself in your mix, with Solo for example: nothing to say)
    const bool bRemoteMuted = fader->IsRemoteMuted();
    if ( bRemoteMuted != it->data ( ROLE_REMOTE_MUTE ).toBool() )
    {
        it->setData ( ROLE_REMOTE_MUTE, bRemoteMuted );
        if ( !fader->GetIsMyOwnFader() )
        {
            Access::announce ( this, ( bRemoteMuted ? tr ( "%1 muted you" ) : tr ( "%1 unmuted you" ) ).arg ( Identity ( fader ) ) );
        }
    }

    if ( it == currentItem() )
    {
        // a value of the current row changed (by us, a MIDI controller or a group): say the value.
        // When nothing it shows changed (another setting, the profile sent again), keep the row as it
        // is, "Maximum, " included, and say nothing.
        const EMode mode = eMode == M_FULL ? M_FULL : M_VALUE;
        if ( mode == eMode && RowText ( fader, mode ) == strBase )
        {
            return;
        }
        SpeakCurrent ( mode, QString(), false );
    }
    else
    {
        UpdateItem ( it );
    }
}

QList<CMixerAccessList::EParam> CMixerAccessList::Params ( CChannelFader* fader ) const
{
    QList<EParam> params { P_VOLUME };
    if ( fader && fader->IsPanShown() )
    {
        params << P_PAN;
    }
    params << P_MUTE << P_SOLO << P_GROUP << P_LEVEL << P_PROFILE;
    return params;
}

CMixerAccessList::EParam CMixerAccessList::CurrentParam ( CChannelFader* fader ) const
{
    return Params ( fader ).contains ( eParam ) ? eParam : P_VOLUME;
}

QString CMixerAccessList::Identity ( CChannelFader* fader ) const
{
    if ( !fader )
    {
        return {};
    }
    QString name = fader->GetReceivedName().simplified();
    if ( name.isEmpty() )
    {
        name = tr ( "No name" );
    }
    return fader->GetIsMyOwnFader() ? tr ( "%1 (you)" ).arg ( name ) : name;
}

QString CMixerAccessList::ParamName ( EParam param ) const
{
    switch ( param )
    {
    case P_VOLUME:
        return tr ( "Volume" );
    case P_PAN:
        return tr ( "Pan" );
    case P_MUTE:
        return tr ( "Mute" );
    case P_SOLO:
        return tr ( "Solo" );
    case P_GROUP:
        return tr ( "Group" );
    case P_LEVEL:
        return tr ( "Level" );
    case P_PROFILE:
        return tr ( "Profile" );
    }
    return {};
}

QString CMixerAccessList::ValueText ( CChannelFader* fader, EParam param ) const
{
    switch ( param )
    {
    case P_VOLUME:
        return QString ( "%1 %" ).arg ( fader->GetFaderLevel() );

    case P_PAN:
    {
        const int iPan = fader->GetPanValue() - AUD_MIX_PAN_MAX / 2;
        if ( iPan == 0 )
        {
            return tr ( "Center" );
        }
        return iPan < 0 ? tr ( "Left %1" ).arg ( -iPan ) : tr ( "Right %1" ).arg ( iPan );
    }

    case P_MUTE:
        return fader->IsMute() ? tr ( "On" ) : tr ( "Off" );

    case P_SOLO:
        return fader->IsSolo() ? tr ( "On" ) : tr ( "Off" );

    case P_GROUP:
        return fader->GetGroupID() == INVALID_INDEX ? tr ( "None" ) : QString::number ( fader->GetGroupID() + 1 );

    case P_LEVEL:
    {
        // the meter shows NUM_STEPS_LED_BAR steps between LOW_BOUND_SIG_METER and 0 dB
        const double dLevel = fader->GetChannelLevel();
        if ( dLevel <= 0 )
        {
            return tr ( "Silent" );
        }
        const int idB = static_cast<int> ( std::lround ( dLevel * ( UPPER_BOUND_SIG_METER - LOW_BOUND_SIG_METER ) / NUM_STEPS_LED_BAR + LOW_BOUND_SIG_METER ) );
        return QString ( "%1 dB" ).arg ( idB );
    }

    case P_PROFILE:
    {
        const CChannelInfo& info = fader->GetChannelInfo();
        QStringList         parts;
        if ( !CInstPictures::IsNotUsedInstrument ( info.iInstrument ) )
        {
            parts << CInstPictures::GetName ( info.iInstrument );
        }
        switch ( info.eSkillLevel )
        {
        case SL_BEGINNER:
            parts << tr ( "Beginner" );
            break;
        case SL_INTERMEDIATE:
            parts << tr ( "Intermediate" );
            break;
        case SL_PROFESSIONAL:
            parts << tr ( "Expert" );
            break;
        default:
            break;
        }
        if ( !info.strCity.isEmpty() )
        {
            parts << info.strCity;
        }
        if ( info.eCountry != QLocale::AnyCountry )
        {
            parts << QLocale::countryToString ( info.eCountry );
        }
        if ( fader->IsRemoteMuted() && !fader->GetIsMyOwnFader() )
        {
            parts << tr ( "Muted you" );
        }
        return parts.isEmpty() ? tr ( "No profile" ) : parts.join ( ", " );
    }
    }
    return {};
}

QString CMixerAccessList::RowText ( CChannelFader* fader, EMode mode ) const
{
    const EParam  param = CurrentParam ( fader );
    const QString value = ValueText ( fader, param );
    switch ( mode )
    {
    case M_VALUE:
        return value;
    case M_PARAM:
        return ParamName ( param ) + ", " + value;
    default:
        return Identity ( fader ) + ", " + ParamName ( param ) + ", " + value;
    }
}

void CMixerAccessList::UpdateItem ( QListWidgetItem* item )
{
    CChannelFader* fader = FaderOf ( item );
    if ( !fader )
    {
        return;
    }
    item->setData ( Qt::UserRole + 3, Identity ( fader ) ); // for "left": the fader is reset by then
    if ( item == currentItem() && !strBase.isEmpty() )
    {
        return; // the current row says what SpeakCurrent() gave it
    }
    const QString text = RowText ( fader, M_FULL );
    if ( item->text() != text )
    {
        item->setText ( text );
    }
}

void CMixerAccessList::SpeakCurrent ( EMode mode, const QString& prefix, bool bSayIfSame )
{
    QListWidgetItem* it    = currentItem();
    CChannelFader*   fader = FaderOf ( it );
    if ( !fader )
    {
        return;
    }
    eMode     = mode;
    strPrefix = prefix;
    strBase   = RowText ( fader, mode );
    it->setData ( Qt::UserRole + 3, Identity ( fader ) );

    const QString text = prefix + strBase;
    if ( it->text() == text )
    {
        // the same text again (a second press on a bound): never a silent key
        if ( bSayIfSame && hasFocus() )
        {
            Access::announce ( this, text );
        }
        return;
    }
    it->setText ( text );

    // Qt does not tell screen readers that the text of a row changed: say it for the focused row,
    // so speech and braille both follow
    if ( hasFocus() && QAccessible::isActive() )
    {
        if ( QAccessibleInterface* iface = QAccessible::queryAccessibleInterface ( this ) )
        {
            if ( QAccessibleTableInterface* table = iface->tableInterface() )
            {
                if ( QAccessibleInterface* cell = table->cellAt ( row ( it ), 0 ) )
                {
                    QAccessibleEvent event ( this, QAccessible::NameChanged );
                    event.setChild ( iface->indexOfChild ( cell ) );
                    QAccessible::updateAccessibility ( &event );
                }
            }
        }
    }
}

void CMixerAccessList::focusOutEvent ( QFocusEvent* event )
{
    // coming back, the current row says everything again (no "First, " or value left over)
    eMode = M_FULL;
    strPrefix.clear();
    strBase.clear();
    if ( currentItem() )
    {
        UpdateItem ( currentItem() );
        if ( CChannelFader* fader = FaderOf ( currentItem() ) )
        {
            strBase = RowText ( fader, M_FULL );
        }
    }
    QListWidget::focusOutEvent ( event );
}

bool CMixerAccessList::event ( QEvent* event )
{
    if ( event->type() == QEvent::ShortcutOverride )
    {
        // Alt+arrows and the letters M and S belong to the mixer, not to a menu or a window shortcut
        auto*                       key       = static_cast<QKeyEvent*> ( event );
        const Qt::KeyboardModifiers modifiers = key->modifiers() & ~Qt::KeypadModifier;
        const bool                  upDown    = key->key() == Qt::Key_Up || key->key() == Qt::Key_Down;
        if ( ( upDown && ( modifiers == Qt::AltModifier || modifiers == ( Qt::AltModifier | Qt::ShiftModifier ) ) ) ||
             ( modifiers == Qt::NoModifier && ( key->key() == Qt::Key_M || key->key() == Qt::Key_S ) ) )
        {
            event->accept();
            return true;
        }
    }
    return QListWidget::event ( event );
}

void CMixerAccessList::keyPressEvent ( QKeyEvent* event )
{
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    CChannelFader*              fader     = FaderOf ( currentItem() );
    const int                   key       = event->key();

    if ( !fader )
    {
        QListWidget::keyPressEvent ( event );
        return;
    }

    if ( modifiers == Qt::NoModifier )
    {
        switch ( key )
        {
        case Qt::Key_Up:
        case Qt::Key_Down:
        {
            const int iRow = currentRow() + ( key == Qt::Key_Up ? -1 : 1 );
            if ( iRow < 0 || iRow >= count() )
            {
                // never a silent edge, even with one musician
                SpeakCurrent ( M_FULL, key == Qt::Key_Up ? tr ( "First, " ) : tr ( "Last, " ) );
                return;
            }
            break;
        }

        case Qt::Key_Left:
        case Qt::Key_Right:
        {
            // the settings of the musician, in a loop; every row follows the same setting
            const QList<EParam> params = Params ( fader );
            const int           iIndex = params.indexOf ( CurrentParam ( fader ) );
            const int           iNext  = ( iIndex + ( key == Qt::Key_Left ? -1 : 1 ) + params.size() ) % params.size();
            eParam                     = params[iNext];
            for ( int i = 0; i < count(); i++ )
            {
                if ( item ( i ) != currentItem() )
                {
                    UpdateItem ( item ( i ) );
                }
            }
            SpeakCurrent ( M_PARAM );
            return;
        }

        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            Activate();
            return;

        case Qt::Key_M:
            ToggleBox ( P_MUTE );
            return;

        case Qt::Key_S:
            ToggleBox ( P_SOLO );
            return;

        default:
            break;
        }
    }

    if ( ( key == Qt::Key_Up || key == Qt::Key_Down ) && ( modifiers == Qt::AltModifier || modifiers == ( Qt::AltModifier | Qt::ShiftModifier ) ) )
    {
        ChangeValue ( key == Qt::Key_Up ? 1 : -1, modifiers & Qt::ShiftModifier );
        return;
    }

    QListWidget::keyPressEvent ( event );
}

void CMixerAccessList::ChangeValue ( int iDirection, bool bSmallStep )
{
    CChannelFader* fader = FaderOf ( currentItem() );
    const EParam   param = CurrentParam ( fader );
    const int      iStep = bSmallStep ? 1 : 5;

    // the value is said by Refresh() when the fader moves; a bound is said here
    const auto bound = [this, iDirection] { SpeakCurrent ( M_VALUE, iDirection > 0 ? tr ( "Maximum, " ) : tr ( "Minimum, " ) ); };

    eMode = M_VALUE;
    strPrefix.clear();

    switch ( param )
    {
    case P_VOLUME:
    {
        QSlider*  slider = fader->GetFaderWidget();
        const int iValue = std::clamp ( slider->value() + iDirection * iStep, slider->minimum(), slider->maximum() );
        if ( iValue == slider->value() )
        {
            bound();
            return;
        }
        slider->setValue ( iValue );
        return;
    }

    case P_PAN:
    {
        QDial*    dial   = fader->GetPanWidget();
        const int iValue = std::clamp ( dial->value() + iDirection * iStep, dial->minimum(), dial->maximum() );
        if ( iValue == dial->value() )
        {
            bound();
            return;
        }
        dial->setValue ( iValue );
        return;
    }

    case P_MUTE:
    case P_SOLO:
    {
        QCheckBox* box = param == P_MUTE ? fader->GetMuteBox() : fader->GetSoloBox();
        if ( box->isChecked() == ( iDirection > 0 ) )
        {
            bound();
            return;
        }
        box->setChecked ( iDirection > 0 );
        return;
    }

    case P_GROUP:
    {
        // None, 1 ... 8
        const int iGroup = fader->GetGroupID() + iDirection;
        if ( iGroup < INVALID_INDEX || iGroup >= MAX_NUM_FADER_GROUPS )
        {
            bound();
            return;
        }
        fader->SetGroupID ( iGroup );
        return;
    }

    case P_LEVEL:
    case P_PROFILE:
        Activate();
        return;
    }
}

void CMixerAccessList::Activate()
{
    CChannelFader* fader = FaderOf ( currentItem() );
    switch ( CurrentParam ( fader ) )
    {
    case P_MUTE:
    case P_SOLO:
        eMode = M_VALUE;
        strPrefix.clear();
        ToggleBox ( CurrentParam ( fader ) );
        return;

    case P_LEVEL:
    case P_PROFILE:
        SpeakCurrent ( M_VALUE );
        return;

    default:
        Access::announce ( this, tr ( "Alt Up and Down adjust" ) );
        return;
    }
}

void CMixerAccessList::ToggleBox ( EParam param )
{
    CChannelFader* fader = FaderOf ( currentItem() );
    QCheckBox*     box   = param == P_MUTE ? fader->GetMuteBox() : fader->GetSoloBox();

    if ( CurrentParam ( fader ) == param )
    {
        // the row shows this setting: its new value is said by Refresh()
        eMode = M_VALUE;
        strPrefix.clear();
        box->setChecked ( !box->isChecked() );
        return;
    }

    // another setting is shown: the row text does not change, say it
    box->setChecked ( !box->isChecked() );
    Access::announce ( this, ParamName ( param ) + " " + ( box->isChecked() ? tr ( "on" ) : tr ( "off" ) ) );
}
