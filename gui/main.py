#!python
#cython: language_level=3

import gi, os, subprocess, re

gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk

# TODO: Make app fetch data periodically

class Window:
    def __init__(self, width, height):
        self.win = Gtk.Window()
        self.setup_window(width, height)
        self.create_layout()
        self.setup_transmisson_object()
        self.setup_autords_object()
        self.setup_rds_object()
        self.setup_headerbar()
        self.setup_statusicon()

    def setup_window(self, width, height):
        self.win.set_default_size(width, height)
        self.win.set_resizable(False)

    def setup_transmisson_object(self):
        self.transmission = Transmission(self)

    def setup_autords_object(self):
        self.autords = AutoRDS(self)

    def setup_rds_object(self):
        self.rds = RDS()
    
    def setup_headerbar(self):
        self.header_bar = Gtk.HeaderBar()
        self.header_bar.set_show_close_button(True)
        self.header_bar.props.title = "PiFmRds"
        self.win.set_titlebar(self.header_bar)

        self.service_state = self.transmission.get_state()
        kill_switch = Gtk.Switch()
        kill_switch.set_state(self.service_state)
        kill_switch.connect("state-set", self.transmission.toggle)
        self.header_bar.pack_start(kill_switch)

    def setup_statusicon(self):
        self.window_hidden = False
        self.statusicon = Gtk.StatusIcon()
        self.statusicon.set_from_file("/home/pi/sound-off-icon-40944.png") # TODO: Find/make image
        self.statusicon.connect('button-press-event', self.statusicon_react)

    def statusicon_react(self, *args): # TODO: Make a menu
        if self.window_hidden:
            self.win.deiconify()
            self.win.present()
        else:
            self.win.hide()
        self.window_hidden = not self.window_hidden

    def create_layout(self):
        self.win_layout = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        self.win_left = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=50)
        self.win_left.set_margin_top(20)
        self.win_left.set_margin_start(50)
        self.win_left.set_margin_end(50)
        self.win_right = Gtk.Grid(column_spacing=20)
        self.win_right.set_margin_top(20)
        self.win_right.set_margin_start(50)
        self.win_right.set_margin_end(50)
        self.win_separator = Gtk.Separator(orientation=Gtk.Orientation.VERTICAL)
        self.win_layout.add(self.win_left)
        self.win_layout.add(self.win_separator)
        self.win_layout.add(self.win_right)
    
    def populate_window(self):
        self.fmstatus_label = Gtk.Label()

        # Getting metadata state (if Auto-RDS is on)
        self.autords_state = self.autords.get_state()

        if self.service_state == True:
            # print("here")
            self.populate_left_win()
            self.populate_right_win()
        else:
            fmstatus = "Dead"
            self.fmstatus_label.set_text(fmstatus)
            self.win_left.add(self.fmstatus_label)
            self.fmstatus_label.show()

        self.win.add(self.win_layout)
        self.win.connect("destroy", Gtk.main_quit) # TODO: Make into tray
        self.win.connect("window-state-event", self.winevent_react)
        self.win.show_all()

    def winevent_react(self, window, event):
        if event.changed_mask & Gdk.WindowState.ICONIFIED:
            if event.new_window_state & Gdk.WindowState.ICONIFIED:
                self.statusicon_react()

    def populate_left_win(self):
        # Play icon
        header_image = Gtk.Image.new_from_icon_name("media-playback-start", Gtk.IconSize.DIALOG)
        self.win_left.add(header_image)
        header_image.show()

        # Getting FM status ("Running in this case")
        self.get_fm_status()

        # Getting current freq
        freq_spin = self.get_current_freq()

        # Setup freq button
        self.get_freq_button(freq_spin)

    def get_fm_status(self):
        fmstatus = "Running"
        self.fmstatus_label.set_text(fmstatus)
        self.win_left.add(self.fmstatus_label)
        self.fmstatus_label.show()

    def get_current_freq(self):
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
        freq_spin.connect("value-changed", self.transmission.freq_entry_changing, freq)
        self.win_left.add(freq_spin)
        freq_spin.show()
        return freq_spin

    def get_freq_button(self, freq_spin):
        freq_button = Gtk.Button(label="Change frequency")
        freq_button.connect("clicked", self.transmission.change_frequency, freq_spin)
        self.win_left.add(freq_button)
        freq_button.show()

    def populate_right_win(self):
        # TODO: Limit characters of these two
        # Setup station name
        station_name = self.get_station_name()

        # Setup station text
        station_text = self.get_station_text()

        # Setup RDS button
        rds_button = self.get_rds_button()

        # Lock/unlock manual RDS
        self.get_manual_rds_func(station_name, station_text, rds_button)

        # Setup Auto-RDS switch
        self.get_auto_rds_switch()

    def get_station_name(self):
        station_name_label = Gtk.Label()
        station_name_label.set_text("Station name")
        station_name = Gtk.Entry(name="station_name")
        
        self.win_right.attach(station_name_label, 0,0,1,1)
        self.win_right.attach(station_name, 1,0,1,1)
        station_name_label.show()
        station_name.show()

        return station_name

    def get_station_text(self):
        station_text_label = Gtk.Label()
        station_text_label.set_text("Station text")
        station_text = Gtk.Entry(name="station_text")
            
        station_text_label.set_margin_top(5)
        
        self.win_right.attach(station_text_label, 0,1,1,1)
        station_text.set_margin_top(5)
        self.win_right.attach(station_text, 1,1,1,1)
        station_text_label.show()
        station_text.show()

        return station_text

    def get_rds_button(self):
        rds_button = Gtk.Button(label="Change RDS")
        rds_button.set_margin_top(20)
        self.win_right.attach(rds_button, 0,2,2,1)
        rds_button.show()
        rds_button.grab_focus()

        return rds_button

    def get_manual_rds_func(self, station_name, station_text, rds_button):
        if self.autords_state:
            self.disable_entry(station_name, "station_name")
            self.disable_entry(station_text, "station_text")
        else:
            rds_history = self.rds.get_history()
            station_name.connect("changed", self.rds.rds_entry_changing, "station_name", rds_history["station_name"])
            station_text.connect("changed", self.rds.rds_entry_changing, "station_text", rds_history["station_text"])
            rds_button.connect("clicked", self.rds.change, station_name, station_text)

            station_name.set_text(rds_history["station_name"])
            station_text.set_text(rds_history["station_text"])

    def get_auto_rds_switch(self):
        rds_metadata_label = Gtk.Label()
        rds_metadata_label.set_text("Auto-RDS")
        rds_metadata_label.set_margin_top(20)
        self.win_right.attach(rds_metadata_label, 0,3,1,1)
        rds_metadata_label.show()

        rds_metadata = Gtk.Switch()
        rds_metadata.set_state(self.autords_state)
        rds_metadata.connect("state-set", self.autords.toggle)
        rds_metadata.set_margin_top(20)
        self.win_right.attach(rds_metadata, 1,3,1,1)
        rds_metadata.show()

    def disable_entry(self, entry, entry_name):
        entry.set_editable(False)
        style_provider = Gtk.CssProvider()
        bg_color = "#86929A"
        css = "#" + entry_name + "{ background:" +  bg_color + "; }"
        style_provider.load_from_data(bytes(css.encode()))
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(), style_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

    def reset(self):
        for side in self.win_layout.get_children():
            if type(side) == Gtk.Separator:
                continue
            for child in side.get_children():
                side.remove(child)
        self.win.remove(self.win_layout)
        self.service_state = self.transmission.get_state()
        self.populate_window()

