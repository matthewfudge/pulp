// PulpStandaloneApp.cpp
//
// Custom JUCE standalone app for the Pulp-UI build (DDD_USE_PULP_UI). JUCE's
// default StandaloneFilterWindow wraps the editor in its own chrome — a JUCE-
// drawn title bar with an "Options" button plus an "Audio input is muted…" /
// "Settings…" notification strip — which shows JUCE UI around our all-Pulp
// editor and forces a non-native title bar (no macOS full-screen).
//
// This replaces it with a bare DocumentWindow that:
//   • uses the NATIVE macOS title bar (traffic lights + full-screen button),
//   • shows ONLY the plugin editor (the Pulp view) — no Options bar, no
//     notification strip, no JUCE chrome,
// while still using JUCE's public StandalonePluginHolder to host the plugin,
// open the audio device, and save/restore state.
//
// Wired in via -DJUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1 on the
// DreamDateFX_Standalone target (see Projects/DreamDateFX/CMakeLists.txt), which
// makes juce_audio_plugin_client_Standalone.cpp defer to our juce_CreateApplication().

// juce_StandaloneFilterWindow.h assumes the JUCE module headers are already in
// scope (it's normally included from inside the juce module .cpp). Pull the
// high-level umbrella in first — it brings juce_audio_processors / juce_gui_basics
// / juce_audio_devices (AudioProcessor, DocumentWindow, AudioDeviceManager, …).
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "ScalableEditorHost.h"   // ddd::ScalableEditorHost — fit-to-window scaling seam

namespace juce
{

//==============================================================================
// Fills the window and scales the fixed-aspect editor to fit (uniform, centred,
// letterboxed) so a freely-resizable / full-screen native window scales the Pulp
// UI without distortion. The editor stays at base*scale; the Pulp overlay (sized
// to the editor's bounds) scales its design to match.
class PulpScalingHost final : public Component
{
public:
    explicit PulpScalingHost (AudioProcessorEditor& ed) : editor (ed)
    {
        setOpaque (true);
        addAndMakeVisible (editor);
    }

    void resized() override
    {
        if (auto* scalable = dynamic_cast<ddd::ScalableEditorHost*> (&editor))
            scalable->applyWindowScale (getWidth(), getHeight());   // editor → base*scale (fit)

        editor.setTopLeftPosition ((getWidth()  - editor.getWidth())  / 2,
                                   (getHeight() - editor.getHeight()) / 2);
    }

    void paint (Graphics& g) override { g.fillAll (Colour (0xffddd2eb)); }  // lavender letterbox

private:
    AudioProcessorEditor& editor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PulpScalingHost)
};

//==============================================================================
// Native-frame window that hosts only the plugin editor (via the scaling host).
class PulpStandaloneWindow final : public DocumentWindow
{
public:
    PulpStandaloneWindow (const String& title, std::unique_ptr<StandalonePluginHolder> holderIn)
        : DocumentWindow (title,
                          Colour (0xffddd2eb),
                          // maximiseButton + resizable => JUCE sets the native
                          // FullScreenPrimary collection behavior, so the green
                          // traffic-light shows the full-screen arrows (not zoom).
                          DocumentWindow::minimiseButton | DocumentWindow::maximiseButton | DocumentWindow::closeButton),
          pluginHolder (std::move (holderIn))
    {
        setUsingNativeTitleBar (true);   // native macOS frame → traffic lights + full-screen
        setResizable (true, false);

        if (auto* proc = pluginHolder != nullptr ? pluginHolder->processor.get() : nullptr)
        {
            if (proc->hasEditor())
            {
                if (auto* ed = proc->createEditorIfNeeded())
                {
                    editor.reset (ed);
                    const int w = editor->getWidth()  > 0 ? editor->getWidth()  : 1000;
                    const int h = editor->getHeight() > 0 ? editor->getHeight() : 536;
                    scalingHost = std::make_unique<PulpScalingHost> (*editor);
                    setContentNonOwned (scalingHost.get(), false);   // host fills the window
                    centreWithSize (w, h);
                }
            }
        }

        setVisible (true);
    }

    ~PulpStandaloneWindow() override
    {
        clearContentComponent();
        scalingHost = nullptr;
        editor = nullptr;          // deletes the editor → AudioProcessor::editorBeingDeleted
        pluginHolder = nullptr;
    }

    void closeButtonPressed() override
    {
        if (auto* app = JUCEApplication::getInstance())
            app->systemRequestedQuit();
    }

    std::unique_ptr<StandalonePluginHolder> pluginHolder;

private:
    std::unique_ptr<AudioProcessorEditor> editor;
    std::unique_ptr<PulpScalingHost>      scalingHost;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PulpStandaloneWindow)
};

//==============================================================================
class PulpStandaloneApp final : public JUCEApplication
{
public:
    PulpStandaloneApp()
    {
        PropertiesFile::Options options;
        options.applicationName     = CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        options.folderName          = "";
        appProperties.setStorageParameters (options);
    }

    const String getApplicationName() override              { return CharPointer_UTF8 (JucePlugin_Name); }
    const String getApplicationVersion() override           { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override              { return true; }
    void anotherInstanceStarted (const String&) override    {}

    void initialise (const String&) override
    {
        if (Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            jassertfalse;   // no displays → no window
            return;
        }

        mainWindow.reset (new PulpStandaloneWindow (getApplicationName(), createPluginHolder()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr && mainWindow->pluginHolder != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (ModalComponentManager::getInstance()->cancelAllModalComponents())
            Timer::callAfterDelay (100, []
            {
                if (auto* app = JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        else
            quit();
    }

private:
    std::unique_ptr<StandalonePluginHolder> createPluginHolder()
    {
       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr StandalonePluginHolder::PluginInOuts channels[] { JucePlugin_PreferredChannelConfigurations };
        const Array<StandalonePluginHolder::PluginInOuts> channelConfig (channels, numElementsInArray (channels));
       #else
        const Array<StandalonePluginHolder::PluginInOuts> channelConfig;
       #endif

        return std::make_unique<StandalonePluginHolder> (appProperties.getUserSettings(),
                                                         false,       // don't take ownership of settings
                                                         String{},    // preferred default device name
                                                         nullptr,     // preferred setup options
                                                         channelConfig,
                                                         false);      // autoOpenMidiDevices
    }

    ApplicationProperties appProperties;
    std::unique_ptr<PulpStandaloneWindow> mainWindow;
};

} // namespace juce

// Custom app entry point (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP). JUCE's
// JUCE_MAIN_FUNCTION_DEFINITION wires main() → juce_CreateApplication().
juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new juce::PulpStandaloneApp(); }
