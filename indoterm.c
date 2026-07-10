// dep : gtk3 vte
// Penulis : Cilegordev & Dibuat bareng Claude 🤖✨
// import version 0.8.3

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <stdlib.h>

static GtkWidget *g_notebook = NULL;
static GtkHeaderBar *g_header = NULL;
static gint g_tab_counter = 0;

static GtkWidget *create_terminal_tab(void);

static void
update_tabs_visibility(void)
{
    gint n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(g_notebook));
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(g_notebook), n_pages >= 2);
}

static void
close_tab(GtkWidget *page)
{
    gint page_num = gtk_notebook_page_num(GTK_NOTEBOOK(g_notebook), page);
    if (page_num < 0)
        return;

    gtk_notebook_remove_page(GTK_NOTEBOOK(g_notebook), page_num);

    if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(g_notebook)) == 0) {
        gtk_main_quit();
        return;
    }

    update_tabs_visibility();
}

static void
on_child_exited(VteTerminal *terminal, gint status, gpointer user_data)
{
    (void) terminal;
    (void) status;
    GtkWidget *page = GTK_WIDGET(user_data);
    close_tab(page);
}

static void
on_window_destroy(GtkWidget *widget, gpointer data)
{
    (void) widget;
    (void) data;
    gtk_main_quit();
}

static void
scroll_terminal_to_bottom(VteTerminal *terminal)
{
    GtkAdjustment *vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(terminal));
    if (vadj != NULL) {
        gtk_adjustment_set_value(vadj,
            gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj));
    }
}

static void
on_copy_activate(GtkMenuItem *item, gpointer user_data)
{
    (void) item;
    VteTerminal *terminal = VTE_TERMINAL(user_data);
    vte_terminal_copy_clipboard_format(terminal, VTE_FORMAT_TEXT);
}

static void
on_paste_activate(GtkMenuItem *item, gpointer user_data)
{
    (void) item;
    VteTerminal *terminal = VTE_TERMINAL(user_data);
    vte_terminal_paste_clipboard(terminal);
    scroll_terminal_to_bottom(terminal);
}

static gboolean
on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    (void) user_data;
    if (event->type != GDK_BUTTON_PRESS || event->button != GDK_BUTTON_SECONDARY)
        return FALSE;

    GtkWidget *menu = gtk_menu_new();

    GtkWidget *copy_item = gtk_menu_item_new_with_label("Copy");
    gtk_widget_set_sensitive(copy_item,
        vte_terminal_get_has_selection(VTE_TERMINAL(widget)));
    g_signal_connect(copy_item, "activate", G_CALLBACK(on_copy_activate), widget);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), copy_item);

    GtkWidget *paste_item = gtk_menu_item_new_with_label("Paste");
    g_signal_connect(paste_item, "activate", G_CALLBACK(on_paste_activate), widget);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), paste_item);

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *) event);

    return TRUE;
}

static void
on_info_clicked(GtkButton *button, gpointer user_data)
{
    (void) user_data;

    char info[512];
    g_snprintf(info, sizeof(info),
        "<b>Di compile dengan :</b>\n"
        "GCC-v%s\n"
        "GTK+-3.0-v%d.%d.%d\n"
        "VTE-2.91-v%d.%d.%d\n"
        "Versi terminal saat ini : 0.8.3",
        __VERSION__,
        gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version(),
        vte_get_major_version(), vte_get_minor_version(), vte_get_micro_version());

    GtkWidget *popover = gtk_popover_new(GTK_WIDGET(button));

    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), info);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_widget_set_margin_top(label, 12);
    gtk_widget_set_margin_bottom(label, 12);
    gtk_widget_set_margin_start(label, 14);
    gtk_widget_set_margin_end(label, 14);

    gtk_container_add(GTK_CONTAINER(popover), label);
    gtk_widget_show_all(popover);
    gtk_popover_popup(GTK_POPOVER(popover));
}

static void
on_new_tab_clicked(GtkButton *button, gpointer user_data)
{
    (void) button;
    (void) user_data;
    create_terminal_tab();
}

static void
on_tab_close_clicked(GtkButton *button, gpointer user_data)
{
    (void) button;
    GtkWidget *page = GTK_WIDGET(user_data);
    close_tab(page);
}

static void
on_paste_clipboard(VteTerminal *terminal, gpointer user_data)
{
    (void) user_data;
    scroll_terminal_to_bottom(terminal);
}

static void
on_spawn_complete(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data)
{
    (void) pid;
    GtkWidget *page = GTK_WIDGET(user_data);

    if (error != NULL) {
        g_printerr("Gagal menjalankan shell: %s\n", error->message);
        close_tab(page);
    }
}

