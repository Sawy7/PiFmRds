#!python
#cython: language_level=3

import gi, os, subprocess, re

gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk

# Functions
def toggle_transmission(switch, state, win_layout):
    if state == True:
        os.system("systemctl --user start pi_fm_rds")
    else:
        os.system("systemctl --user stop pi_fm_rds")
    reset_window(win_layout, state)

def toggle_metadata(switch, state, win_layout):
    switch_rds_metadata(state)
    reset_window(win_layout, True)

def change_freq(button, freq_spin):
    file_content = None
    with open("/usr/bin/pi_fm_runner", "r") as f:
        file_content = f.read()
        new_freq = freq_spin.get_text().replace(",", ".")
        file_content = re.sub("-freq \d*.\d*", f"-freq {new_freq}", file_content)

    with open("/usr/bin/pi_fm_runner", "w") as f:
        f.write(file_content)

    os.system("systemctl --user restart pi_fm_rds")
    reset_window(win_layout, True)

def switch_rds_metadata(state):
    file_content = None
    with open("/usr/bin/pi_fm_runner", "r") as f:
        file_content = f.read()
        if state:
            file_content = re.sub("-ctl rdspipe -rdsh rdshistory.txt", f"-dbus", file_content)
        else:
            file_content = re.sub("-dbus", f"-ctl rdspipe -rdsh rdshistory.txt", file_content)

    with open("/usr/bin/pi_fm_runner", "w") as f:
        f.write(file_content)

    os.system("systemctl --user restart pi_fm_rds")
    reset_window(win_layout, True)

def freq_changing(spinbutton, freq):
    style_provider = Gtk.CssProvider()
    bg_color = "#33D17A" if spinbutton.get_text().replace(",", ".") == freq else "#F57900"
    css = "#freq_spin { background:" +  bg_color + "; }"
    style_provider.load_from_data(bytes(css.encode()))
    Gtk.StyleContext.add_provider_for_screen(
        Gdk.Screen.get_default(), style_provider,
        Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
    )

def station_changing(entry, entry_name, history_value):
    style_provider = Gtk.CssProvider()
    bg_color = "#33D17A" if entry.get_text() == history_value else "#F57900"
    css = "#" + entry_name + "{ background:" +  bg_color + "; }"
    style_provider.load_from_data(bytes(css.encode()))
    Gtk.StyleContext.add_provider_for_screen(
        Gdk.Screen.get_default(), style_provider,
        Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
    )

def disable_entry(entry, entry_name):
    entry.set_editable(False)
    style_provider = Gtk.CssProvider()
    bg_color = "#86929A"
    css = "#" + entry_name + "{ background:" +  bg_color + "; }"
    style_provider.load_from_data(bytes(css.encode()))
    Gtk.StyleContext.add_provider_for_screen(
        Gdk.Screen.get_default(), style_provider,
        Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
    )

def is_service_running():
    fmstatus = os.system("systemctl --user is-active --quiet pi_fm_rds")
    if fmstatus == 0:
        return True
    else:
        return False

def is_metadata_on():
    file_content = None
    with open("/usr/bin/pi_fm_runner", "r") as f:
        file_content = f.read()
    if re.search("-dbus", file_content) is not None:
        return True
    else:
        return False

def change_rds(button, station_name, station_text, rds_history):
    station_name_new = station_name.get_text()
    if station_name_new != "":
        os.system(f"echo 'PS {station_name_new}' > /tmp/rdspipe")
        rds_history["station_name"] = station_name_new
        station_changing(station_name, "station_name", station_name_new)
    station_text_new = station_text.get_text()
    if station_text_new != "":
        os.system(f"echo 'RT {station_text_new}' > /tmp/rdspipe")
        rds_history["station_text"] = station_text_new
        station_changing(station_text, "station_text", station_text_new)

def get_rds_history():
    with open("/tmp/rdshistory.txt", "r") as f:
        content = f.readlines()
        for line in content:
            field_text = line[3:-1]
            if "PS" in line:
                station_name = field_text
            elif "RT" in line:
                station_text = field_text
    return {"station_name": station_name, "station_text": station_text}

