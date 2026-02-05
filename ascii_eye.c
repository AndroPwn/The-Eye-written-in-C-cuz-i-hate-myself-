#include <gtk/gtk.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

typedef struct { double x, y; } Point;

// ==========================================
// --- GLOBAL CONFIGURATION ---
// ==========================================
double SCATTER_HOLD_TIME   = 80.0;  
double RETURN_SPEED        = 0.015; 
double MOUSE_DELAY_SMOOTH  = 0.07;  

double LID_SPACING         = 0.02;  
double INNER_SPACING       = 0.15;  

// --- NEW PUPIL SCALE VARIABLE ---
// 0.5 = small/constricted, 1.0 = normal, 2.5 = huge/dilated
double PUPIL_SCALE         = 1.0; 

double LID_SENSITIVITY     = 0.12; 
double CIRC_SENSITIVITY    = 0.22; 
double IRIS_SENSITIVITY    = 0.38; 

double EYE_WIDTH           = 420.0; 
double EYE_HEIGHT          = 140.0; 
double EYE_PINCH           = 6.0;   
double CIRCLE_RADIUS       = 110.0; 

double LIMIT_X             = 85.0;  
double LIMIT_Y             = 20.0;  

double shake_intensity = 0.0;
double red_flash_timer = 0.0;
double critical_timer  = 0.0; 
gboolean permanent_red_lock = FALSE;

const char* UI_TITLE       = "The Eye";
const char* UI_LINE_1      = "1. Move your mouse around";
const char* UI_LINE_2      = "2. DO NOT DRAG AND CLICK ON THE EYE";
const char* UI_TEXT_ALARM  = "STOP"; 
const char* UI_TEXT_CHAOS  = "YOU DIDNT LISTEN";

double IRIS_W = 28.0, IRIS_H = 55.0; 
double BASE_PUPIL_W = 12.0, BASE_PUPIL_H = 30.0; // Renamed to use as bases

Point mouse_actual = {-500, -500}; 
Point mouse_delayed = {0, 0};
gboolean m1_down = FALSE;
double path_offset = 0.0;
gboolean permanently_opened = FALSE, blink_active = FALSE, is_primed = FALSE; 

double scat[15000], hold[15000]; 
double blink_scat[5000], blink_hold[5000]; 

Point get_rand_dir(double seed) { return (Point){ cos(seed * 123.4), sin(seed * 567.8) }; }

void trigger_feedback() { 
    if(!permanent_red_lock) {
        shake_intensity = 15.0; 
        red_flash_timer = 1.0; 
    }
}

gboolean is_inside_eye(Point p, Point lid_center) {
    double rel_x = p.x - lid_center.x;
    double norm_x = fabs(rel_x) / EYE_WIDTH;
    if (norm_x >= 1.0) return FALSE;
    double limit_y = EYE_HEIGHT * (1.0 - pow(norm_x, EYE_PINCH));
    return (fabs(p.y - lid_center.y) <= limit_y);
}

