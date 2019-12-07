/*
 * Copyright (c) 2018-2019 Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "fs_shim.h"

/* Missing fsp-srv commands. */
static Result _fsOpenSession(Service *s, Service* out, u32 cmd_id) {
    return serviceDispatch(s, cmd_id,
        .out_num_objects = 1,
        .out_objects = out,
    );
}

Result fsOpenSdCardFileSystemFwd(Service* s, FsFileSystem* out) {
    return _fsOpenSession(s, &out->s, 18);
}

Result fsOpenBisStorageFwd(Service* s, FsStorage* out, FsBisPartitionId partition_id) {
    const u32 tmp = partition_id;
    return serviceDispatchIn(s, 12, tmp,
        .out_num_objects = 1,
        .out_objects = &out->s,
    );
}

Result fsOpenDataStorageByCurrentProcessFwd(Service* s, FsStorage* out) {
    return _fsOpenSession(s, &out->s, 200);
}

Result fsOpenDataStorageByDataIdFwd(Service* s, FsStorage* out, u64 data_id, NcmStorageId storage_id) {
    const struct {
        u8 storage_id;
        u64 data_id;
    } in = { storage_id, data_id };

    return serviceDispatchIn(s, 202, in,
        .out_num_objects = 1,
        .out_objects = &out->s,
    );
}

Result fsOpenSaveDataFileSystemFwd(Service* s, FsFileSystem* out, FsSaveDataSpaceId save_data_space_id, const FsSaveDataAttribute *attr) {
    const struct {
        u8 save_data_space_id;
        u8 pad[7];
        FsSaveDataAttribute attr;
    } in = { (u8)save_data_space_id, {0}, *attr };

    return serviceDispatchIn(s, 51, in,
        .out_num_objects = 1,
        .out_objects = &out->s,
    );
}
