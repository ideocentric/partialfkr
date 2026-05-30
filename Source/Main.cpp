// SPDX-License-Identifier: AGPL-3.0-or-later
#include "UI/MainComponent.h"
#include "UI/CommandIDs.h"
#include "UI/MacWindowHelpers.h"
#include "UI/AboutComponent.h"
#include "UI/PartialFKRLookAndFeel.h"
#if JUCE_DEBUG
#include "Dev/ScreenshotHelper.h"
#endif

#include <JuceHeader.h>

class PartialFKRApplication : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& /*commandLineParameters*/) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

        // Register app-level commands (fileNew) so the command manager knows about them.
        // MainComponent registers its own commands when it becomes the first target.
        commandManager.registerAllCommandsForTarget(this);
        commandManager.setFirstCommandTarget(nullptr);

        // Create the first window before setMacMainMenu so that when menuBarItemsChanged
        // fires during setMacMainMenu, activeComponent and command registrations are ready.
        createNewWindow();
#if JUCE_MAC
        {
            juce::PopupMenu appleItems;
            appleItems.addCommandItem(&commandManager, CommandIDs::appAbout);
            juce::MenuBarModel::setMacMainMenu(&appMenu, &appleItems);
        }
#endif

#if JUCE_DEBUG
        // Screenshot mode: PartialFKR --screenshots /path/to/sample.pfkr [outDir]
        if (ScreenshotHelper::isRequested(getCommandLineParameters()))
        {
            juce::File pfkrFile, outDir;
            ScreenshotHelper::parseArgs(getCommandLineParameters(), pfkrFile, outDir);
            if (pfkrFile.existsAsFile())
            {
                shotHelper = std::make_unique<ScreenshotHelper>(
                    *windows.back(), commandManager);
                shotHelper->start(pfkrFile, outDir);
            }
        }
