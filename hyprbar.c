// dep : gtk4 gtk4-layer-shell fontconfig fontawesome iw pulseaudio brightnessctl radeontop
// Penulis : Cilegordev & Dibuat bareng Claude 🤖✨
// import version 1.0.3

#include <gtk/gtk.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <fontconfig/fontconfig.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ICON_FONT_PATH "/usr/share/fonts/fontawesome/Font Awesome 6 Free-Solid-900.otf"
#define ICON_FONT_FAMILY "Font Awesome 6 Free"

static void register_icon_font(void) {
    if (!FcConfigAppFontAddFile(NULL, (const FcChar8 *)ICON_FONT_PATH)) {
        g_warning("Gagal memuat font ikon dari %s", ICON_FONT_PATH);
    }
}

static char *icon_markup(const char *str) {
    if (!str) return g_strdup("");

    GString *out = g_string_new(NULL);
    gboolean in_icon = FALSE;
    const char *p = str;

    while (*p) {
        gunichar ch = g_utf8_get_char(p);
        const char *next = g_utf8_next_char(p);
        gboolean is_icon = (ch >= 0xE000 && ch <= 0xF8FF);

        if (is_icon && !in_icon) {
            g_string_append(out, "<span font_family=\"" ICON_FONT_FAMILY "\">");
            in_icon = TRUE;
        } else if (!is_icon && in_icon) {
            g_string_append(out, "</span>");
            in_icon = FALSE;
        }

        char *esc = g_markup_escape_text(p, (int)(next - p));
        g_string_append(out, esc);
        g_free(esc);

        p = next;
    }
    if (in_icon) g_string_append(out, "</span>");

    return g_string_free(out, FALSE);
}

// Fungsi menetapkan font text dan font icon berjalan dengan sesuai
static void set_icon_label_text(GtkLabel *label, const char *text) {
    char *markup = icon_markup(text);
    gtk_label_set_markup(label, markup);
    g_free(markup);
}

// Fungsi untuk Jam
static gboolean update_time_label(gpointer user_data) {
    GtkLabel *label = GTK_LABEL(user_data);
    GtkWidget *widget = GTK_WIDGET(label);

    char time_buf[128];
    char tooltip_buf[128];
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);

    strftime(time_buf, sizeof(time_buf), " %H:%M", tm_now);
    set_icon_label_text(label, time_buf);

    strftime(tooltip_buf, sizeof(tooltip_buf), " %A, %d %B %Y", tm_now);
    gtk_widget_set_tooltip_text(widget, tooltip_buf);

    return G_SOURCE_CONTINUE;
}

