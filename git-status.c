/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <libpeas/peas.h>
#include <gedit/gedit-window.h>
#include <gedit/gedit-window-activatable.h>
#include <gedit/gedit-message-bus.h>
#include "git-query.h"

#define BUS_PATH "/plugins/filebrowser"
/* Shared File Browser model columns in gedit 50. Validate before reading. */
#define LOCATION_COLUMN 3
typedef struct {
    PeasExtensionBase parent;
    GeditWindow *window;
    GtkTreeView *view;
    GtkTreeViewColumn *column;
    GCancellable *cancel;
    GitSnapshot *snapshot;
    gchar *directory;
    guint timer;
    gboolean active, busy, old_tooltip;
    gulong tooltip_handler;
} GitStatus;
typedef PeasExtensionBaseClass GitStatusClass;
static void activatable_init(GeditWindowActivatableInterface *iface);
G_DEFINE_DYNAMIC_TYPE_EXTENDED(GitStatus,git_status,PEAS_TYPE_EXTENSION_BASE,0,
    G_IMPLEMENT_INTERFACE_DYNAMIC(GEDIT_TYPE_WINDOW_ACTIVATABLE,activatable_init))

static gboolean valid_model(GtkTreeModel *model)
{
    return model && g_str_equal(G_OBJECT_TYPE_NAME(model),"GeditFileBrowserStore") &&
        gtk_tree_model_get_n_columns(model)>LOCATION_COLUMN &&
        g_type_is_a(gtk_tree_model_get_column_type(model,LOCATION_COLUMN),G_TYPE_FILE);
}
static void badge(GtkTreeViewColumn *column, GtkCellRenderer *cell,
                  GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    GitStatus *self=data; GFile *file=NULL; gchar *path=NULL;
    const gchar *code=NULL;
    if (self->snapshot && valid_model(model)) {
        gtk_tree_model_get(model,iter,LOCATION_COLUMN,&file,-1);
        if (file) { path=g_file_get_path(file); g_object_unref(file); }
        if (path) code=g_hash_table_lookup(self->snapshot->codes,path);
    }
    g_object_set(cell,"text",code ? code : "",NULL); g_free(path);
}
static gboolean tooltip(GtkWidget *widget, gint x, gint y, gboolean keyboard,
                         GtkTooltip *tip, gpointer data)
{
    GitStatus *self=data; GtkTreeViewColumn *column=NULL;
    if (!self->snapshot || keyboard) return FALSE;
    gtk_tree_view_convert_widget_to_bin_window_coords(self->view,x,y,&x,&y);
    if (!gtk_tree_view_get_path_at_pos(self->view,x,y,NULL,&column,NULL,NULL) ||
        column!=self->column) return FALSE;
    gchar *text=g_strdup_printf("Branch: %s\nGit index / working tree status\n●: changes below this folder\nDisk status; save edits to update Git status.",self->snapshot->branch);
    gtk_tooltip_set_text(tip,text); g_free(text); return TRUE;
}
static void detach(GitStatus *self)
{
    if (self->view) {
        g_signal_handler_disconnect(self->view,self->tooltip_handler);
        gtk_widget_set_has_tooltip(GTK_WIDGET(self->view),self->old_tooltip);
        gtk_tree_view_remove_column(self->view,self->column);
        g_clear_object(&self->view);
    }
    self->column=NULL;
}
static void attach(GitStatus *self)
{
    GeditMessageBus *bus=gedit_window_get_message_bus(self->window);
    if (!gedit_message_bus_is_registered(bus,BUS_PATH,"get_view")) {
        detach(self); return;
    }
    GeditMessage *message=gedit_message_bus_send_sync(bus,BUS_PATH,"get_view",NULL,NULL);
    GtkTreeView *view=NULL;
    if (message) { g_object_get(message,"view",&view,NULL); g_object_unref(message); }
    if (view==self->view) { g_clear_object(&view); return; }
    detach(self); self->view=view;
    if (!view) return;
    GtkCellRenderer *cell=gtk_cell_renderer_text_new();
    g_object_set(cell,"xpad",6,NULL);
    self->column=gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(self->column,cell,FALSE);
    gtk_tree_view_column_set_cell_data_func(self->column,cell,badge,self,NULL);
    gtk_tree_view_insert_column(view,self->column,0);
    self->old_tooltip=gtk_widget_get_has_tooltip(GTK_WIDGET(view));
    gtk_widget_set_has_tooltip(GTK_WIDGET(view),TRUE);
    self->tooltip_handler=g_signal_connect(view,"query-tooltip",G_CALLBACK(tooltip),self);
}
static void worker(GTask *task,gpointer source,gpointer data,GCancellable *cancel)
{ g_task_return_pointer(task,git_snapshot_read(data,cancel),(GDestroyNotify)git_snapshot_free); }
static void completed(GObject *source,GAsyncResult *result,gpointer data)
{
    GitStatus *self=(GitStatus *)source;
    GitSnapshot *snapshot=g_task_propagate_pointer(G_TASK(result),NULL);
    self->busy=FALSE;
    if (self->active && self->cancel==g_task_get_cancellable(G_TASK(result)) &&
        g_strcmp0(self->directory,g_task_get_task_data(G_TASK(result)))==0) {
        git_snapshot_free(self->snapshot); self->snapshot=snapshot; snapshot=NULL;
        if (self->view) gtk_widget_queue_draw(GTK_WIDGET(self->view));
    }
    git_snapshot_free(snapshot);
}
static gboolean refresh(gpointer data)
{
    GitStatus *self=data;
    attach(self);
    GFile *root=NULL;
    GeditMessageBus *bus=gedit_window_get_message_bus(self->window);
    if (self->view && gedit_message_bus_is_registered(bus,BUS_PATH,"get_root")) {
        GeditMessage *message=gedit_message_bus_send_sync(bus,BUS_PATH,"get_root",NULL,NULL);
        if (message) { g_object_get(message,"location",&root,NULL); g_object_unref(message); }
    }
    gchar *directory=root ? g_file_get_path(root) : NULL;
    g_clear_object(&root);
    if (g_strcmp0(directory,self->directory)!=0) {
        g_free(self->directory); self->directory=g_strdup(directory);
        g_clear_pointer(&self->snapshot,git_snapshot_free);
        if (self->view) gtk_widget_queue_draw(GTK_WIDGET(self->view));
    }
    if (directory && !self->busy) {
        self->busy=TRUE;
        GTask *task=g_task_new(self,self->cancel,completed,NULL);
        g_task_set_task_data(task,g_strdup(directory),g_free);
        g_task_run_in_thread(task,worker); g_object_unref(task);
    }
    g_free(directory); return G_SOURCE_CONTINUE;
}
static void activate(GeditWindowActivatable *object)
{
    GitStatus *self=(GitStatus *)object;
    self->active=TRUE; self->cancel=g_cancellable_new();
    refresh(self); self->timer=g_timeout_add_seconds(2,refresh,self);
}
static void deactivate(GeditWindowActivatable *object)
{
    GitStatus *self=(GitStatus *)object;
    self->active=FALSE;
    if (self->timer) { g_source_remove(self->timer); self->timer=0; }
    if (self->cancel) g_cancellable_cancel(self->cancel);
    g_clear_object(&self->cancel); detach(self);
    g_clear_pointer(&self->snapshot,git_snapshot_free);
    g_clear_pointer(&self->directory,g_free);
}
static void set_property(GObject *object,guint id,const GValue *value,GParamSpec *spec)
{ ((GitStatus *)object)->window=g_value_dup_object(value); }
static void get_property(GObject *object,guint id,GValue *value,GParamSpec *spec)
{ g_value_set_object(value,((GitStatus *)object)->window); }
static void dispose(GObject *object)
{
    GitStatus *self=(GitStatus *)object;
    deactivate((GeditWindowActivatable *)self); g_clear_object(&self->window);
    G_OBJECT_CLASS(git_status_parent_class)->dispose(object);
}
static void git_status_class_init(GitStatusClass *klass)
{
    GObjectClass *object=G_OBJECT_CLASS(klass);
    object->set_property=set_property; object->get_property=get_property;
    object->dispose=dispose; g_object_class_override_property(object,1,"window");
}
static void git_status_class_finalize(GitStatusClass *klass) {}
static void git_status_init(GitStatus *self) {}
static void activatable_init(GeditWindowActivatableInterface *iface)
{ iface->activate=activate; iface->deactivate=deactivate; }
G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module)
{
    git_status_register_type(G_TYPE_MODULE(module));
    peas_object_module_register_extension_type(module,GEDIT_TYPE_WINDOW_ACTIVATABLE,git_status_get_type());
}
