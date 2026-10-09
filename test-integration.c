/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <gedit/gedit-app.h>
#include <gedit/gedit-window-activatable.h>
#include <gedit/gedit-message-bus.h>
#include <libpeas/peas.h>
#include "git-query.h"
/* Test-only setup functions exported by gedit 50, not used by the plugin. */
void gedit_dirs_init(void);
GObject *gedit_factory_new(void);
static void pump(guint seconds)
{
    gint64 end=g_get_monotonic_time()+seconds*G_USEC_PER_SEC;
    while (g_get_monotonic_time()<end) {
        while (g_main_context_iteration(NULL,FALSE));
        g_usleep(10000);
    }
}
static gboolean check_badge(GtkTreeModel *model,GtkTreePath *path,GtkTreeIter *iter,gpointer data)
{
    GtkTreeViewColumn *column=data; GFile *file=NULL;
    gtk_tree_model_get(model,iter,3,&file,-1);
    if (!file) return FALSE;
    gchar *name=g_file_get_basename(file);
    if (g_str_equal(name,"modified.txt")) {
        gtk_tree_view_column_cell_set_cell_data(column,model,iter,FALSE,FALSE);
        GList *cells=gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(column));
        gchar *text=NULL; g_object_get(cells->data,"text",&text,NULL);
        g_assert_cmpstr(text,==," M"); g_free(text); g_list_free(cells);
        g_object_set_data(G_OBJECT(column),"verified",GINT_TO_POINTER(1));
    }
    g_free(name); g_object_unref(file); return FALSE;
}
int main(int argc,char **argv)
{
    gtk_init(&argc,&argv); gedit_dirs_init(); tepl_init();
    GObject *factory=gedit_factory_new();
    tepl_abstract_factory_set_singleton(TEPL_ABSTRACT_FACTORY(factory));
    GeditApp *app=g_object_new(GEDIT_TYPE_APP,"application-id","org.gnome.gedit.GitStatusTest",
                               "flags",G_APPLICATION_NON_UNIQUE,NULL);
    g_application_set_resource_base_path(G_APPLICATION(app),"/org/gnome/gedit");
    g_assert_true(g_application_register(G_APPLICATION(app),NULL,NULL));
    GSettings *settings=g_settings_new("org.gnome.gedit.plugins");
    const gchar *empty[]={NULL};
    g_settings_set_strv(settings,"active-plugins",empty); g_object_unref(settings);
    GeditWindow *window=gedit_app_create_window(app,NULL);
    PeasEngine *engine=peas_engine_new();
    peas_engine_add_search_path(engine,argv[1],NULL);
    peas_engine_add_search_path(engine,argv[2],NULL);
    PeasPluginInfo *browser=peas_engine_get_plugin_info(engine,"filebrowser");
    g_assert_nonnull(browser); g_assert_true(peas_engine_load_plugin(engine,browser));
    PeasExtension *fb=peas_engine_create_extension(engine,browser,GEDIT_TYPE_WINDOW_ACTIVATABLE,"window",window,NULL);
    gedit_window_activatable_activate(GEDIT_WINDOW_ACTIVATABLE(fb));
    GeditMessageBus *bus=gedit_window_get_message_bus(window);
    GFile *root=g_file_new_for_path(argv[3]);
    GeditMessage *msg=gedit_message_bus_send_sync(bus,"/plugins/filebrowser","set_root","location",root,NULL);
    g_clear_object(&msg); g_object_unref(root); pump(1);
    msg=gedit_message_bus_send_sync(bus,"/plugins/filebrowser","get_view",NULL,NULL);
    GtkTreeView *view=NULL; g_object_get(msg,"view",&view,NULL); g_object_unref(msg);
    gint before=gtk_tree_view_get_n_columns(view);
    PeasPluginInfo *plugin=peas_engine_get_plugin_info(engine,"git-status");
    g_assert_true(peas_engine_load_plugin(engine,plugin));
    PeasExtension *extension=peas_engine_create_extension(engine,plugin,GEDIT_TYPE_WINDOW_ACTIVATABLE,"window",window,NULL);
    gedit_window_activatable_activate(GEDIT_WINDOW_ACTIVATABLE(extension)); pump(3);
    g_assert_cmpint(gtk_tree_view_get_n_columns(view),==,before+1);
    GtkTreeViewColumn *column=gtk_tree_view_get_column(view,0);
    gtk_tree_model_foreach(gtk_tree_view_get_model(view),check_badge,column);
    g_assert_nonnull(g_object_get_data(G_OBJECT(column),"verified"));
    gedit_window_activatable_deactivate(GEDIT_WINDOW_ACTIVATABLE(extension));
    g_assert_cmpint(gtk_tree_view_get_n_columns(view),==,before);
    /* Re-enable, then disable while the worker may still be running. */
    gedit_window_activatable_activate(GEDIT_WINDOW_ACTIVATABLE(extension));
    gedit_window_activatable_deactivate(GEDIT_WINDOW_ACTIVATABLE(extension)); pump(1);
    g_assert_cmpint(gtk_tree_view_get_n_columns(view),==,before);
    g_object_unref(extension); gedit_window_activatable_deactivate(GEDIT_WINDOW_ACTIVATABLE(fb));
    g_object_unref(fb); g_object_unref(view);
    g_print("PASS: load, modified badge, disable cleanup, re-enable and cancellation\n");
    return 0;
}
