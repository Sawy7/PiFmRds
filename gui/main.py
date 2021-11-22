#!python
#cython: language_level=3

import gi, os, subprocess, re, time

gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk, Gio, GObject
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

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
        self.af_dialog_exists = False

    def setup_transmisson_object(self):
        self.transmission = Transmission(self)

    def setup_autords_object(self):
        self.autords = AutoRDS(self)

    def setup_rds_object(self):
        self.rds = RDS(self)
    
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
            if self.af_dialog_exists:
                self.af_dialog.win.close()
        self.window_hidden = not self.window_hidden

    def setup_history_modifications(self):
        self.file_handler = FileEventHandler(self)
        self.file_observer = Observer()
        
    def start_history_modifications(self):
        self.file_observer.schedule(self.file_handler, path="/tmp/rdshistory.txt", recursive=False)
        self.file_observer.start()

    def stop_history_modifications(self):
        if hasattr(self, "file_observer"):
            self.file_observer.stop()
            self.file_observer.join()

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
            self.populate_left_win()
            self.populate_right_win()
            # Setup watchdog for history file
            self.setup_history_modifications()
            self.start_history_modifications()
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
        # Setup station pi
        self.station_pi = self.get_station_pi()

        # Setup station name
        self.station_name = self.get_station_name()

        # Setup station text
        self.station_text = self.get_station_text()

        # Setup station PTY
        self.station_pty = self.get_station_pty()

        # Setup RDS button
        self.rds_button = self.get_rds_button()

        # Setup TA
        self.station_ta = self.get_station_ta()

        # Setup AF button
        self.af_button = self.get_af_button()

        # Lock/unlock manual RDS
        self.get_manual_rds_func()

        # Setup Auto-RDS switch
        self.get_auto_rds_switch()

    def get_station_pi(self):
        station_pi_label = Gtk.Label()
        station_pi_label.set_text("Station PI")
        station_pi = Gtk.Entry(name="station_pi")
        
        self.win_right.attach(station_pi_label, 0,0,1,1)
        self.win_right.attach(station_pi, 1,0,1,1)
        station_pi_label.show()
        station_pi.show()

        return station_pi

    def get_station_name(self):
        station_name_label = Gtk.Label()
        station_name_label.set_text("Station name")
        station_name = Gtk.Entry(name="station_name")
        
        station_name_label.set_margin_top(5)
        self.win_right.attach(station_name_label, 0,1,1,1)
        station_name.set_margin_top(5)
        self.win_right.attach(station_name, 1,1,1,1)
        station_name_label.show()
        station_name.show()

        return station_name

    def get_station_text(self):
        station_text_label = Gtk.Label()
        station_text_label.set_text("Station text")
        station_text = Gtk.Entry(name="station_text")
            
        station_text_label.set_margin_top(5)
        self.win_right.attach(station_text_label, 0,2,1,1)
        station_text.set_margin_top(5)
        self.win_right.attach(station_text, 1,2,1,1)
        station_text_label.show()
        station_text.show()

        return station_text

    def get_station_pty(self):
        station_pty_label = Gtk.Label()
        station_pty_label.set_text("Program Type")
        station_pty = Gtk.ComboBoxText(name="station_pty")
        # station_pty.set_name("station_pty")

        self.fill_station_pty(station_pty)
        station_pty.set_active(0)
            
        station_pty_label.set_margin_top(5)
        self.win_right.attach(station_pty_label, 0,3,1,1)
        station_pty.set_margin_top(5)
        self.win_right.attach(station_pty, 1,3,1,1)
        station_pty_label.show()
        station_pty.show()

        return station_pty

    def fill_station_pty(self, station_pty):
        station_pty.insert(0, "0", "Nothing")
        station_pty.insert(1, "1", "News")
        station_pty.insert(2, "2", "Current affairs")
        station_pty.insert(3, "3", "Information")
        station_pty.insert(4, "4", "Sport")
        station_pty.insert(5, "5", "Education")
        station_pty.insert(6, "6", "Drama")
        station_pty.insert(7, "7", "Culture")
        station_pty.insert(8, "8", "Science")
        station_pty.insert(9, "9", "Varied")
        station_pty.insert(10, "10", "Popular Music (Pop)")
        station_pty.insert(11, "11", "Rock Music")
        station_pty.insert(12, "12", "Easy Listening")
        station_pty.insert(13, "13", "Light Classical")
        station_pty.insert(14, "14", "Serious Classical")
        station_pty.insert(15, "15", "Other Music")
        station_pty.insert(16, "16", "Weather")
        station_pty.insert(17, "17", "Finance")
        station_pty.insert(18, "18", "Children's Programmes")
        station_pty.insert(19, "19", "Social Affairs")
        station_pty.insert(20, "20", "Religion")
        station_pty.insert(21, "21", "Phone-in")
        station_pty.insert(22, "22", "Travel")
        station_pty.insert(23, "23", "Leisure")
        station_pty.insert(24, "24", "Jazz Music")
        station_pty.insert(25, "25", "Country Music")
        station_pty.insert(26, "26", "National Music")
        station_pty.insert(27, "27", "Oldies Music")
        station_pty.insert(28, "28", "Folk Music")
        station_pty.insert(29, "29", "Documentary")
        station_pty.insert(30, "30", "Alarm Test")
        station_pty.insert(31, "31", "Alarm")

    def get_rds_button(self):
        rds_button = Gtk.Button(label="Change RDS")
        rds_button.set_margin_top(20)
        self.win_right.attach(rds_button, 0,4,2,1)
        rds_button.show()
        rds_button.grab_focus()

        return rds_button

    def get_station_ta(self):
        station_ta_label = Gtk.Label()
        station_ta_label.set_text("Traffic Ann.")
        station_ta_label.set_margin_top(20)
        self.win_right.attach(station_ta_label, 0,5,1,1)
        station_ta_label.show()

        station_ta = Gtk.Switch()
        station_ta.set_state(self.autords_state)
        station_ta.connect("state-set", self.rds.toggle_ta)
        station_ta.set_margin_top(20)
        self.win_right.attach(station_ta, 1,5,1,1)
        station_ta.show()

        return station_ta

    def get_af_button(self):
        af_button = Gtk.Button(label="Manage AFs")
        af_button.set_margin_top(20)
        self.win_right.attach(af_button, 0,6,2,1)
        af_button.show()
        af_button.grab_focus()
        af_button.connect("clicked", self.show_af_dialog)

        return af_button

    def show_af_dialog(self, button):
        self.af_dialog = AFDialog(self)
        self.af_dialog_exists = True

    def get_manual_rds_func(self):
        self.previous_rds_history = self.rds.get_history()
        self.station_pi.connect("changed", self.rds.rds_entry_changing, "station_pi")
        self.station_name.connect("changed", self.rds.rds_entry_changing, "station_name")
        self.station_pty.connect("changed", self.rds.rds_entry_changing, "station_pty")
        self.rds_button.connect("clicked", self.rds.change)

        self.station_pi.set_text(self.previous_rds_history["station_pi"])
        self.station_name.set_text(self.previous_rds_history["station_name"])
        self.station_text.set_text(self.previous_rds_history["station_text"])
        self.station_pty.set_active(int(self.previous_rds_history["station_pty"]))
        
        if self.autords_state:
            # self.disable_entry(station_name, "station_name")
            self.disable_entry(self.station_text, "station_text")
            self.station_text.set_text("<Auto>")
        else:
            self.station_text.connect("changed", self.rds.rds_entry_changing, "station_text")

    def repopulate_rds_entries(self):
        rds_history = self.rds.get_history()
        if rds_history == self.previous_rds_history:
            return
        
        self.station_name.set_text(rds_history["station_name"])
        self.rds.rds_entry_changing(self.station_pi, "station_pi")
        self.rds.rds_entry_changing(self.station_name, "station_name")
        self.rds.rds_entry_changing(self.station_pty, "station_pty")
        
        if not self.autords_state:
            self.station_text.set_text(rds_history["station_text"])
            self.rds.rds_entry_changing(self.station_text, "station_text")

        if self.af_dialog_exists:
            self.af_dialog.update_afbox(rds_history)

        self.previous_rds_history = rds_history

    def get_auto_rds_switch(self):
        rds_metadata_label = Gtk.Label()
        rds_metadata_label.set_text("Auto-RDS")
        rds_metadata_label.set_margin_top(20)
        self.win_right.attach(rds_metadata_label, 0,7,1,1)
        rds_metadata_label.show()

        rds_metadata = Gtk.Switch()
        rds_metadata.set_state(self.autords_state)
        rds_metadata.connect("state-set", self.autords.toggle)
        rds_metadata.set_margin_top(20)
        self.win_right.attach(rds_metadata, 1,7,1,1)
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
        self.stop_history_modifications()
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
                file_content += " -dbus"
            else:
                file_content = re.sub(" -dbus", f"", file_content)

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

    def change_frequency(self, button, freq_spin):
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
    def __init__(self, window):
        self.charlimits = {"station_name": 8, "station_text": 64}
        self.window = window
        
    def change(self, button):
        station_pi_new = self.window.station_pi.get_text()
        station_name_new = self.window.station_name.get_text()
        station_text_new = self.window.station_text.get_text()
        station_pty_new = self.window.station_pty.get_active()
        
        event_counter = 0

        if station_pi_new != "":
            event_counter += 1
        if station_name_new != "":
            event_counter += 1
        if station_text_new != "" and station_text_new != "<Auto>":
            event_counter += 1
        # if station_pty_new != "":
        #     event_counter += 1
        self.window.file_handler.ignore_events(event_counter-1)
            
        if station_pi_new != "":
            os.system(f'echo "PI {station_pi_new}" > /tmp/rdspipe')
        if station_name_new != "":
            os.system(f'echo "PS {station_name_new}" > /tmp/rdspipe')
            # station_name.set_text(station_name_new)
        if station_text_new != "" and station_text_new != "<Auto>":
            os.system(f'echo "RT {station_text_new}" > /tmp/rdspipe')
            # station_text.set_text(station_text_new)
        if station_pty_new != "":
            os.system(f'echo "PTY {station_pty_new}" > /tmp/rdspipe')

    def get_history(self):
        try:
            station_pi = ""
            station_name = ""
            station_text = ""
            station_pty = ""
            station_af = []
            psvar = False
            with open("/tmp/rdshistory.txt", "r") as f:
                content = f.readlines()
                for line in content:
                    field_text = line[3:-1].rstrip()
                    if "PI " in line:
                        station_pi = field_text
                    elif "PS " in line:
                        if not psvar:
                            station_name = field_text
                    elif "PSVAR" in line:
                        station_name = "<var>"
                        psvar = True
                    elif "RT " in line:
                        station_text = field_text
                    elif "PTY " in line:
                        field_text = field_text[1:]
                        station_pty = field_text
                    elif "AF " in line:
                        station_af = field_text.split(";")
                        station_af = [(int(af)+875)/10 for af in station_af]
            self.history = {"station_pi": station_pi, "station_name": station_name, "station_text": station_text, "station_pty": station_pty, "station_af": station_af}
            print(self.history)
            return self.history
        except:
            return None

    def rds_entry_changing(self, entry, entry_name):
        # Character limit
        try:
            charlimit = self.charlimits[entry_name]
            entry.set_text(entry.get_text()[:charlimit])
        except KeyError:
            pass # no charlimit
        # Colors
        style_provider = Gtk.CssProvider()
        if entry_name == "af_input":
            af = entry.get_text()
            try:
                af = float(af)
                bg_color = "#33D17A" if float(entry.get_text()) >= 87.6 and float(entry.get_text()) <= 107.9 else "#F57900"
            except:
                bg_color = "#F57900"
            css = "#" + entry_name + " { background: " +  bg_color + "; }"
        elif isinstance(entry, Gtk.Entry):
            bg_color = "#33D17A" if entry.get_text() == self.history[entry_name] else "#F57900"
            css = "#" + entry_name + " { background: " +  bg_color + "; }"
        else:
            bg_color = "#33D17A" if entry.get_active() == int(self.history[entry_name]) else "#F57900"
            css = "#" + entry_name + " button{ background: " +  bg_color + "; }"
        style_provider.load_from_data(bytes(css.encode()))
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(), style_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

    def toggle_ta(self, switch, state):
        option = "ON" if state else "OFF"
        self.window.file_handler.ignore_events(1)
        os.system(f'echo "TA {option}" > /tmp/rdspipe')