static GtkWidget *
create_terminal_tab(void)
{
    /* Widget VTE terminal */
    GtkWidget *terminal = vte_terminal_new();

    /* Tampilan dasar: font, scrollback, warna */
    PangoFontDescription *font = pango_font_description_from_string("Liberation Mono 11");
    vte_terminal_set_font(VTE_TERMINAL(terminal), font);
    pango_font_description_free(font);

    /* Scrollback tanpa batas */
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(terminal), -1);
    vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(terminal), VTE_CURSOR_BLINK_ON);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(terminal), TRUE);
    vte_terminal_set_scroll_on_output(VTE_TERMINAL(terminal), FALSE);
    vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(terminal), TRUE);

    /* Foreground mengikuti tema sistem, background dipaksa hitam pekat */
    {
        GtkStyleContext *style = gtk_widget_get_style_context(GTK_WIDGET(g_header));
        GdkRGBA fg, bg;
        gtk_style_context_get_color(style, GTK_STATE_FLAG_NORMAL, &fg);
        gdk_rgba_parse(&bg, "#000000");
        vte_terminal_set_colors(VTE_TERMINAL(terminal), &fg, &bg, NULL, 0);
    }

    /* Bungkus dengan scrollbar -> ini jadi widget halaman (page) notebook */
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *scrollbar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL,
        gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(terminal)));

    gtk_box_pack_start(GTK_BOX(page), terminal, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(page), scrollbar, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(page), "vte-terminal", terminal);

    /* Label tab + tombol close kecil */
    GtkWidget *tab_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    g_tab_counter++;
    gchar *tab_title = g_strdup_printf("Tab ke %d", g_tab_counter);
    GtkWidget *tab_label = gtk_label_new(tab_title);
    g_free(tab_title);
    gtk_label_set_ellipsize(GTK_LABEL(tab_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(tab_label), 18);
    gtk_widget_set_halign(tab_label, GTK_ALIGN_START);

    GtkWidget *close_button = gtk_button_new_from_icon_name("window-close",
        GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(close_button), GTK_RELIEF_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(close_button), "flat");
    gtk_widget_set_tooltip_text(close_button, "Tutup tab");
    g_signal_connect(close_button, "clicked", G_CALLBACK(on_tab_close_clicked), page);

    gtk_box_pack_start(GTK_BOX(tab_box), tab_label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), close_button, FALSE, FALSE, 0);
    gtk_widget_show_all(tab_box);

    gint page_num = gtk_notebook_append_page(GTK_NOTEBOOK(g_notebook), page, tab_box);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(g_notebook), page, TRUE);
    gtk_container_child_set(GTK_CONTAINER(g_notebook), page,
        "tab-expand", TRUE, "tab-fill", TRUE, NULL);

    /* Jalankan shell login pengguna */
    const char *shell_env = g_getenv("SHELL");
    char *shell = g_strdup(shell_env && *shell_env ? shell_env : "/bin/bash");
    char *command[] = { shell, NULL };

    gchar **envp = g_get_environ();
    gint vte_version = vte_get_major_version() * 10000
                      + vte_get_minor_version() * 100
                      + vte_get_micro_version();
    gchar *vte_version_str = g_strdup_printf("%d", vte_version);
    envp = g_environ_setenv(envp, "VTE_VERSION", vte_version_str, TRUE);
    g_free(vte_version_str);

    envp = g_environ_setenv(envp, "TERM", "xterm-256color", TRUE);

    vte_terminal_spawn_async(
        VTE_TERMINAL(terminal),
        VTE_PTY_DEFAULT,
        g_get_home_dir(),
        command,
        envp,
        G_SPAWN_SEARCH_PATH,
        NULL, NULL, NULL,
        -1,
        NULL,
        on_spawn_complete,
        page
    );
    g_free(shell);
    g_strfreev(envp);

    g_signal_connect(terminal, "child-exited", G_CALLBACK(on_child_exited), page);
    g_signal_connect(terminal, "button-press-event", G_CALLBACK(on_button_press), NULL);
    g_signal_connect_after(terminal, "paste-clipboard", G_CALLBACK(on_paste_clipboard), NULL);

    gtk_widget_show_all(page);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(g_notebook), page_num);
    gtk_widget_grab_focus(terminal);
    update_tabs_visibility();

    return page;
}

int
main(int argc, char *argv[])
{
    /* Paksa matikan ATI-SPI2-CORE */
    g_setenv("NO_AT_BRIDGE", "1", TRUE);

    gtk_init(&argc, &argv);

    /* Jendela utama */
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Indoterm");
    gtk_window_set_default_size(GTK_WINDOW(window), 824, 472);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);

    /* Headerbar client-side decoration bawaan */
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "Terminal");
    gtk_window_set_titlebar(GTK_WINDOW(window), header);
    g_header = GTK_HEADER_BAR(header);

    /* Tombol info (tentang) di kanan headerbar */
    GtkWidget *info_button = gtk_button_new_from_icon_name("gtk-about",
        GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(info_button, "Informasi terminal");
    gtk_style_context_add_class(gtk_widget_get_style_context(info_button), "flat");
    g_signal_connect(info_button, "clicked", G_CALLBACK(on_info_clicked), NULL);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), info_button);

    /* Tombol new tab, diposisikan sebelum tombol tentang */
    GtkWidget *new_tab_button = gtk_button_new_from_icon_name("gtk-new",
        GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(new_tab_button, "Tab baru");
    gtk_style_context_add_class(gtk_widget_get_style_context(new_tab_button), "flat");
    g_signal_connect(new_tab_button, "clicked", G_CALLBACK(on_new_tab_clicked), NULL);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), new_tab_button);

    /* Notebook untuk tab-tab terminal */
    GtkWidget *notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook), FALSE);
    g_notebook = notebook;

    gtk_container_add(GTK_CONTAINER(window), notebook);

    /* Buat tab pertama */
    create_terminal_tab();

    gtk_widget_show_all(window);
    gtk_main();

    return EXIT_SUCCESS;
}
