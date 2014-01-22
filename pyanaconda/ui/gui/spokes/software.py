# Software selection spoke classes
#
# Copyright (C) 2011-2013  Red Hat, Inc.
#
# This copyrighted material is made available to anyone wishing to use,
# modify, copy, or redistribute it subject to the terms and conditions of
# the GNU General Public License v.2, or (at your option) any later version.
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY expressed or implied, including the implied warranties of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.  You should have received a copy of the
# GNU General Public License along with this program; if not, write to the
# Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.  Any Red Hat trademarks that are incorporated in the
# source code or documentation are not subject to the GNU General Public
# License and may only be used or replicated with the express permission of
# Red Hat, Inc.
#
# Red Hat Author(s): Chris Lumens <clumens@redhat.com>
#

from gi.repository import Gtk, Pango

from pyanaconda.flags import flags
from pyanaconda.i18n import _, C_, CN_
from pyanaconda.packaging import MetadataError
from pyanaconda.threads import threadMgr, AnacondaThread
from pyanaconda import constants

from pyanaconda.ui.communication import hubQ
from pyanaconda.ui.gui.spokes import NormalSpoke
from pyanaconda.ui.gui.spokes.lib.detailederror import DetailedErrorDialog
from pyanaconda.ui.gui.utils import enlightbox, gtk_action_wait, escape_markup
from pyanaconda.ui.gui.categories.software import SoftwareCategory

import sys

__all__ = ["SoftwareSelectionSpoke"]

