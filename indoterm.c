// dep : gtk3 vte zsh
// Penulis : Cilegordev & Dibuat bareng ChatGPT 🤖✨
// import version: 0.5.1-beta

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include <string.h>

GtkWidget *notebook;
GtkWidget *main_window;
GtkWidget *header;

typedef struct {
    GtkWidget *terminal;
    GtkWidget *label;
    GPid pid;
    gchar *last_cwd;
    guint timeout_id;
} TerminalData;

static void update_tab_title(TerminalData *data);
static void update_tab_visibility(void);
static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static void on_switch_page(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer user_data);

static void on_selection_changed(VteTerminal *terminal, gpointer user_data) {
    vte_terminal_copy_clipboard_format(terminal, VTE_FORMAT_TEXT);

    vte_terminal_set_scroll_on_output(terminal, FALSE);
}

static void update_tab_visibility() {
    int page_count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));

    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), page_count > 1);

    if (page_count == 0) {
        gtk_window_close(GTK_WINDOW(main_window));
    }
}

static gboolean check_cwd_change(gpointer user_data) {
    TerminalData *data = (TerminalData *)user_data;
    if (data->pid <= 0) return G_SOURCE_CONTINUE;

    gchar *proc_path = g_strdup_printf("/proc/%d/cwd", data->pid);
    gchar *cwd = g_file_read_link(proc_path, NULL);
    g_free(proc_path);

    if (cwd) {
        if (data->last_cwd == NULL || strcmp(data->last_cwd, cwd) != 0) {
            g_free(data->last_cwd);
            data->last_cwd = cwd;
            update_tab_title(data);
        } else {
            g_free(cwd);
        }
    }
    return G_SOURCE_CONTINUE;
}

static void update_tab_title(TerminalData *data) {
    if (data->pid > 0) {
        gchar *basename = NULL;
        if (data->last_cwd) {
            basename = g_path_get_basename(data->last_cwd);
        } else {
            gchar *proc_path = g_strdup_printf("/proc/%d/cwd", data->pid);
            gchar *cwd = g_file_read_link(proc_path, NULL);
            g_free(proc_path);

            if (cwd) {
                basename = g_path_get_basename(cwd);
                data->last_cwd = cwd;
            }
        }

        if (basename) {
            gtk_label_set_text(GTK_LABEL(data->label), basename);
            g_free(basename);
        } else {
            gtk_label_set_text(GTK_LABEL(data->label), "Terminal");
        }
    }
}

static void on_cwd_changed(VteTerminal *terminal, GParamSpec *pspec, gpointer user_data) {
    TerminalData *data = (TerminalData *)user_data;
    if (data->timeout_id) {
        g_source_remove(data->timeout_id);
    }
    data->timeout_id = g_timeout_add(100, check_cwd_change, data);
}

//BADPIG
static void copy_terminal_text(GtkWidget *widget, gpointer user_data) {
    VteTerminal *terminal = VTE_TERMINAL(user_data);
    vte_terminal_copy_clipboard_format(terminal, VTE_FORMAT_TEXT);
}

static gboolean scroll_terminal_bottom(gpointer user_data) {
    VteTerminal *terminal = VTE_TERMINAL(user_data);
    GtkAdjustment *adjustment = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(terminal));

    if (adjustment) {
        gdouble upper = gtk_adjustment_get_upper(adjustment);
        gdouble page_size = gtk_adjustment_get_page_size(adjustment);
        gtk_adjustment_set_value(adjustment, upper - page_size);
    }

    return G_SOURCE_REMOVE;
}

static void paste_terminal_text(GtkWidget *widget, gpointer user_data) {
    VteTerminal *terminal = VTE_TERMINAL(user_data);
    vte_terminal_paste_clipboard(terminal);
    g_idle_add(scroll_terminal_bottom, terminal);
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        GtkWidget *menu = gtk_menu_new();

        GtkWidget *copy_item = gtk_menu_item_new_with_label("Copy");
        GtkWidget *paste_item = gtk_menu_item_new_with_label("Paste");

        g_signal_connect(copy_item, "activate", G_CALLBACK(copy_terminal_text), user_data);
        g_signal_connect(paste_item, "activate", G_CALLBACK(paste_terminal_text), user_data);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), copy_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), paste_item);

        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);

        return TRUE;
    }
    return FALSE;
}

static void on_switch_page(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer user_data) {
    TerminalData *data = g_object_get_data(G_OBJECT(page), "terminal-data");
    if (data) {
        update_tab_title(data);
    }
}

