/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <gio/gio.h>
typedef struct { GHashTable *codes; gchar *branch; } GitSnapshot;
GitSnapshot *git_snapshot_read(const gchar *directory, GCancellable *cancel);
void git_snapshot_free(GitSnapshot *snapshot);