static void draw_char(cairo_t *cr, double x, double y, const char *letter) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_move_to(cr, -3, 5); 
    cairo_show_text(cr, letter);
    cairo_restore(cr);
}

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    if (shake_intensity > 0.1) {
        cairo_translate(cr, ((rand()%100)/100.0-0.5)*shake_intensity, ((rand()%100)/100.0-0.5)*shake_intensity);
    }

    if (permanent_red_lock) cairo_set_source_rgb(cr, 1.0, 0, 0); 
    else cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_paint(cr);

    cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    
    if (permanent_red_lock) {
        cairo_set_font_size(cr, 140.0); 
        cairo_set_source_rgb(cr, 1, 1, 1); 
        cairo_move_to(cr, 40, 160); 
        cairo_show_text(cr, UI_TEXT_CHAOS);
    } 
    else if (red_flash_timer > 0.1) {
        cairo_set_font_size(cr, 22.0);
        cairo_set_source_rgb(cr, 1, 0, 0);
        cairo_move_to(cr, 25, 50);
        cairo_show_text(cr, UI_TEXT_ALARM);
    } 
    else {
        cairo_set_source_rgb(cr, 0, 1, 0);
        cairo_set_font_size(cr, 18.0);
        cairo_move_to(cr, 25, 40); cairo_show_text(cr, UI_TITLE);
        cairo_set_font_size(cr, 13.0);
        cairo_move_to(cr, 25, 70); cairo_show_text(cr, UI_LINE_1);
        cairo_move_to(cr, 25, 95); cairo_show_text(cr, UI_LINE_2);
    }

    cairo_set_font_size(cr, 11.0); 
    cairo_set_source_rgb(cr, 1, 1, 1);
    Point center = {w / 2.0, h / 2.0};
    if (mouse_delayed.x == 0) mouse_delayed = center;

    double dx = mouse_delayed.x - center.x, dy = mouse_delayed.y - center.y;
    Point lid_pos = { center.x + dx * LID_SENSITIVITY, center.y + dy * LID_SENSITIVITY };
    
    double c_dx = dx * CIRC_SENSITIVITY, c_dy = dy * CIRC_SENSITIVITY;
    if (c_dx > LIMIT_X) c_dx = LIMIT_X; if (c_dx < -LIMIT_X) c_dx = -LIMIT_X;
    if (c_dy > LIMIT_Y) c_dy = LIMIT_Y; if (c_dy < -LIMIT_Y) c_dy = -LIMIT_Y;
    Point circ_pos = { lid_pos.x + c_dx, lid_pos.y + c_dy };

    Point iris_pos = { circ_pos.x + dx * 0.10, circ_pos.y + dy * 0.10 };
    Point pupil_pos = { iris_pos.x + dx * 0.05, iris_pos.y + dy * 0.05 };

    if (!permanently_opened && !permanent_red_lock) {
        double d_m = sqrt(pow(mouse_actual.x-center.x,2)+pow(mouse_actual.y-center.y,2));
        if (d_m < 350.0) { blink_active = TRUE; if (m1_down) is_primed = TRUE; }
        else { if (is_primed) { permanently_opened = TRUE; is_primed = FALSE; } blink_active = FALSE; }
    }

    int idx = 0, f_idx = 0;
    // --- LID LAYER (0100) ---
    for (double i = 0; i < 2.0 * M_PI; i += LID_SPACING, idx++) {
        double rot = i + path_offset, t = pow(fabs(cos(rot)), EYE_PINCH); 
        double x_p = lid_pos.x + (t * (cos(rot)>0?EYE_WIDTH:-EYE_WIDTH) + (1.0-t)*EYE_HEIGHT*cos(rot));
        double y_p = lid_pos.y + (EYE_HEIGHT * 1.1) * sin(rot) * (1.0-t*0.97);
        if (!permanent_red_lock && m1_down && sqrt(pow(x_p-mouse_actual.x,2)+pow(y_p-mouse_actual.y,2)) < 30) { scat[idx]=1.0; hold[idx]=SCATTER_HOLD_TIME; trigger_feedback(); }
        draw_char(cr, x_p + get_rand_dir(i).x*(scat[idx]*400), y_p + get_rand_dir(i).y*(scat[idx]*400), "0100");

        if (sin(rot) > 0 && (blink_active || (permanently_opened && blink_scat[f_idx] > 0.01))) {
            double y_bot = lid_pos.y * 2 - y_p;
            for (double sy = 12.0; sy < fabs(y_bot-y_p); sy += 12.0) {
                if (f_idx >= 5000) break;
                double fill_y = y_p + (y_bot > y_p ? sy : -sy);
                if (!permanent_red_lock && m1_down && sqrt(pow(x_p-mouse_actual.x,2)+pow(fill_y-mouse_actual.y,2)) < 35) { blink_scat[f_idx]=1.0; blink_hold[f_idx]=SCATTER_HOLD_TIME; trigger_feedback(); }
                draw_char(cr, x_p + get_rand_dir(i+sy).x*(blink_scat[f_idx]*450), fill_y + get_rand_dir(i+sy).y*(blink_scat[f_idx]*450), "i");
                f_idx++;
            }
        }
    }

    // --- INNER LAYERS ---
    const char* syms[] = {"i", "i", "i"};
    // Applying PUPIL_SCALE to the third index of widths/heights
    double r_ws[] = {CIRCLE_RADIUS, IRIS_W, BASE_PUPIL_W * PUPIL_SCALE};
    double r_hs[] = {CIRCLE_RADIUS, IRIS_H, BASE_PUPIL_H * PUPIL_SCALE};
    Point off[] = {circ_pos, iris_pos, pupil_pos};
    double fsc[] = {250, 200, 150};

    for(int l=0; l<3; l++) {
        for (double i = 0; i < 2.0 * M_PI; i += (INNER_SPACING * (l+1)), idx++) {
            double spin_i = i + (path_offset * (l + 1)); 
            Point p = { off[l].x + r_ws[l]*cos(spin_i), off[l].y + r_hs[l]*sin(spin_i) };
            if (is_inside_eye(p, lid_pos) || permanent_red_lock) {
                if (!permanent_red_lock && m1_down && sqrt(pow(p.x-mouse_actual.x,2)+pow(p.y-mouse_actual.y,2)) < 25) { scat[idx]=1.0; hold[idx]=SCATTER_HOLD_TIME; trigger_feedback(); }
                draw_char(cr, p.x + get_rand_dir(i).x*(scat[idx]*fsc[l]), p.y + get_rand_dir(i).y*(scat[idx]*fsc[l]), syms[l]);
            }
        }
    }

    if (!permanent_red_lock && red_flash_timer > 0.01) { 
        cairo_set_source_rgba(cr, 1.0, 0, 0, red_flash_timer * 0.4); 
        cairo_paint(cr); 
    }
    return FALSE;
}