// Fungsi untuk status internet
static gboolean update_wifi_status(gpointer user_data) {
    GtkLabel *label = GTK_LABEL(user_data);
    GtkWidget *widget = GTK_WIDGET(label);

    FILE *fp_eth = popen("ip -o -4 addr show | awk '{print $2,$4}'", "r");
    char iface[64], ip[64];
    gboolean eth_connected = FALSE;

    if (fp_eth) {
        while (fscanf(fp_eth, "%63s %63s", iface, ip) == 2) {
            if (strncmp(iface, "eth", 3) == 0 ||
                strncmp(iface, "enp", 3) == 0 ||
                strncmp(iface, "ens", 3) == 3 ||
                strncmp(iface, "enx", 3) == 0) {
                char *slash = strchr(ip, '/');
                if (slash) *slash = '\0';
                eth_connected = TRUE;
                break;
            }
        }
        pclose(fp_eth);
    }

    if (eth_connected) {
        char label_text[128];
        snprintf(label_text, sizeof(label_text), " \"%s\"", ip);
        set_icon_label_text(label, label_text);
        gtk_widget_set_tooltip_text(widget, "🌐");
        return G_SOURCE_CONTINUE;
    }

    FILE *fp_usb = popen("ip addr show usb0 2>/dev/null", "r");
    gboolean usb_connected = FALSE;
    if (fp_usb) {
        char line[256];
        while (fgets(line, sizeof(line), fp_usb)) {
            if (strstr(line, "inet ")) {
                usb_connected = TRUE;
                break;
            }
        }
        pclose(fp_usb);
    }

    if (usb_connected) {
        set_icon_label_text(label, " \"USB-Tethering\"");
        gtk_widget_set_tooltip_text(widget, "");
        return G_SOURCE_CONTINUE;
    }

    FILE *fp_wifi = popen("ls /sys/class/net", "r");
    gboolean connected = FALSE;
    char ssid[64] = "";
    char signal_level[64] = "";

    if (fp_wifi) {
        char ifname[64];
        while (fgets(ifname, sizeof(ifname), fp_wifi)) {
            ifname[strcspn(ifname, "\n")] = 0;

            if (strncmp(ifname, "wlan", 4) != 0 &&
                strncmp(ifname, "wlp", 3) != 0)
                continue;

            char cmd[128];
            snprintf(cmd, sizeof(cmd), "iw dev %s link", ifname);
            FILE *fp = popen(cmd, "r");
            if (!fp) continue;

            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                if (strncmp(line, "Connected to", 12) == 0) {
                    connected = TRUE;
                }

                char *ssid_ptr = strstr(line, "SSID: ");
                if (ssid_ptr) {
                    ssid_ptr += strlen("SSID: ");
                    strncpy(ssid, ssid_ptr, sizeof(ssid) - 1);
                    ssid[strcspn(ssid, "\n")] = 0;
                }

                char *signal_ptr = strstr(line, "signal: ");
                if (signal_ptr) {
                    signal_ptr += strlen("signal: ");
                    strncpy(signal_level, signal_ptr, sizeof(signal_level) - 1);
                    signal_level[strcspn(signal_level, "\n")] = 0;
                }
            }

            pclose(fp);
            if (connected && ssid[0]) break;
        }
        pclose(fp_wifi);
    }

    if (connected && ssid[0]) {
        char label_text[128];
        snprintf(label_text, sizeof(label_text), "  \"%s\"", ssid);
        set_icon_label_text(label, label_text);

        char tooltip[128];
        snprintf(tooltip, sizeof(tooltip), "  %s", signal_level[0] ? signal_level : "Unknown");
        gtk_widget_set_tooltip_text(widget, tooltip);
    } else {
        set_icon_label_text(label, " Disconnected");
        gtk_widget_set_tooltip_text(widget, "");
    }

    return G_SOURCE_CONTINUE;
}

// Fungsi untuk mengecek apakah internet terhubung
static gboolean is_interface_connected() {
    FILE *fp = popen("ls /sys/class/net", "r");
    if (!fp) return FALSE;

    char ifname[64];
    while (fgets(ifname, sizeof(ifname), fp)) {
        ifname[strcspn(ifname, "\n")] = 0;

        if (strcmp(ifname, "lo") == 0)
            continue;

        char path[256], state[32];
        snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", ifname);
        FILE *fp_state = fopen(path, "r");
        if (!fp_state) continue;

        if (fgets(state, sizeof(state), fp_state)) {
            state[strcspn(state, "\n")] = 0;
            fclose(fp_state);
            if (strcmp(state, "up") == 0) {
                pclose(fp);
                return TRUE;
            }
        } else {
            fclose(fp_state);
        }
    }

    pclose(fp);
    return FALSE;
}