class SoftwareSelectionSpoke(NormalSpoke):
    builderObjects = ["addonStore", "environmentStore", "softwareWindow"]
    mainWidgetName = "softwareWindow"
    uiFile = "spokes/software.glade"

    category = SoftwareCategory

    icon = "package-x-generic-symbolic"
    title = CN_("GUI|Spoke", "_SOFTWARE SELECTION")

    # Add-on selection states
    # no user interaction with this add-on
    _ADDON_DEFAULT = 0
    # user selected
    _ADDON_SELECTED = 1
    # user de-selected
    _ADDON_DESELECTED = 2

    def __init__(self, *args, **kwargs):
        NormalSpoke.__init__(self, *args, **kwargs)
        self._errorMsgs = None
        self._tx_id = None
        self._selectFlag = False

        self.selectedGroups = []
        self.excludedGroups = []
        self.environment = None

        self._environmentListBox = self.builder.get_object("environmentListBox")
        self._addonListBox = self.builder.get_object("addonListBox")

        # Used to determine which add-ons to display for each environment.
        # The dictionary keys are environment IDs. The dictionary values are two-tuples
        # consisting of lists of add-on group IDs. The first list is the add-ons specific
        # to the environment, and the second list is the other add-ons possible for the
        # environment.
        self._environmentAddons = {}

        # Used to store how the user has interacted with add-ons for the default add-on
        # selection logic. The dictionary keys are group IDs, and the values are selection
        # state constants. See refreshAddons for how the values are used.
        self._addonStates = {}

        # Used for detecting whether anything's changed in the spoke.
        self._origAddons = []
        self._origEnvironment = None

    def _apply(self):
        env = self._get_selected_environment()
        if not env:
            return

        addons = self._get_selected_addons()
        for group in addons:
            if group not in self.selectedGroups:
                self.selectedGroups.append(group)

        self._selectFlag = False
        self.payload.data.packages.groupList = []
        self.payload.selectEnvironment(env)
        self.environment = env
        for group in self.selectedGroups:
            self.payload.selectGroup(group)

        # And then save these values so we can check next time.
        self._origAddons = addons
        self._origEnvironment = self.environment

        hubQ.send_not_ready(self.__class__.__name__)
        hubQ.send_not_ready("SourceSpoke")
        threadMgr.add(AnacondaThread(name=constants.THREAD_CHECK_SOFTWARE,
                                     target=self.checkSoftwareSelection))

    def apply(self):
        self._apply()
        self.data.packages.seen = True

    def checkSoftwareSelection(self):
        from pyanaconda.packaging import DependencyError
        hubQ.send_message(self.__class__.__name__, _("Checking software dependencies..."))
        try:
            self.payload.checkSoftwareSelection()
        except DependencyError as e:
            self._errorMsgs = "\n".join(sorted(e.message))
            hubQ.send_message(self.__class__.__name__, _("Error checking software dependencies"))
            self._tx_id = None
        else:
            self._errorMsgs = None
            self._tx_id = self.payload.txID
        finally:
            hubQ.send_ready(self.__class__.__name__, False)
            hubQ.send_ready("SourceSpoke", False)

    @property
    def completed(self):
        processingDone = not threadMgr.get(constants.THREAD_CHECK_SOFTWARE) and \
                         not self._errorMsgs and self.txid_valid

        if flags.automatedInstall:
            return processingDone and self.data.packages.seen
        else:
            return self._get_selected_environment() is not None and processingDone

    @property
    def changed(self):
        env = self._get_selected_environment()
        if not env:
            return True

        addons = self._get_selected_addons()

        # Don't redo dep solving if nothing's changed.
        if env == self._origEnvironment and set(addons) == set(self._origAddons) and \
           self.txid_valid:
            return False

        return True

    @property
    def mandatory(self):
        return True

    @property
    def ready(self):
        # By default, the software selection spoke is not ready.  We have to
        # wait until the installation source spoke is completed.  This could be
        # because the user filled something out, or because we're done fetching
        # repo metadata from the mirror list, or we detected a DVD/CD.

        return bool(not threadMgr.get(constants.THREAD_SOFTWARE_WATCHER) and
                    not threadMgr.get(constants.THREAD_PAYLOAD_MD) and
                    not threadMgr.get(constants.THREAD_CHECK_SOFTWARE) and
                    self.payload.baseRepo is not None)

    @property
    def showable(self):
        return not flags.livecdInstall and not self.data.method.method == "liveimg"

    @property
    def status(self):
        if self._errorMsgs:
            return _("Error checking software selection")

        if not self.ready:
            return _("Installation source not set up")

        if not self.txid_valid:
            return _("Source changed - please verify")

        env = self._get_selected_environment()
        if not env:
            # Kickstart installs with %packages will have a row selected, unless
            # they did an install without a desktop environment.  This should
            # catch that one case.
            if flags.automatedInstall and self.data.packages.seen:
                return _("Custom software selected")

            return _("Nothing selected")

        return self.payload.environmentDescription(env)[0]

    def initialize(self):
        NormalSpoke.initialize(self)
        threadMgr.add(AnacondaThread(name=constants.THREAD_SOFTWARE_WATCHER,
                      target=self._initialize))

    def _initialize(self):
        hubQ.send_message(self.__class__.__name__, _("Downloading package metadata..."))

        threadMgr.wait(constants.THREAD_PAYLOAD)

        hubQ.send_message(self.__class__.__name__, _("Downloading group metadata..."))

        # we have no way to select environments with kickstart right now
        # so don't try.
        if flags.automatedInstall and self.data.packages.seen:
            # We don't want to do a full refresh, just join the metadata thread
            threadMgr.wait(constants.THREAD_PAYLOAD_MD)
        else:
            # Grabbing the list of groups could potentially take a long time
            # at first (yum does a lot of magic property stuff, some of which
            # involves side effects like network access.  We need to reference
            # them here, outside of the main thread, to not block the UI.
            try:
                # pylint: disable-msg=W0104
                self.payload.environments
                # pylint: disable-msg=W0104
                self.payload.groups

                # Parse the environments and groups into the form we want
                self._parseEnvironments()
            except MetadataError:
                hubQ.send_message(self.__class__.__name__,
                                  _("No installation source available"))
                return

            # And then having done all that slow downloading, we need to do the first
            # refresh of the UI here so there's an environment selected by default.
            # This happens inside the main thread by necessity.  We can't do anything
            # that takes any real amount of time, or it'll block the UI from updating.
            if not self._first_refresh():
                return

        self.payload.release()

        hubQ.send_ready(self.__class__.__name__, False)

        # If packages were provided by an input kickstart file (or some other means),
        # we should do dependency solving here.
        self._apply()

    def _parseEnvironments(self):
        self._environmentAddons = {}

        for environment in self.payload.environments:
            self._environmentAddons[environment] = ([], [])

            # Determine which groups are specific to this environment and which other groups
            # are available in this environment.
            for grp in self.payload.groups:
                if self.payload.environmentHasOption(environment, grp):
                    self._environmentAddons[environment][0].append(grp)
                elif self.payload._isGroupVisible(grp) and self.payload._groupHasInstallableMembers(grp):
                    self._environmentAddons[environment][1].append(grp)

        # Set all of the add-on selection states to the default
        self._addonStates = {}
        for grp in self.payload.groups:
            self._addonStates[grp] = self._ADDON_DEFAULT

    @gtk_action_wait
    def _first_refresh(self):
        try:
            self.refresh()
            return True
        except MetadataError:
            hubQ.send_message(self.__class__.__name__, _("No installation source available"))
            return False

    def _add_row(self, listbox, name, desc, button):
        row = Gtk.ListBoxRow()
        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        box.set_spacing(6)

        button.set_valign(Gtk.Align.START)
        button.connect("clicked", self.on_button_toggled, row)
        box.add(button)

        label = Gtk.Label()
        label.set_line_wrap(True)
        label.set_line_wrap_mode(Pango.WrapMode.WORD_CHAR)
        label.set_markup("<b>%s</b>\n%s" % (escape_markup(name), escape_markup(desc)))
        label.set_hexpand(True)
        label.set_alignment(0, 0.5)
        box.add(label)

        row.add(box)
        listbox.insert(row, -1)

    def refresh(self):
        NormalSpoke.refresh(self)

        threadMgr.wait(constants.THREAD_PAYLOAD_MD)

        if self.environment not in self.payload.environments:
            self.environment = None

        firstEnvironment = True
        firstRadio = None

        self._clear_listbox(self._environmentListBox)

        for environment in self.payload.environments:
            (name, desc) = self.payload.environmentDescription(environment)

            radio = Gtk.RadioButton(group=firstRadio)

            active = environment == self.environment or \
                     not self.environment and firstEnvironment
            radio.set_active(active)
            if active:
                self.environment = environment

            self._add_row(self._environmentListBox, name, desc, radio)
            firstRadio = firstRadio or radio

            firstEnvironment = False

        self.refreshAddons()

    def _addAddon(self, grp):
        (name, desc) = self.payload.groupDescription(grp)

        # If the add-on was previously selected by the user, select it
        if self._addonStates[grp] == self._ADDON_SELECTED:
            selected = True
        # If the add-on was previously de-selected by the user, de-select it
        elif self._addonStates[grp] == self._ADDON_DESELECTED:
            selected = False
        # Otherwise, use the default state
        else:
            selected = self.payload.environmentOptionIsDefault(self.environment, grp)

        check = Gtk.CheckButton()
        check.set_active(selected)
        self._add_row(self._addonListBox, name, desc, check)

    def refreshAddons(self):
        # The source was changed, make sure the list is current
        if not self.txid_valid:
            self._parseEnvironments()

        if self.environment and (self.environment in self._environmentAddons):
            self._clear_listbox(self._addonListBox)

            # We have two lists:  One of addons specific to this environment,
            # and one of all the others.  The environment-specific ones will be displayed
            # first and then a separator, and then the generic ones.  This is to make it
            # a little more obvious that the thing on the left side of the screen and the
            # thing on the right side of the screen are related.
            #
            # If a particular add-on was previously selected or de-selected by the user, that
            # state will be used. Otherwise, the add-on will be selected if it is a default
            # for this environment.

            addSep = len(self._environmentAddons[self.environment][0]) > 0 and \
                     len(self._environmentAddons[self.environment][1]) > 0

            for grp in self._environmentAddons[self.environment][0]:
                self._addAddon(grp)

            # This marks a separator in the view - only add it if there's both environment
            # specific and generic addons.
            if addSep:
                self._addonListBox.insert(Gtk.Separator(), -1)

            for grp in self._environmentAddons[self.environment][1]:
                self._addAddon(grp)

        self._selectFlag = True

        if self._errorMsgs:
            self.set_warning(_("Error checking software dependencies.  Click for details."))
        else:
            self.clear_info()

    def _allAddons(self):
        return self._environmentAddons[self.environment][0] + \
               [""] + \
               self._environmentAddons[self.environment][1]

    def _get_selected_addons(self):
        retval = []

        addons = self._allAddons()

        for (ndx, row) in enumerate(self._addonListBox.get_children()):
            box = row.get_children()[0]

            if isinstance(box, Gtk.Separator):
                continue

            button = box.get_children()[0]
            if button.get_active():
                retval.append(addons[ndx])

        return retval

    # Returns the row in the store corresponding to what's selected on the
    # left hand panel, or None if nothing's selected.
    def _get_selected_environment(self):
        for (ndx, row) in enumerate(self._environmentListBox.get_children()):
            box = row.get_children()[0]
            button = box.get_children()[0]
            if button.get_active():
                return self.payload.environments[ndx]

        return None

    def _clear_listbox(self, listbox):
        for child in listbox.get_children():
            listbox.remove(child)
            del(child)

    @property
    def txid_valid(self):
        return self._tx_id == self.payload.txID

    # Signal handlers
    def on_button_toggled(self, radio, row):
        row.activate()

    def on_environment_activated(self, listbox, row):
        if not self._selectFlag:
            return

        box = row.get_children()[0]
        button = box.get_children()[0]

        button.handler_block_by_func(self.on_button_toggled)
        button.set_active(not button.get_active())
        button.handler_unblock_by_func(self.on_button_toggled)

        # Remove all the groups that were selected by the previously
        # selected environment.
        for groupid in self.payload.environmentGroups(self.environment):
            if groupid in self.selectedGroups:
                self.selectedGroups.remove(groupid)

        # Then mark the clicked environment as selected and update the screen.
        self.environment = self.payload.environments[row.get_index()]
        self.refreshAddons()
        self._addonListBox.show_all()

    def on_addon_activated(self, listbox, row):
        box = row.get_children()[0]
        if isinstance(box, Gtk.Separator):
            return

        button = box.get_children()[0]
        addons = self._allAddons()
        group = addons[row.get_index()]

        wasActive = group in self.selectedGroups

        button.handler_block_by_func(self.on_button_toggled)
        button.set_active(not wasActive)
        button.handler_unblock_by_func(self.on_button_toggled)

        if wasActive:
            self.selectedGroups.remove(group)
            self._addonStates[group] = self._ADDON_DESELECTED
        else:
            self.selectedGroups.append(group)

            if group in self.excludedGroups:
                self.excludedGroups.remove(group)

            self._addonStates[group] = self._ADDON_SELECTED

    def on_info_bar_clicked(self, *args):
        if not self._errorMsgs:
            return

        label = _("The following software marked for installation has errors.  "
                  "This is likely caused by an error with\nyour installation source.  "
                  "You can change your installation source or quit the installer.")
        dialog = DetailedErrorDialog(self.data,
                buttons=[C_("GUI|Software Selection|Error Dialog", "_Quit"),
                         C_("GUI|Software Selection|Error Dialog", "_Cancel"),
                         C_("GUI|Software Selection|Error Dialog", "_Modify Software Source")],
                label=label)
        with enlightbox(self.window, dialog.window):
            dialog.refresh(self._errorMsgs)
            rc = dialog.run()

        dialog.window.destroy()

        if rc == 0:
            # Quit.
            sys.exit(0)
        elif rc == 1:
            # Close the dialog so the user can change selections.
            pass
        elif rc == 2:
            # Send the user to the installation source spoke.
            self.skipTo = "SourceSpoke"
            self.window.emit("button-clicked")
        else:
            pass