def populate_window(win_layout, state):
    win_left = win_layout.get_children()[0]
    win_right = win_layout.get_children()[2] # 1 is separator
    fmstatus_label = Gtk.Label()
    if state == True:
        header_image = Gtk.Image.new_from_icon_name("media-playback-start", Gtk.IconSize.DIALOG)
        win_left.add(header_image)
        header_image.show()

        fmstatus = "Running"
        fmstatus_label.set_text(fmstatus)
        win_left.add(fmstatus_label)
        fmstatus_label.show()

        rds_metadata_state = is_metadata_on()
        
        freq = str(subprocess.check_output("systemctl --user status pi_fm_rds | grep freq", shell=True))
        freq = re.findall("-freq (\d*.\d*)", freq)[0]

        style_provider = Gtk.CssProvider()
        css = "#freq_spin { background: #33D17A; } #station_name { background: #33D17A; } #station_text { background: #33D17A; }"
        style_provider.load_from_data(bytes(css.encode()))
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(), style_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

        freq_adjust = Gtk.Adjustment(value=float(freq),
                                     lower=76,
                                     upper=108,
                                     step_increment=0.1,
                                     page_increment=0.5,
                                     page_size=0.0)
        freq_spin = Gtk.SpinButton(adjustment=freq_adjust, climb_rate=1.0, digits=1, name="freq_spin")
        freq_spin.connect("value-changed", freq_changing, freq)
        win_left.add(freq_spin)
        freq_spin.show()
        
        freq_button = Gtk.Button(label="Change frequency")
        freq_button.connect("clicked", change_freq, freq_spin)
        win_left.add(freq_button)
        freq_button.show()

        rds_history = get_rds_history()
        
        station_name_label = Gtk.Label()
        station_name_label.set_text("Station name")
        station_name = Gtk.Entry(name="station_name")
        station_name.connect("changed", station_changing, "station_name", rds_history["station_name"])
        win_right.attach(station_name_label, 0,0,1,1)
        win_right.attach(station_name, 1,0,1,1)
        station_name_label.show()
        station_name.show()

        station_text_label = Gtk.Label()
        station_text_label.set_text("Station text")
        station_text = Gtk.Entry(name="station_text")
        station_text.connect("changed", station_changing, "station_text", rds_history["station_text"])
        station_text_label.set_margin_top(5)
        win_right.attach(station_text_label, 0,1,1,1)
        station_text.set_margin_top(5)
        win_right.attach(station_text, 1,1,1,1)
        station_text_label.show()
        station_text.show()

        if rds_metadata_state:
            disable_entry(station_name, "station_name")
            disable_entry(station_text, "station_text")
        else:
            station_name.set_text(rds_history["station_name"])
            station_text.set_text(rds_history["station_text"])

        rds_button = Gtk.Button(label="Change RDS")
        rds_button.connect("clicked", change_rds, station_name, station_text, rds_history)
        rds_button.set_margin_top(20)
        win_right.attach(rds_button, 0,2,2,1)
        rds_button.show()
        rds_button.grab_focus()

        rds_metadata_label = Gtk.Label()
        rds_metadata_label.set_text("Auto-RDS")
        rds_metadata_label.set_margin_top(20)
        win_right.attach(rds_metadata_label, 0,3,1,1)
        rds_metadata_label.show()
        rds_metadata = Gtk.Switch()
        rds_metadata.set_state(rds_metadata_state)
        rds_metadata.connect("state-set", toggle_metadata, win_layout)
        rds_metadata.set_margin_top(20)
        win_right.attach(rds_metadata, 1,3,1,1)
        rds_metadata.show()

    else:
        fmstatus = "Dead"
        fmstatus_label.set_text(fmstatus)
        win_left.add(fmstatus_label)
        fmstatus_label.show()

def reset_window(win_layout, state):
    for side in win_layout.get_children():
        if type(side) == Gtk.Separator:
            continue
        for child in side.get_children():
            side.remove(child)
    populate_window(win_layout, state)

header_bar = Gtk.HeaderBar()
header_bar.set_show_close_button(True)
header_bar.props.title = "PiFmRds"

win = Gtk.Window()
win.set_default_size(500,400)
win.set_resizable(False)
win.set_titlebar(header_bar)

win_layout = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
win_left = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=50)
win_left.set_margin_top(20)
win_left.set_margin_start(50)
win_left.set_margin_end(50)
win_right = Gtk.Grid(column_spacing=20)
win_right.set_margin_top(20)
win_right.set_margin_start(50)
win_right.set_margin_end(50)
win_separator = Gtk.Separator(orientation=Gtk.Orientation.VERTICAL)
win_layout.add(win_left)
win_layout.add(win_separator)
win_layout.add(win_right)

service_state = is_service_running()
kill_switch = Gtk.Switch()
kill_switch.set_state(service_state)
kill_switch.connect("state-set", toggle_transmission, win_layout)
header_bar.pack_start(kill_switch)

populate_window(win_layout, service_state)

win.add(win_layout)
win.connect("destroy", Gtk.main_quit)
win.show_all()
Gtk.main()