// Fungsi untuk update kecepatan jaringan
static gboolean update_network_speed(gpointer user_data) {
    GtkLabel *label = GTK_LABEL(user_data);

    static unsigned long long prev_rx = 0, prev_tx = 0;
    static char current_iface[16] = "";

    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) {
        set_icon_label_text(label, " N/A");
        return G_SOURCE_CONTINUE;
    }

    char line[512];
    unsigned long long rx_bytes = 0, tx_bytes = 0;
    gboolean found = FALSE;

    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp)) {
        char iface[16];
        unsigned long long rcv, snd;
        if (sscanf(line, " %15[^:]: %llu %*s %*s %*s %*s %*s %*s %*s %llu", iface, &rcv, &snd) == 3) {
            if (strcmp(iface, "lo") == 0) continue;

            char path[64];
            snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", iface);
            FILE *state_fp = fopen(path, "r");
            if (!state_fp) continue;

            char state[16];
            if (fgets(state, sizeof(state), state_fp)) {
                state[strcspn(state, "\n")] = 0;
                fclose(state_fp);
                if (strcmp(state, "up") != 0 && strcmp(state, "unknown") != 0)
                    continue;
            }
            else {
                fclose(state_fp);
                continue;
            }

            rx_bytes = rcv;
            tx_bytes = snd;
            strncpy(current_iface, iface, sizeof(current_iface));
            found = TRUE;
            break;
        }
    }

    fclose(fp);

    if (!found) {
        set_icon_label_text(label, " N/A");
        current_iface[0] = '\0';
        prev_rx = prev_tx = 0;
        return G_SOURCE_CONTINUE;
    }

    double rx_kb = (rx_bytes - prev_rx) / 1024.0;
    double tx_kb = (tx_bytes - prev_tx) / 1024.0;
    double rx_mb = rx_kb / 1024.0;
    double tx_mb = tx_kb / 1024.0;

    prev_rx = rx_bytes;
    prev_tx = tx_bytes;

    char rx_text[64], tx_text[64];

    if (rx_mb >= 0.01)
        snprintf(rx_text, sizeof(rx_text), "%.2f MB/s", rx_mb);
    else
        snprintf(rx_text, sizeof(rx_text), "%.1f KB/s", rx_kb);

    if (tx_mb >= 0.01)
        snprintf(tx_text, sizeof(tx_text), "%.2f MB/s", tx_mb);
    else
        snprintf(tx_text, sizeof(tx_text), "%.1f KB/s", tx_kb);

    char speed_label[128];
    snprintf(speed_label, sizeof(speed_label), " %s   %s", rx_text, tx_text);
    set_icon_label_text(label, speed_label);

    return G_SOURCE_CONTINUE;
}

// Fungsi untuk status batrai
static gboolean update_battery_status(gpointer user_data) {
    GtkLabel *label = GTK_LABEL(user_data);
    FILE *fp;
    char path[1035];

    if (access("/sys/class/power_supply/BAT0/capacity", F_OK) != 0) {
        set_icon_label_text(label, "-PC");
        return G_SOURCE_CONTINUE;
    }

    fp = fopen("/sys/class/power_supply/BAT0/capacity", "r");
    if (fp == NULL) {
        set_icon_label_text(label, " Not Available");
        return G_SOURCE_CONTINUE;
    }

    if (fgets(path, sizeof(path), fp) != NULL) {
        path[strcspn(path, "\n")] = 0;
        fclose(fp);

        fp = fopen("/sys/class/power_supply/ADP0/online", "r");
        if (fp == NULL) {
            set_icon_label_text(label, " Not Available");
            return G_SOURCE_CONTINUE;
        }

        char charging_status[10];
        if (fgets(charging_status, sizeof(charging_status), fp) != NULL) {
            charging_status[strcspn(charging_status, "\n")] = 0;
            char battery_status[50];

            if (strcmp(charging_status, "1") == 0) {
                snprintf(battery_status, sizeof(battery_status), " %s%%", path);
            }
            else {
                snprintf(battery_status, sizeof(battery_status), " %s%%", path);
            }

            set_icon_label_text(label, battery_status);
        }
        else {
            set_icon_label_text(label, " Not Available");
        }

        fclose(fp);
    }
    else {
        set_icon_label_text(label, " Not Available");
        fclose(fp);
    }

    return G_SOURCE_CONTINUE;
}

// Fungsi untuk status mikrofon
static gboolean update_mic_status(gpointer user_data) {
    GtkLabel *label = GTK_LABEL(user_data);

    FILE *fp = popen("pactl get-source-volume @DEFAULT_SOURCE@ | grep -oP '\\d+%' | head -1", "r");
    if (!fp) {
        set_icon_label_text(label, " Off");
        return G_SOURCE_CONTINUE;
    }

    char volume[16];
    if (fgets(volume, sizeof(volume), fp) == NULL) {
        pclose(fp);
        set_icon_label_text(label, " Off");
        return G_SOURCE_CONTINUE;
    }
    volume[strcspn(volume, "\n")] = 0;
    pclose(fp);

    fp = popen("pactl get-source-mute @DEFAULT_SOURCE@ | awk '{print $2}'", "r");
    if (!fp) {
        set_icon_label_text(label, " Off");
        return G_SOURCE_CONTINUE;
    }

    char mute_status[8];
    if (fgets(mute_status, sizeof(mute_status), fp) == NULL) {
        pclose(fp);
        set_icon_label_text(label, " Off");
        return G_SOURCE_CONTINUE;
    }
    mute_status[strcspn(mute_status, "\n")] = 0;
    pclose(fp);

    char label_text[64];
    if (strcmp(mute_status, "yes") == 0) {
        snprintf(label_text, sizeof(label_text), " Mute");
    } else {
        snprintf(label_text, sizeof(label_text), " %s", volume);
    }

    set_icon_label_text(label, label_text);
    return G_SOURCE_CONTINUE;
}