static GtkWidget* create_tab_label(GtkWidget *child, TerminalData *data) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *label = gtk_label_new("Terminal");
    GtkWidget *close_btn = gtk_button_new_with_label("⨯");

    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_widget_destroy), child);
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(update_tab_visibility), NULL);

    data->label = label;

    gtk_button_set_relief(GTK_BUTTON(close_btn), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(close_btn, FALSE);
    gtk_widget_set_valign(close_btn, GTK_ALIGN_CENTER);

    gtk_widget_set_hexpand(label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);

    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), close_btn, FALSE, FALSE, 0);

    gtk_widget_show_all(box);
    return box;
}

static void on_terminal_exit(VteTerminal *terminal, gint status, gpointer user_data) {
    TerminalData *data = (TerminalData *)user_data;
    if (data->timeout_id) {
        g_source_remove(data->timeout_id);
    }

    GtkWidget *page = gtk_widget_get_parent(GTK_WIDGET(terminal));
    gtk_widget_destroy(page);

    update_tab_visibility();

    g_free(data->last_cwd);
    g_free(data);
}

static void on_terminal_spawn_success(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data) {
    TerminalData *data = (TerminalData *)user_data;

    if (error != NULL) {
        g_warning("Failed to spawn terminal: %s", error->message);
        g_error_free(error);
        return;
    }

    data->pid = pid;
    data->timeout_id = g_timeout_add(100, check_cwd_change, data);
    update_tab_title(data);
    gtk_widget_grab_focus(GTK_WIDGET(terminal));
}

static GtkWidget* create_terminal_tab(TerminalData **data_ptr) {
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *terminal = vte_terminal_new();
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(terminal), -1);
    gtk_container_add(GTK_CONTAINER(scrolled_window), terminal);
    vte_terminal_set_scroll_on_output(VTE_TERMINAL(terminal), FALSE);
    

    TerminalData *data = g_new0(TerminalData, 1);
    data->terminal = terminal;
    data->pid = -1;
    data->last_cwd = NULL;
    data->timeout_id = 0;
    *data_ptr = data;

    g_object_set_data(G_OBJECT(scrolled_window), "terminal-data", data);
    
    //BADPIG
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

    g_signal_connect(terminal, "button-press-event", G_CALLBACK(on_button_press), terminal);
    g_signal_connect(terminal, "notify::current-directory-uri", G_CALLBACK(on_cwd_changed), data);

    char *cwd = g_get_current_dir();
    char **argv = (char*[]){ "/bin/zsh", NULL };
    vte_terminal_spawn_async(
        VTE_TERMINAL(terminal),
        VTE_PTY_DEFAULT,
        cwd,
        argv,
        NULL,
        G_SPAWN_DEFAULT,
        NULL, NULL,
        NULL, -1,
        NULL,
        on_terminal_spawn_success,
        data
    );

    g_signal_connect(terminal, "child-exited", G_CALLBACK(on_terminal_exit), data);
    g_free(cwd);

    return scrolled_window;
}

static void on_new_tab(GtkButton *button, gpointer user_data) {
    GtkNotebook *nb = GTK_NOTEBOOK(user_data);
    TerminalData *data = NULL;
    GtkWidget *terminal = create_terminal_tab(&data);
    GtkWidget *tab_label = create_tab_label(terminal, data);

    int page_num = gtk_notebook_append_page(nb, terminal, tab_label);

    gtk_widget_show_all(terminal);
    gtk_notebook_set_current_page(nb, page_num);
    gtk_widget_grab_focus(terminal);

    update_tab_visibility();
}

static void activate(GtkApplication *app, gpointer user_data) {
    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "indoterm-dev");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 824, 472);
    gtk_window_set_icon_name(GTK_WINDOW(main_window), "utilities-terminal");

    gtk_window_set_position(GTK_WINDOW(main_window), GTK_WIN_POS_CENTER);

    header = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "indoterm-dev");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(main_window), header);

    GtkWidget *new_tab_icon = gtk_image_new_from_icon_name("gtk-new", GTK_ICON_SIZE_BUTTON);
    GtkWidget *new_tab_btn = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(new_tab_btn), GTK_RELIEF_NONE);
    gtk_container_add(GTK_CONTAINER(new_tab_btn), new_tab_icon);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), new_tab_btn);

    notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    gtk_container_add(GTK_CONTAINER(main_window), notebook);

    g_signal_connect(new_tab_btn, "clicked", G_CALLBACK(on_new_tab), notebook);
    g_signal_connect(notebook, "switch-page", G_CALLBACK(on_switch_page), NULL);

    on_new_tab(GTK_BUTTON(new_tab_btn), notebook);

    gtk_widget_show_all(main_window);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.vte.terminal", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