#endif
    }

    void shutdown() override
    {
#if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu(nullptr);
#endif
        windows.clear();
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    }

    void systemRequestedQuit() override
    {
        isQuitting = true;
        tryQuit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        // On macOS, JUCE routes application:openFile: here with a (possibly quoted) path.
        // Strip quotes and check for a .pfkr file.
        const juce::String path = commandLine.trim().unquoted();
        const juce::File   file(path);

        if (!file.existsAsFile() || !file.hasFileExtension("pfkr"))
            return;

        // Use the frontmost window if it has no partials loaded, otherwise open a new one.
        MainWindow* target = nullptr;
        if (auto* w = getActiveMainWindow())
        {
            if (auto* mc = dynamic_cast<MainComponent*>(w->getContentComponent()))
                if (!mc->hasPartials())
                    target = w;
        }

        if (target == nullptr)
        {
            createNewWindow();
            target = windows.back().get();
        }

        if (auto* mc = dynamic_cast<MainComponent*>(target->getContentComponent()))
            mc->loadProject(file);
    }

    // ── Application-level command target (fileNew only) ───────────────────────
    // JUCEApplication already inherits ApplicationCommandTarget; implement here.

    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }

    void getAllCommands(juce::Array<juce::CommandID>& commands) override
    {
        commands.add(CommandIDs::fileNew);
        commands.add(CommandIDs::fileOpenProject);
        commands.add(CommandIDs::windowMinimize);
        commands.add(CommandIDs::windowZoom);
        commands.add(CommandIDs::windowBringAllToFront);
        commands.add(CommandIDs::appAbout);
        commands.add(CommandIDs::appHelp);
    }

    void getCommandInfo(juce::CommandID commandID,
                        juce::ApplicationCommandInfo& result) override
    {
        const bool hasWindow = !windows.empty();

        if (commandID == CommandIDs::fileNew)
        {
            result.setInfo("New", "Open a new empty project window", "File", 0);
            result.addDefaultKeypress('n', juce::ModifierKeys::commandModifier);
        }
        else if (commandID == CommandIDs::fileOpenProject)
        {
            result.setInfo("Open...", "Open a .pfkr project file", "File", 0);
            result.addDefaultKeypress('o', juce::ModifierKeys::commandModifier);
        }
        else if (commandID == CommandIDs::windowMinimize)
        {
            result.setInfo("Minimize", "Minimize the window", "Window", 0);
            result.addDefaultKeypress('m', juce::ModifierKeys::commandModifier);
            result.setActive(hasWindow);
        }
        else if (commandID == CommandIDs::windowZoom)
        {
            result.setInfo("Zoom", "Zoom the window", "Window", 0);
            result.setActive(hasWindow);
        }
        else if (commandID == CommandIDs::windowBringAllToFront)
        {
            result.setInfo("Bring All to Front", "Bring all windows to the front", "Window", 0);
            result.setActive(hasWindow);
        }
        else if (commandID == CommandIDs::appAbout)
        {
            result.setInfo("About PartialFKR", "Show application information", "Application", 0);
        }
        else if (commandID == CommandIDs::appHelp)
        {
            result.setInfo("PartialFKR Help", "Open the help documentation", "Application", 0);
        }
    }

    bool perform(const juce::ApplicationCommandTarget::InvocationInfo& info) override
    {
        if (info.commandID == CommandIDs::fileNew)
        {
            createNewWindow();
            return true;
        }
        if (info.commandID == CommandIDs::fileOpenProject)
        {
            openProjectDialog();
            return true;
        }
        if (info.commandID == CommandIDs::windowMinimize)
        {
            if (auto* w = getActiveMainWindow())
                if (auto* peer = w->getPeer())
                    peer->setMinimised(true);
            return true;
        }
        if (info.commandID == CommandIDs::windowZoom)
        {
            MacWindowHelpers::zoom(getActiveMainWindow());
            return true;
        }
        if (info.commandID == CommandIDs::windowBringAllToFront)
        {
            MacWindowHelpers::bringAllToFront();
            return true;
        }
        if (info.commandID == CommandIDs::appAbout)
        {
            AboutComponent::show(getActiveMainWindow());
            return true;
        }
        if (info.commandID == CommandIDs::appHelp)
        {
            juce::URL("https://github.com/ideocentric/partialfkr").launchInDefaultBrowser();
            return true;
        }
        return false;
    }

    void openProjectDialog()
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Open project", juce::File{}, "*.pfkr");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                const auto results = fc.getResults();
                if (results.isEmpty()) return;
                createNewWindow();
                if (auto* mc = dynamic_cast<MainComponent*>(
                        windows.back()->getContentComponent()))
                    mc->loadProject(results[0]);
            });
    }

    // ── App-level menu bar model ──────────────────────────────────────────────
    //
    // Lives at application scope so the menu bar persists with no windows open.
    // Menu items are built with addCommandItem so JUCE sets native NSMenuItem
    // key equivalents and greyed states automatically.

    struct AppMenuBarModel : public juce::MenuBarModel {
        juce::ApplicationCommandManager& cm;
        MainComponent* activeComponent = nullptr;

        explicit AppMenuBarModel(juce::ApplicationCommandManager& c) : cm(c) {}

        juce::StringArray getMenuBarNames() override { return { "File", "Edit", "View", "Transport", "Window", "Help" }; }

        juce::PopupMenu getMenuForIndex(int index,
                                        const juce::String& /*name*/) override
        {
            // Helper: add a command item with an explicit enable state while still
            // displaying the registered key equivalent from the command manager.
            auto addItem = [&](juce::PopupMenu& menu,
                               juce::CommandID commandID,
                               bool isEnabled)
            {
                if (auto* info = cm.getCommandForID(commandID))
                {
                    juce::PopupMenu::Item item;
                    item.itemID         = (int) commandID;
                    item.text           = info->shortName;
                    item.isEnabled      = isEnabled;
                    item.commandManager = &cm;
                    menu.addItem(item);
                }
            };

            const bool hasParts   = activeComponent != nullptr && activeComponent->hasPartials();
            const bool hasSel     = activeComponent != nullptr && activeComponent->hasSelection();
            const bool hasClip    = activeComponent != nullptr && activeComponent->hasClipboard();
            const bool doUndo     = activeComponent != nullptr && activeComponent->canUndo();
            const bool doRedo     = activeComponent != nullptr && activeComponent->canRedo();
            const bool noAudio    = activeComponent == nullptr || !activeComponent->hasSourceAudio();
            const bool notNorm    = activeComponent == nullptr || !activeComponent->getIsNormalizing();
            const bool hasFadeRange = activeComponent != nullptr && activeComponent->hasValidFadeRange();
            const bool canSave    = hasParts || (activeComponent != nullptr
                                                 && activeComponent->getCurrentFile().existsAsFile());

            if (index == 0)  // File
            {
                juce::PopupMenu file;
                file.addCommandItem(&cm, CommandIDs::fileNew);
                file.addCommandItem(&cm, CommandIDs::fileOpenProject);
                file.addSeparator();
                addItem(file, CommandIDs::fileAnalyzeAudio, noAudio);
                file.addSeparator();
                addItem(file, CommandIDs::fileSave,   canSave);
                addItem(file, CommandIDs::fileSaveAs, hasParts);
                file.addSeparator();
                file.addCommandItem(&cm, CommandIDs::fileClose);
                file.addSeparator();

                juce::PopupMenu exportSub;
                exportSub.addCommandItem(&cm, CommandIDs::exportAudio);
                exportSub.addSeparator();
                exportSub.addCommandItem(&cm, CommandIDs::exportMidi);
                exportSub.addCommandItem(&cm, CommandIDs::exportMidiPackage);
                exportSub.addSeparator();
                exportSub.addCommandItem(&cm, CommandIDs::exportCsound);
                exportSub.addCommandItem(&cm, CommandIDs::exportSuperCollider);
                exportSub.addCommandItem(&cm, CommandIDs::exportSdif);
                exportSub.addCommandItem(&cm, CommandIDs::exportJson);
                file.addSubMenu("Export", exportSub);

                return file;
            }

            if (index == 2)  // View
            {
                juce::PopupMenu view;
                view.addCommandItem(&cm, CommandIDs::viewZoomIn);
                view.addCommandItem(&cm, CommandIDs::viewZoomOut);
                view.addCommandItem(&cm, CommandIDs::viewZoomFit);
                view.addSeparator();
                view.addCommandItem(&cm, CommandIDs::viewTogglePanel);
                return view;
            }

            if (index == 5)  // Help
            {
                juce::PopupMenu help;
                help.addCommandItem(&cm, CommandIDs::appHelp);
#if !JUCE_MAC
                help.addSeparator();
                help.addCommandItem(&cm, CommandIDs::appAbout);
#endif
                return help;
            }

            if (index == 4)  // Window
            {
                juce::PopupMenu window;
                window.addCommandItem(&cm, CommandIDs::windowMinimize);
                window.addCommandItem(&cm, CommandIDs::windowZoom);
#if JUCE_MAC
                window.addSeparator();
                window.addCommandItem(&cm, CommandIDs::windowBringAllToFront);
#endif
                return window;
            }

            if (index == 3)  // Transport
            {
                juce::PopupMenu transport;
                transport.addCommandItem(&cm, CommandIDs::transportPlayPause);
                transport.addCommandItem(&cm, CommandIDs::transportStop);
                transport.addSeparator();
                transport.addCommandItem(&cm, CommandIDs::transportLoop);
                transport.addSeparator();
                transport.addCommandItem(&cm, CommandIDs::transportSetInPoint);
                transport.addCommandItem(&cm, CommandIDs::transportSetOutPoint);
                transport.addCommandItem(&cm, CommandIDs::transportSetInOutFromSel);
                return transport;
            }

            // Edit (index == 1)
            juce::PopupMenu edit;
            addItem(edit, juce::StandardApplicationCommandIDs::undo,      doUndo);
            addItem(edit, juce::StandardApplicationCommandIDs::redo,      doRedo);
            edit.addSeparator();
            addItem(edit, juce::StandardApplicationCommandIDs::cut,       hasSel);
            addItem(edit, juce::StandardApplicationCommandIDs::copy,      hasSel);
            addItem(edit, juce::StandardApplicationCommandIDs::paste,     hasClip);
            addItem(edit, juce::StandardApplicationCommandIDs::del,       hasSel);
            edit.addSeparator();
            edit.addCommandItem(&cm, CommandIDs::editStretch);
            edit.addSeparator();
            addItem(edit, juce::StandardApplicationCommandIDs::selectAll,   hasParts);
            addItem(edit, juce::StandardApplicationCommandIDs::deselectAll, hasSel);
            addItem(edit, CommandIDs::invertSelection,                       hasParts);
            edit.addSeparator();
            edit.addCommandItem(&cm, CommandIDs::editBridgePartials);
            edit.addCommandItem(&cm, CommandIDs::editCrossfadeOverlap);
            edit.addSeparator();
            addItem(edit, CommandIDs::normalize,      hasParts && notNorm);
            addItem(edit, CommandIDs::scaleAmplitude, hasParts && notNorm);
            addItem(edit, CommandIDs::editFadeIn,     hasParts && notNorm && hasFadeRange);
            addItem(edit, CommandIDs::editFadeOut,    hasParts && notNorm && hasFadeRange);
            return edit;
        }

        void menuItemSelected(int, int) override {} // commands handled by command manager
    };

    // ── Document window ───────────────────────────────────────────────────────

    class MainWindow : public juce::DocumentWindow {
    public:
        MainWindow(const juce::String& name, PartialFKRApplication& ownerApp)
            : juce::DocumentWindow(
                  name,
                  juce::Desktop::getInstance()
                      .getDefaultLookAndFeel()
                      .findColour(juce::ResizableWindow::backgroundColourId),
                  juce::DocumentWindow::allButtons),
              app(ownerApp)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
            setResizeLimits(800, getHeight(), 10000, 10000);
#if !JUCE_MAC
            setMenuBar(&ownerApp.appMenu);
#endif
        }