// Fungsi untuk output suara
static gboolean update_volume_status(gpointer user_data) {
    GtkLabel *label = GTK_LABEL(user_data);

    FILE *fp = popen("pactl get-sink-volume @DEFAULT_SINK@ | grep -oP '\\d+%' | head -1", "r");
    if (!fp) {
        set_icon_label_text(label, " Off");
        return G_SOURCE_CONTINUE;
    }

    char volume[16];
    if (fgets(volume, sizeof(volume), fp) == NULL) {
        pclose(fp);
        set_icon_label_text(label, " Off");
        return G_SOURCE_CONTINUE;
    }
    volume[strcspn(volume, "\n")] = 0;
    pclose(fp);

    fp = popen("pactl get-sink-mute @DEFAULT_SINK@ | awk '{print $2}'", "r");
    if (!fp) {
        set_icon_label_text(label, " Off");
        return G_SOURCE_CONTINUE;
    }

    char mute_status[8];
    if (fgets(mute_status, sizeof(mute_status), fp) == NULL) {
        pclose(fp);
        set_icon_label_text(label, " Off");
        return G_SOURCE_CONTINUE;
    }
    mute_status[strcspn(mute_status, "\n")] = 0;
    pclose(fp);

    char label_text[64];
    if (strcmp(mute_status, "yes") == 0) {
        snprintf(label_text, sizeof(label_text), " Mute");
    } else {
        snprintf(label_text, sizeof(label_text), "  %s", volume);
    }

    set_icon_label_text(label, label_text);
    return G_SOURCE_CONTINUE;
}

// Fungsi untuk output kecerahan layar monitor
static gboolean update_brightness_status(gpointer user_data) {
    GtkLabel *label = GTK_LABEL(user_data);
    FILE *fp = popen("brightnessctl -m | cut -d, -f4", "r");
    if (!fp) {
        set_icon_label_text(label, " N/A");
        return G_SOURCE_CONTINUE;
    }

    char brightness[32];
    if (fgets(brightness, sizeof(brightness), fp) != NULL) {
        brightness[strcspn(brightness, "\n")] = 0;
        char label_text[64];
        snprintf(label_text, sizeof(label_text), " %s", brightness);
        set_icon_label_text(label, label_text);
    }

    else {
        set_icon_label_text(label, " N/A");
    }

    pclose(fp);
    return G_SOURCE_CONTINUE;
}