class FileEventHandler(FileSystemEventHandler):
    def __init__(self, window):
        self.window = window
        self.ignore = 0   

    # def on_any_event(self, event):
    #     print(event)
    
    def on_closed(self, event):
        if self.ignore == 0:
            self.window.repopulate_rds_entries()
        else:
            self.ignore -= 1

    def ignore_events(self, value):
        self.ignore += value

class AFDialog():
    def __init__(self, win):
        self.win = Gtk.Window()
        self.setup_window(300, 300)
        self.win.set_title("AF MGMT")
        self.mainwin = win
        self.create_layout()
        self.setup_afbox()
        self.setup_input()
        self.setup_add_button()
        self.setup_clear_button()
        self.update_afbox(self.mainwin.rds.get_history())
        self.win.show_all()

    def setup_window(self, width, height):
        self.win.set_default_size(width, height)
        # self.win.set_resizable(False)
        self.win.connect("destroy", self.on_close)

    def create_layout(self):
        self.win_layout = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)

        self.scrolled_win = Gtk.ScrolledWindow()
        self.scrolled_win.set_hexpand(True)
        self.scrolled_win.set_vexpand(True)

        self.win_bottom = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)

        self.win_layout.add(self.scrolled_win)
        self.win_layout.add(self.win_bottom)

        self.win.add(self.win_layout)

    def setup_afbox(self):
        self.afbox = Gtk.ListBox()
        self.af_store = Gio.ListStore()
        self.afbox.bind_model(self.af_store, self.generate_afbox_item)
        self.scrolled_win.add(self.afbox)
        # self.afbox.show_all()

    def generate_afbox_item(self, input):
        row = Gtk.ListBoxRow()

        title_label = Gtk.Label()
        title_label.set_text(input.frequency)
        row.add(title_label)
        
        return row

    def add_af_entry(self, af):
        self.af_store.append(AFRow(float(af)))
        self.afbox.show_all()

    def setup_input(self):
        self.input = Gtk.Entry(name="af_input")
        self.input.connect("changed", self.mainwin.rds.rds_entry_changing, "af_input")
        self.win_bottom.add(self.input)

    def setup_add_button(self):
        self.add_button = Gtk.Button(label="Add")
        self.add_button.connect("clicked", self.send_af)
        self.win_bottom.add(self.add_button)

    def setup_clear_button(self):
        self.clear_button = Gtk.Button(label="Clear")
        self.clear_button.connect("clicked", self.clear_af)
        self.win_bottom.add(self.clear_button)

    def send_af(self, button):
        af = self.input.get_text()
        af_float = 0
        try:
            af_float = float(af)
        except:
            return
        if float(af) >= 87.6 and float(af) <= 107.9 and len(self.af_store) < 25:
            self.mainwin.file_handler.ignore_events(1)
            os.system(f'echo "AF {af}" > /tmp/rdspipe')
            self.add_af_entry(af)
            self.input.set_text("")

    def clear_af(self, button):
        self.mainwin.previous_rds_history["station_af"] = None
        os.system(f'echo "AF CLEAR" > /tmp/rdspipe')

    def update_afbox(self, rds_history):
        self.af_store.remove_all()
        for af in rds_history["station_af"]:
            self.af_store.append(AFRow(af))
        # self.afbox.show_all()

    def on_close(self, win):
        self.mainwin.af_dialog_exists = False

class AFRow(GObject.GObject):
    frequency = GObject.Property(type = str)

    def __init__(self, frequency):
        GObject.GObject.__init__(self)
        self.frequency = frequency

if __name__ == "__main__":
    mainWin = Window(500, 400)
    mainWin.populate_window()
    Gtk.main()
