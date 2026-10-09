/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "git-query.h"
#include <string.h>

static GBytes *run(const gchar *dir, GCancellable *cancel, const gchar *a,
                   const gchar *b, const gchar *c, const gchar *d)
{
    const gchar *argv[] = {"git", "--no-optional-locks", "-C", dir, a,b,c,d,NULL};
    GSubprocess *process = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_SILENCE, NULL);
    GBytes *out = NULL;
    if (!process) return NULL;
    if (!g_subprocess_communicate(process, NULL, cancel, &out, NULL, NULL) ||
        !g_subprocess_get_successful(process)) {
        g_clear_pointer(&out, g_bytes_unref);
        if (g_cancellable_is_cancelled(cancel)) g_subprocess_force_exit(process);
    }
    g_object_unref(process);
    return out;
}

GitSnapshot *git_snapshot_read(const gchar *directory, GCancellable *cancel)
{
    GBytes *root_bytes = run(directory,cancel,"rev-parse","--show-toplevel",NULL,NULL);
    if (!root_bytes) return NULL;
    gsize size; const gchar *data = g_bytes_get_data(root_bytes,&size);
    gchar *root = g_strndup(data,size);
    /* Git terminates this output with one newline; preserve whitespace in paths. */
    if (size && root[size-1]=='\n') root[size-1]=0;
    g_bytes_unref(root_bytes);
    GBytes *bytes = run(root,cancel,"status","--porcelain=v1","-z","--untracked-files=all");
    if (!bytes) { g_free(root); return NULL; }
    GitSnapshot *s = g_new0(GitSnapshot,1);
    s->codes = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,g_free);
    data = g_bytes_get_data(bytes,&size);
    for (gsize i=0;i<size;) {
        const gchar *end = memchr(data+i,0,size-i);
        if (!end || end-data-i<4) break;
        gchar code[3] = {data[i],data[i+1],0};
        gchar *path = g_build_filename(root,data+i+3,NULL);
        g_hash_table_replace(s->codes,path,g_strdup(code));
        /* Mark every ancestor, including folders not expanded in the browser. */
        gchar *parent = g_path_get_dirname(path);
        while (g_str_has_prefix(parent,root)) {
            if (!g_hash_table_contains(s->codes,parent))
                g_hash_table_insert(s->codes,g_strdup(parent),g_strdup("●"));
            if (g_str_equal(parent,root)) break;
            gchar *next = g_path_get_dirname(parent); g_free(parent); parent=next;
        }
        g_free(parent);
        i = end-data+1;
        /* In -z output rename/copy destination comes first, then the old name. */
        if (code[0]=='R'||code[0]=='C'||code[1]=='R'||code[1]=='C') {
            end = memchr(data+i,0,size-i); if (!end) break;
            /* A rename also changes the folder from which the file moved. */
            if (code[0]=='R'||code[1]=='R') {
                gchar *old_path=g_build_filename(root,data+i,NULL);
                gchar *old_parent=g_path_get_dirname(old_path);
                while (g_str_has_prefix(old_parent,root)) {
                    if (!g_hash_table_contains(s->codes,old_parent))
                        g_hash_table_insert(s->codes,g_strdup(old_parent),g_strdup("●"));
                    if (g_str_equal(old_parent,root)) break;
                    gchar *next=g_path_get_dirname(old_parent);
                    g_free(old_parent); old_parent=next;
                }
                g_free(old_parent); g_free(old_path);
            }
            i=end-data+1;
        }
    }
    g_bytes_unref(bytes);
    bytes=run(root,cancel,"symbolic-ref","--quiet","--short","HEAD");
    if (bytes) { data=g_bytes_get_data(bytes,&size); s->branch=g_strndup(data,size);
        g_strchomp(s->branch); g_bytes_unref(bytes); }
    else s->branch=g_strdup("detached HEAD");
    g_free(root); return s;
}
void git_snapshot_free(GitSnapshot *s)
{ if (s) { g_hash_table_unref(s->codes); g_free(s->branch); g_free(s); } }
