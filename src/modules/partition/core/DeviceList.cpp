/* === This file is part of Calamares - <https://github.com/calamares> ===
 *
 *   Copyright 2015-2016, Teo Mrnjavac <teo@kde.org>
 *   Copyright 2018-2019, Adriaan de Groot <groot@kde.org>
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

#include "DeviceList.h"

#include "PartitionCoreModule.h"
#include "core/DeviceModel.h"
#include "core/KPMHelpers.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "partition/PartitionIterator.h"
#include "utils/Logger.h"

#include <kpmcore/backend/corebackend.h>
#include <kpmcore/backend/corebackendmanager.h>
#include <kpmcore/core/device.h>
#include <kpmcore/core/partition.h>

#include <QProcess>
#include <QTemporaryDir>

using CalamaresUtils::Partition::PartitionIterator;

namespace PartUtils
{

/**
 * Does the given @p device contain the root filesystem? This is true if
 * the device contains a partition which is currently mounted at / .
 */
static bool
hasRootPartition( Device* device )
{
    for ( auto it = PartitionIterator::begin( device ); it != PartitionIterator::end( device ); ++it )
        if ( ( *it )->mountPoint() == "/" )
        {
            return true;
        }
    return false;
}

static bool
blkIdCheckIso9660( const QString& path )
{
    QProcess blkid;
    blkid.start( "blkid", { path } );
    blkid.waitForFinished();
    QString output = QString::fromLocal8Bit( blkid.readAllStandardOutput() );
    return output.contains( "iso9660" );
}

static bool
isIso9660( const Device* device )
{
    const QString path = device->deviceNode();
    if ( path.isEmpty() )
    {
        return false;
    }
    if ( blkIdCheckIso9660( path ) )
    {
        return true;
    }

    if ( device->partitionTable() && !device->partitionTable()->children().isEmpty() )
    {
        for ( const Partition* partition : device->partitionTable()->children() )
        {
            if ( blkIdCheckIso9660( partition->partitionPath() ) )
            {
                return true;
            }
        }
    }
    return false;
}


static inline QDebug&
operator<<( QDebug& s, QList< Device* >::iterator& it )
{
    s << ( ( *it ) ? ( *it )->deviceNode() : QString( "<null device>" ) );
    return s;
}

using DeviceList = QList< Device* >;

static inline DeviceList::iterator
erase( DeviceList& l, DeviceList::iterator& it )
{
    Device* p = *it;
    auto r = l.erase( it );
    delete p;
    return r;
}

QList< Device* >
getDevices( DeviceType which, qint64 minimumSize )
{
    bool writableOnly = ( which == DeviceType::WritableOnly );

    CoreBackend* backend = CoreBackendManager::self()->backend();
#if defined( WITH_KPMCORE4API )
    DeviceList devices = backend->scanDevices( /* not includeReadOnly, not includeLoopback */ ScanFlag( 0 ) );
#else
    DeviceList devices = backend->scanDevices( /* excludeReadOnly */ true );
#endif

#ifdef DEBUG_PARTITION_UNSAFE
    cWarning() << "Allowing unsafe partitioning choices." << devices.count() << "candidates.";
#ifdef DEBUG_PARTITION_LAME
    cDebug() << Logger::SubEntry << "it has been lamed, and will fail.";
#endif
#else
    cDebug() << "Removing unsuitable devices:" << devices.count() << "candidates.";

    // Remove the device which contains / from the list
    for ( DeviceList::iterator it = devices.begin(); it != devices.end(); )
        if ( !( *it ) )
        {
            cDebug() << Logger::SubEntry << "Skipping nullptr device";
            it = erase( devices, it );
        }
        else if ( ( *it )->deviceNode().startsWith( "/dev/zram" ) )
        {
            cDebug() << Logger::SubEntry << "Removing zram" << it;
            it = erase( devices, it );
        }
        else if ( writableOnly && hasRootPartition( *it ) )
        {
            cDebug() << Logger::SubEntry << "Removing device with root filesystem (/) on it" << it;
            it = erase( devices, it );
        }
        else if ( writableOnly && isIso9660( *it ) )
        {
            cDebug() << Logger::SubEntry << "Removing device with iso9660 filesystem (probably a CD) on it" << it;
            it = erase( devices, it );
        }
        else if ( ( minimumSize >= 0 ) && !( ( *it )->capacity() > minimumSize ) )
        {
            cDebug() << Logger::SubEntry << "Removing too-small" << it;
            it = erase( devices, it );
        }
        else
        {
            ++it;
        }
#endif

    return devices;
}

}  // namespace PartUtils