// ... update_animation, mouse signals, and main same as before ...
static gboolean update_animation(GtkWidget *widget) {
    path_offset += 0.015;
    mouse_delayed.x += (mouse_actual.x-mouse_delayed.x)*MOUSE_DELAY_SMOOTH;
    mouse_delayed.y += (mouse_actual.y-mouse_delayed.y)*MOUSE_DELAY_SMOOTH;
    if (!permanent_red_lock) {
        if (red_flash_timer > 0.1) {
            critical_timer += 0.016; 
            if (critical_timer > 20.0) { 
                permanent_red_lock = TRUE; 
                shake_intensity = 50.0; 
                for(int i=0; i<15000; i++) scat[i] = 1.2 + ((rand()%100)/100.0);
                for(int i=0; i<5000; i++) blink_scat[i] = 1.2 + ((rand()%100)/100.0);
            }
        } else { critical_timer = 0; }
        if (shake_intensity > 0) shake_intensity *= 0.85;
        if (red_flash_timer > 0) red_flash_timer -= 0.04;
        for (int i = 0; i < 15000; i++) { if (hold[i] > 0) hold[i] -= 1.0; else if (scat[i] > 0) scat[i] -= RETURN_SPEED; }
        for (int i = 0; i < 5000; i++) { if (blink_hold[i] > 0) blink_hold[i] -= 1.0; else if (blink_scat[i] > 0) blink_scat[i] -= RETURN_SPEED; }
    } else { shake_intensity = 10.0; }
    gtk_widget_queue_draw(widget);
    return TRUE;
}

static gboolean on_mouse_move(GtkWidget *widget, GdkEventMotion *event) { (void)widget; mouse_actual.x = event->x; mouse_actual.y = event->y; return TRUE; }
static gboolean on_mouse_button(GtkWidget *widget, GdkEventButton *event) { (void)widget; if (event->button == 1) m1_down = (event->type == GDK_BUTTON_PRESS); return TRUE; }

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    memset(scat, 0, sizeof(scat)); memset(blink_scat, 0, sizeof(blink_scat));
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *da = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(win), da);
    gtk_widget_add_events(da, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
    g_signal_connect(da, "draw", G_CALLBACK(on_draw_event), NULL);
    g_signal_connect(da, "motion-notify-event", G_CALLBACK(on_mouse_move), NULL);
    g_signal_connect(da, "button-press-event", G_CALLBACK(on_mouse_button), NULL);
    g_signal_connect(da, "button-release-event", G_CALLBACK(on_mouse_button), NULL);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_timeout_add(16, (GSourceFunc)update_animation, da);
    gtk_window_set_default_size(GTK_WINDOW(win), 1200, 800);
    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}