// Fungsi untuk status hardware
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static gboolean update_resource_usage(GtkLabel *label) {
    static long long prev_idle = 0, prev_total = 0;
    long long idle, total;
    char buf[256];

    // --- CPU ---
    char cpu_name[128] = "Unknown CPU";
    FILE *fp_cpuinfo = fopen("/proc/cpuinfo", "r");
    if (fp_cpuinfo) {
        char line[256];
        while (fgets(line, sizeof(line), fp_cpuinfo)) {
            if (strncmp(line, "model name", 10) == 0) {
                char *colon = strchr(line, ':');
                if (colon) {
                    snprintf(cpu_name, sizeof(cpu_name), "%s", colon + 2);
                    cpu_name[strcspn(cpu_name, "\n")] = 0;
                    break;
                }
            }
        }
        fclose(fp_cpuinfo);
    }

    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        set_icon_label_text(label, "CPU: N/A GPU: N/A RAM: N/A SWAP: N/A DISK: N/A");
        return G_SOURCE_CONTINUE;
    }

    char line[512];
    fgets(line, sizeof(line), fp);
    fclose(fp);

    long long user, nice, system, idle_time, iowait, irq, softirq, steal;
    sscanf(line, "cpu %lld %lld %lld %lld %lld %lld %lld %lld",
           &user, &nice, &system, &idle_time, &iowait, &irq, &softirq, &steal);

    idle = idle_time + iowait;
    total = user + nice + system + idle + irq + softirq + steal;

    long long diff_idle = idle - prev_idle;
    long long diff_total = total - prev_total;
    double cpu_usage = 0.0;
    if (diff_total != 0)
        cpu_usage = (1.0 - (double)diff_idle / diff_total) * 100.0;

    prev_idle = idle;
    prev_total = total;

    // --- RAM ---
    char ram_usage[16] = "N/A";      // persentase untuk label
    long ram_total_mib = 0;          // total untuk tooltip
    FILE *fp_ram = popen("free | awk '/Mem:/ {print int($3*100/$2)}'", "r");
    if (fp_ram && fgets(ram_usage, sizeof(ram_usage), fp_ram)) {
        ram_usage[strcspn(ram_usage, "\n")] = 0;
    }
    if (fp_ram) fclose(fp_ram);

    FILE *fp_ram_total = popen("free -m | awk '/Mem:/ {print $2}'", "r");
    if (fp_ram_total) {
        fscanf(fp_ram_total, "%ld", &ram_total_mib);
        fclose(fp_ram_total);
    }

    // --- SWAP ---
    char swap_usage[16] = "N/A";         // untuk status bar
    char swap_percent_str[16] = "N/A";   // untuk tooltip
    long swap_total_mib = 0;

    FILE *fp_swap = popen("free | awk '/Swap:/ {if ($2==0) print 0; else print int($3*100/$2)}'", "r");
    if (fp_swap && fgets(swap_usage, sizeof(swap_usage), fp_swap)) {
        swap_usage[strcspn(swap_usage, "\n")] = 0;
        snprintf(swap_percent_str, sizeof(swap_percent_str), "%s", swap_usage);
    }
    if (fp_swap) fclose(fp_swap);

    FILE *fp_swap_total = popen("free -m | awk '/Swap:/ {print $2}'", "r");
    if (fp_swap_total) {
        fscanf(fp_swap_total, "%ld", &swap_total_mib);
        fclose(fp_swap_total);
    }

    // --- GPU ---
    char gpu_usage_str[16] = "N/A";
    float gpu_usage = 0.0f;
    char gpu_name[128] = "Unknown GPU";

    FILE *fp_lspci = popen("lspci | grep VGA | head -n1", "r");
    if (fp_lspci) {
        char line[128];
        if (fgets(line, sizeof(line), fp_lspci)) {
            line[strcspn(line, "\n")] = 0;
            snprintf(gpu_name, sizeof(gpu_name), "%s", line);
        }
        pclose(fp_lspci);
    }

    if (access("/sys/class/drm/card0/device/gpu_busy_percent", F_OK) == 0) {
        FILE *fp_gpu = fopen("/sys/class/drm/card0/device/gpu_busy_percent", "r");
        if (fp_gpu && fgets(gpu_usage_str, sizeof(gpu_usage_str), fp_gpu)) {
            gpu_usage_str[strcspn(gpu_usage_str, "\n")] = 0;
            gpu_usage = atof(gpu_usage_str);
        }
        if (fp_gpu) fclose(fp_gpu);
    } else {
        FILE *fp_gpu = popen("radeontop -d - -l 1 | grep -oP 'gpu \\K[0-9.]+'", "r");
        if (fp_gpu && fgets(gpu_usage_str, sizeof(gpu_usage_str), fp_gpu)) {
            gpu_usage_str[strcspn(gpu_usage_str, "\n")] = 0;
            gpu_usage = atof(gpu_usage_str);
        }
        if (fp_gpu) pclose(fp_gpu);
    }

    // --- Disk ---
    char disk_usage[16] = "N/A";     // persentase untuk label
    char disk_total[16] = "N/A";     // total untuk tooltip
    FILE *fp_disk = popen("df -h / | awk 'NR==2 {print $5 \" \" $2}'", "r");
    if (fp_disk) {
        fscanf(fp_disk, "%15s %15s", disk_usage, disk_total);
        fclose(fp_disk);
    }

    // --- FORMAT LABEL STATUS BAR ---
    snprintf(buf, sizeof(buf),
             " %.0f%%   %.0f%%   %s%%   %s%%   %s",
             cpu_usage, gpu_usage, ram_usage, swap_usage, disk_usage);
    set_icon_label_text(label, buf);

    // --- TOOLTIP DYNAMIC ---
    char tooltip[512];
    snprintf(tooltip, sizeof(tooltip),
             "CPU: %s\nGPU: %s\nRAM: %s%% / %ldMiB\nSWAP: %s%% / %ldMiB\nDisk: %s/%s",
             cpu_name, gpu_name, ram_usage, ram_total_mib,
             swap_percent_str, swap_total_mib,
             disk_usage, disk_total);

    // Pastikan tooltip diaktifkan
    gtk_widget_set_has_tooltip(GTK_WIDGET(label), TRUE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(label), tooltip);

    return G_SOURCE_CONTINUE;
}

