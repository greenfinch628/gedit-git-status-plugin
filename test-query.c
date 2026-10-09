/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "git-query.h"
int main(int argc,char **argv)
{
    GCancellable *cancel=g_cancellable_new();
    GitSnapshot *s=git_snapshot_read(argv[1],cancel);
    g_assert_nonnull(s);
    gchar *path=g_build_filename(argv[1],"modified.txt",NULL);
    g_assert_cmpstr(g_hash_table_lookup(s->codes,path),==," M"); g_free(path);
    path=g_build_filename(argv[1],"added.txt",NULL);
    g_assert_cmpstr(g_hash_table_lookup(s->codes,path),==,"A "); g_free(path);
    path=g_build_filename(argv[1],"sub","odd\nname.txt",NULL);
    g_assert_cmpstr(g_hash_table_lookup(s->codes,path),==,"??"); g_free(path);
    path=g_build_filename(argv[1],"sub",NULL);
    g_assert_cmpstr(g_hash_table_lookup(s->codes,path),==,"●"); g_free(path);
    g_assert_nonnull(s->branch); git_snapshot_free(s);
    g_assert_null(git_snapshot_read("/tmp",cancel));
    g_cancellable_cancel(cancel); g_assert_null(git_snapshot_read(argv[1],cancel));
    g_object_unref(cancel);
    g_print("PASS: modified, staged addition, newline filename, folder aggregation, non-repo, cancellation\n");
}
