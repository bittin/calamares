/* === This file is part of Calamares - <https://github.com/calamares> ===
 *
 *   Copyright (c) 2017, Kyle Robbertze <kyle@aims.ac.za>
 *   Copyright 2017, 2020, Adriaan de Groot <groot@kde.org>
 *
 *   Calamares is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Calamares is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Calamares. If not, see <http://www.gnu.org/licenses/>.
 */

#include "PackageTreeItem.h"

#include "utils/Logger.h"

#if 0
PackageTreeItem::PackageTreeItem( const ItemData& data, PackageTreeItem* parent )
    : m_parentItem( parent )
    , m_data( data )
{
}
#endif

PackageTreeItem::PackageTreeItem( const QString& packageName, PackageTreeItem* parent )
    : m_parentItem( parent )
    , m_packageName( packageName )
    , m_selected( parent ? parent->isSelected() : Qt::Unchecked )
{
}

PackageTreeItem::PackageTreeItem( PackageTreeItem* parent )
    : PackageTreeItem( QString(), parent )
{
}

PackageTreeItem::PackageTreeItem::PackageTreeItem()
    : m_parentItem( nullptr )
    , m_name( QStringLiteral( "<root>" ) )
    , m_selected( Qt::Checked )

{
}

PackageTreeItem::~PackageTreeItem()
{
    qDeleteAll( m_childItems );
}

void
PackageTreeItem::appendChild( PackageTreeItem* child )
{
    m_childItems.append( child );
}

PackageTreeItem*
PackageTreeItem::child( int row )
{
    return m_childItems.value( row );
}

int
PackageTreeItem::childCount() const
{
    return m_childItems.count();
}

int
PackageTreeItem::row() const
{
    if ( m_parentItem )
    {
        return m_parentItem->m_childItems.indexOf( const_cast< PackageTreeItem* >( this ) );
    }
    return 0;
}

QVariant
PackageTreeItem::data( int column ) const
{
    if ( packageName() != nullptr )  // package
    {
        if ( !column )
        {
            return QVariant( packageName() );
        }
        return QVariant();
    }
    switch ( column )  // group
    {
    case 0:
        return QVariant( name() );
    case 1:
        return QVariant( description() );
    default:
        return QVariant();
    }
}

bool
PackageTreeItem::hiddenSelected() const
{
    if ( !isHidden() )
    {
        cError() << "Non-hidden package item" << name() << packageName() << "for hiddenSelected()";
    }

    if ( !isSelected() )
    {
        return false;
    }

    const PackageTreeItem* currentItem = parentItem();
    while ( currentItem != nullptr )
    {
        if ( !currentItem->isHidden() )
        {
            return currentItem->isSelected() != Qt::Unchecked;
        }
        currentItem = currentItem->parentItem();
    }

    /* Has no non-hidden parents */
    return isSelected();
}


void
PackageTreeItem::setSelected( Qt::CheckState isSelected )
{
    if ( parentItem() == nullptr )
    // This is the root, it is always checked so don't change state
    {
        return;
    }

    m_selected = isSelected;
    setChildrenSelected( isSelected );

    // Look for suitable parent item which may change checked-state
    // when one of its children changes.
    PackageTreeItem* currentItem = parentItem();
    while ( ( currentItem != nullptr ) && ( currentItem->childCount() == 0 ) )
    {
        currentItem = currentItem->parentItem();
    }
    if ( currentItem == nullptr )
    // Reached the root .. don't bother
    {
        return;
    }

    // Figure out checked-state based on the children
    int childrenSelected = 0;
    int childrenPartiallySelected = 0;
    for ( int i = 0; i < currentItem->childCount(); i++ )
    {
        if ( currentItem->child( i )->isSelected() == Qt::Checked )
        {
            childrenSelected++;
        }
        if ( currentItem->child( i )->isSelected() == Qt::PartiallyChecked )
        {
            childrenPartiallySelected++;
        }
    }
    if ( !childrenSelected && !childrenPartiallySelected )
    {
        currentItem->setSelected( Qt::Unchecked );
    }
    else if ( childrenSelected == currentItem->childCount() )
    {
        currentItem->setSelected( Qt::Checked );
    }
    else
    {
        currentItem->setSelected( Qt::PartiallyChecked );
    }
}

void
PackageTreeItem::setChildrenSelected( Qt::CheckState isSelected )
{
    if ( isSelected != Qt::PartiallyChecked )
        // Children are never root; don't need to use setSelected on them.
        for ( auto* child : m_childItems )
        {
            child->m_selected = isSelected;
            child->setChildrenSelected( isSelected );
        }
}

int
PackageTreeItem::type() const
{
    return QStandardItem::UserType;
}

QVariant
PackageTreeItem::toOperation() const
{
    // If it's a package with a pre- or post-script, replace
    // with the more complicated datastructure.
    if ( hasScript() )
    {
        QMap< QString, QVariant > sdetails;
        sdetails.insert( "pre-script", m_preScript );
        sdetails.insert( "package", m_packageName );
        sdetails.insert( "post-script", m_postScript );
        return sdetails;
    }
    else
    {
        return m_packageName;
    }
}