// Fungsi untuk informasi versi dari kernel linux
static gboolean update_linux_version(gpointer user_data) {
    GtkLabel *label = GTK_LABEL(user_data);
    FILE *fp;
    char path[1035];

    fp = popen("uname -r", "r");
    if (fp == NULL) {
        set_icon_label_text(label, "");
        return G_SOURCE_CONTINUE;
    }

    if (fgets(path, sizeof(path), fp) != NULL) {
        path[strcspn(path, "\n")] = 0;
        char version_label[128];
        snprintf(version_label, sizeof(version_label), " %s", path);
        set_icon_label_text(label, version_label);
    }

    else {
        set_icon_label_text(label, "");
    }

    fclose(fp);
    return G_SOURCE_CONTINUE;
}

// Struktur data statusbar
static void activate(GtkApplication *app, gpointer user_data) {
    register_icon_font();

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "hyprbar");
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    GdkDisplay *display = gdk_display_get_default();
    GListModel *monitors = gdk_display_get_monitors(display);
    GdkMonitor *monitor = g_list_model_get_item(monitors, 0);
    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    gtk_window_set_default_size(GTK_WINDOW(window), geometry.width, 32);

    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(window));

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 5);
    gtk_widget_set_margin_bottom(box, 5);
    GtkWidget *left_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_hexpand(left_box, TRUE);
    gtk_widget_set_halign(right_box, GTK_ALIGN_END);

    GtkWidget *time_label = gtk_label_new("Clock");
    gtk_box_append(GTK_BOX(left_box), time_label);
    GtkWidget *wifi_label = gtk_label_new("Wi-Fi");
    gtk_box_append(GTK_BOX(left_box), wifi_label);
    GtkWidget *net_label = gtk_label_new("Network");
    gtk_box_append(GTK_BOX(left_box), net_label);
    GtkWidget *battery_label = gtk_label_new("Power");
    gtk_box_append(GTK_BOX(left_box), battery_label);
    GtkWidget *mic_label = gtk_label_new("Microphone");
    gtk_box_append(GTK_BOX(left_box), mic_label);
    GtkWidget *volume_label = gtk_label_new("Sound");
    gtk_box_append(GTK_BOX(left_box), volume_label);
    GtkWidget *brightness_label = gtk_label_new("Brightness");
    gtk_box_append(GTK_BOX(left_box), brightness_label);
    GtkWidget *resource_label = gtk_label_new("Hardware");
    gtk_box_append(GTK_BOX(left_box), resource_label);
    GtkWidget *linux_label = gtk_label_new("Linux");
    gtk_box_append(GTK_BOX(right_box), linux_label);

    gtk_box_append(GTK_BOX(box), left_box);
    gtk_box_append(GTK_BOX(box), right_box);
    gtk_window_set_child(GTK_WINDOW(window), box);

    g_timeout_add_seconds(1, (GSourceFunc)update_time_label, time_label);
    g_timeout_add_seconds(1, (GSourceFunc)update_wifi_status, wifi_label);
    g_timeout_add_seconds(1, (GSourceFunc)update_network_speed, net_label);
    g_timeout_add_seconds(1, (GSourceFunc)update_battery_status, battery_label);
    g_timeout_add_seconds(1, (GSourceFunc)update_mic_status, mic_label);
    g_timeout_add_seconds(1, (GSourceFunc)update_volume_status, volume_label);
    g_timeout_add_seconds(1, (GSourceFunc)update_brightness_status, brightness_label);
    g_timeout_add_seconds(1, (GSourceFunc)update_resource_usage, resource_label);
    update_linux_version(GTK_LABEL(linux_label));

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new("org.hyprbar", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