class AutoRDS:
    def __init__(self, window):
        self.window = window

    def get_state(self):
        file_content = None
        with open("/usr/bin/pi_fm_runner", "r") as f:
            file_content = f.read()
        if re.search("-dbus", file_content) is not None:
            return True
        else:
            return False

    def toggle(self, switch, state):
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
        self.window.reset()

class Transmission:
    def __init__(self, window):
        self.window = window

    def toggle(self, switch, state):
        if state == True:
            os.system("systemctl --user start pi_fm_rds")
        else:
            os.system("systemctl --user stop pi_fm_rds")
        self.window.reset()

    def get_state(self):
        fmstatus = os.system("systemctl --user is-active --quiet pi_fm_rds")
        if fmstatus == 0:
            return True
        else:
            return False

    def change_frequency(self, freq_spin):
        file_content = None
        with open("/usr/bin/pi_fm_runner", "r") as f:
            file_content = f.read()
            new_freq = freq_spin.get_text().replace(",", ".")
            file_content = re.sub("-freq \d*.\d*", f"-freq {new_freq}", file_content)

        with open("/usr/bin/pi_fm_runner", "w") as f:
            f.write(file_content)

        os.system("systemctl --user restart pi_fm_rds")
        self.window.reset()

    def freq_entry_changing(self, spinbutton, freq):
        style_provider = Gtk.CssProvider()
        bg_color = "#33D17A" if spinbutton.get_text().replace(",", ".") == freq else "#F57900"
        css = "#freq_spin { background:" +  bg_color + "; }"
        style_provider.load_from_data(bytes(css.encode()))
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(), style_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

class RDS:
    def __init__(self):
        self.history = self.get_history()

    def change(self, button, station_name, station_text):
        station_name_new = station_name.get_text()
        if station_name_new != "":
            os.system(f"echo 'PS {station_name_new}' > /tmp/rdspipe")
            self.rds_history["station_name"] = station_name_new
            self.rds_entry_changing(station_name, "station_name", station_name_new)
        station_text_new = station_text.get_text()
        if station_text_new != "":
            os.system(f"echo 'RT {station_text_new}' > /tmp/rdspipe")
            self.rds_history["station_text"] = station_text_new
            self.rds_entry_changing(station_text, "station_text", station_text_new)

    def get_history(self):
        with open("/tmp/rdshistory.txt", "r") as f:
            content = f.readlines()
            for line in content:
                field_text = line[3:-1]
                if "PS" in line:
                    station_name = field_text
                elif "RT" in line:
                    station_text = field_text
        return {"station_name": station_name, "station_text": station_text}

    def rds_entry_changing(self, entry, entry_name, history_value):
        style_provider = Gtk.CssProvider()
        bg_color = "#33D17A" if entry.get_text() == history_value else "#F57900"
        css = "#" + entry_name + "{ background:" +  bg_color + "; }"
        style_provider.load_from_data(bytes(css.encode()))
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(), style_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

if __name__ == "__main__":
    mainWin = Window(500, 400)
    mainWin.populate_window()
    Gtk.main()