#if !JUCE_MAC
        ~MainWindow() override { setMenuBar(nullptr); }
#endif

        // When this window becomes the active (frontmost) window, make its
        // MainComponent the first target in the command dispatch chain so that
        // the menu bar reflects this window's state (undo availability, etc.).
        void activeWindowStatusChanged() override
        {
            if (isActiveWindow())
            {
                if (auto* mc = dynamic_cast<MainComponent*>(getContentComponent()))
                {
                    app.commandManager.registerAllCommandsForTarget(mc);
                    app.commandManager.setFirstCommandTarget(mc);
                    app.appMenu.activeComponent = mc;
                    mc->onMenuStateChanged  = [&app = app] { app.appMenu.menuItemsChanged(); };
                    mc->onCloseRequested    = [wPtr = static_cast<MainWindow*>(getTopLevelComponent())]
                                             { wPtr->requestClose(); };
                    app.appMenu.menuItemsChanged();  // rebuild immediately for new frontmost window
                }
            }
        }

        bool keyPressed(const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress('o', juce::ModifierKeys::commandModifier, 0))
            {
                app.openProjectDialog();
                return true;
            }
            if (key == juce::KeyPress('n', juce::ModifierKeys::commandModifier, 0))
            {
                app.createNewWindow();
                return true;
            }
            if (key == juce::KeyPress('w', juce::ModifierKeys::commandModifier, 0))
            {
                requestClose();
                return true;
            }
            return juce::DocumentWindow::keyPressed(key);
        }

        void closeButtonPressed() override
        {
            auto* mc = dynamic_cast<MainComponent*>(getContentComponent());
            if (mc == nullptr || !mc->isDirty())
            {
                juce::MessageManager::callAsync([appPtr = &app, wPtr = this]() {
                    appPtr->windowClosed(wPtr);
                });
                return;
            }

            const juce::String name = mc->getDisplayName();
            juce::AlertWindow::showAsync(
                juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::QuestionIcon)
                    .withTitle("Save \"" + name + "\"?")
                    .withMessage("Do you want to save your changes before closing?")
                    .withButton("Save")
                    .withButton("Don't Save")
                    .withButton("Cancel"),
                [appPtr = &app, wPtr = this, mc](int result)
                {
                    if (result == 1)  // Save
                    {
                        if (mc->getCurrentFile().existsAsFile())
                        {
                            mc->saveCurrentFile();
                            juce::MessageManager::callAsync([appPtr, wPtr] {
                                appPtr->windowClosed(wPtr);
                            });
                        }
                        else
                        {
                            mc->saveProjectAs([appPtr, wPtr](bool saved) {
                                if (saved)
                                    juce::MessageManager::callAsync([appPtr, wPtr] {
                                        appPtr->windowClosed(wPtr);
                                    });
                            });
                        }
                    }
                    else if (result == 2)  // Don't Save
                    {
                        juce::MessageManager::callAsync([appPtr, wPtr] {
                            appPtr->windowClosed(wPtr);
                        });
                    }
                    // result == 3 or 0 → Cancel: abort any in-progress quit
                    else { appPtr->cancelQuit(); }
                });
        }

        void requestClose() { closeButtonPressed(); }

    private:
        PartialFKRApplication& app;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

    // ── Window management ─────────────────────────────────────────────────────

    MainWindow* getActiveMainWindow()
    {
        for (auto& w : windows)
            if (w->isActiveWindow())
                return w.get();
        return windows.empty() ? nullptr : windows.back().get();
    }

    void createNewWindow()
    {
        auto w = std::make_unique<MainWindow>(getApplicationName(), *this);

        if (!windows.empty())
        {
            auto pos = windows.back()->getPosition();
            w->setTopLeftPosition(pos.x + 30, pos.y + 30);
        }

        windows.push_back(std::move(w));

        // Set the command target, active component, and menu-state callback eagerly.
        if (auto* mc = dynamic_cast<MainComponent*>(windows.back()->getContentComponent()))
        {
            commandManager.registerAllCommandsForTarget(mc);
            commandManager.setFirstCommandTarget(mc);
            appMenu.activeComponent  = mc;
            mc->onMenuStateChanged = [this] { appMenu.menuItemsChanged(); };
            mc->onCloseRequested   = [w = windows.back().get()] { w->requestClose(); };
        }
    }

    void windowClosed(MainWindow* w)
    {
        // Clear all references to the closing window's MainComponent before it is destroyed
        if (appMenu.activeComponent != nullptr)
        {
            appMenu.activeComponent->onMenuStateChanged = nullptr;
            appMenu.activeComponent->onCloseRequested   = nullptr;
        }
        commandManager.setFirstCommandTarget(nullptr);
        appMenu.activeComponent = nullptr;

        windows.erase(
            std::remove_if(windows.begin(), windows.end(),
                           [w](const auto& p) { return p.get() == w; }),
            windows.end());

        if (windows.empty())
        {
            quit();
        }
        else if (isQuitting)
        {
            // Continue closing remaining dirty windows as part of a quit sequence.
            tryQuit();
        }
        else
        {
            // Bring another window to front — triggers activeWindowStatusChanged
            // which re-establishes the command target for the new frontmost window.
            windows.back()->toFront(true);
        }
    }

    void tryQuit()
    {
        for (auto& w : windows)
        {
            auto* mc = dynamic_cast<MainComponent*>(w->getContentComponent());
            if (mc && mc->isDirty())
            {
                w->requestClose();  // shows Save / Don't Save / Cancel dialog
                return;
            }
        }
        quit();  // no dirty windows remain
    }

    void cancelQuit() { isQuitting = false; }

private:
    PartialFKRLookAndFeel lookAndFeel;
    juce::ApplicationCommandManager commandManager;
    AppMenuBarModel appMenu{commandManager};
    std::vector<std::unique_ptr<MainWindow>> windows;
    bool isQuitting = false;
#if JUCE_DEBUG
    std::unique_ptr<ScreenshotHelper> shotHelper;
#endif
};

START_JUCE_APPLICATION(PartialFKRApplication)