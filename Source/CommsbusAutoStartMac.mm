// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2020 Jesse Chappell
// Commsbus fork additions Copyright (C) 2026 LifeNZ

#include "CommsbusAutoStart.h"

#include <juce_core/system/juce_TargetPlatform.h>

#if JUCE_MAC

#import <Cocoa/Cocoa.h>

#include <unistd.h>

// NB: deliberately no "using namespace juce" here. Cocoa drags in Carbon's
// MacTypes.h, whose ::Point clashes with juce::Point -- the same reason
// CrossPlatformUtilsMac.mm qualifies everything explicitly.

namespace CommsbusAutoStart
{

static const char * kAgentLabel = "org.lifenz.commsbus";

static juce::File getAgentFile()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
             .getChildFile ("Library/LaunchAgents")
             .getChildFile (juce::String (kAgentLabel) + ".plist");
}

/** Absolute path of the running executable inside the .app bundle. */
static juce::String getExecutablePath()
{
    // currentExecutableFile resolves to Contents/MacOS/<name> inside the bundle,
    // which is exactly what launchd needs to exec.
    return juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName();
}

static juce::String getGuiDomainTarget()
{
    return "gui/" + juce::String ((int) getuid());
}

/**
 * Runs launchctl so a change takes effect now rather than at the next login.
 * A failure here is not fatal: the agent file is what actually persists, and
 * launchd picks it up at the next login regardless.
 */
static void runLaunchctl (const juce::StringArray & args)
{
    juce::StringArray full;
    full.add ("/bin/launchctl");
    full.addArray (args);

    juce::ChildProcess proc;

    if (proc.start (full, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        proc.waitForProcessToFinish (5000);
}

bool isSupported()
{
    return true;
}

bool isEnabled()
{
    return getAgentFile().existsAsFile();
}

juce::String getRegistrationPath()
{
    return getAgentFile().getFullPathName();
}

bool setEnabled (bool shouldBeEnabled, juce::String & errorMessage)
{
    errorMessage.clear();

    const juce::File agentFile = getAgentFile();

    if (! shouldBeEnabled)
    {
        if (! agentFile.existsAsFile())
            return true;

        runLaunchctl ({ "bootout", getGuiDomainTarget() + "/" + juce::String (kAgentLabel) });

        if (! agentFile.deleteFile())
        {
            errorMessage = TRANS ("Could not remove the startup item at") + " " + agentFile.getFullPathName();
            return false;
        }

        return true;
    }

    const juce::String exePath = getExecutablePath();

    if (! juce::File (exePath).existsAsFile())
    {
        errorMessage = TRANS ("Could not locate the Commsbus executable to register for startup.");
        return false;
    }

    const juce::File parent = agentFile.getParentDirectory();

    if (! parent.exists())
    {
        const juce::Result res = parent.createDirectory();

        if (res.failed())
        {
            errorMessage = res.getErrorMessage();
            return false;
        }
    }

    // KeepAlive/SuccessfulExit=false means launchd restarts Commsbus if it exits
    // abnormally, but leaves it alone when the operator quits it deliberately.
    juce::String plist;
    plist << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
          << "<plist version=\"1.0\">\n"
          << "<dict>\n"
          << "\t<key>Label</key>\n\t<string>" << kAgentLabel << "</string>\n"
          << "\t<key>ProgramArguments</key>\n\t<array>\n"
          << "\t\t<string>" << exePath.replace ("&", "&amp;").replace ("<", "&lt;") << "</string>\n"
          << "\t</array>\n"
          << "\t<key>RunAtLoad</key>\n\t<true/>\n"
          << "\t<key>KeepAlive</key>\n\t<dict>\n"
          << "\t\t<key>SuccessfulExit</key>\n\t\t<false/>\n"
          << "\t</dict>\n"
          << "\t<key>ProcessType</key>\n\t<string>Interactive</string>\n"
          << "</dict>\n"
          << "</plist>\n";

    if (! agentFile.replaceWithText (plist))
    {
        errorMessage = TRANS ("Could not write the startup item at") + " " + agentFile.getFullPathName();
        return false;
    }

    // Re-bootstrap so an edited agent is reloaded rather than left stale.
    runLaunchctl ({ "bootout", getGuiDomainTarget() + "/" + juce::String (kAgentLabel) });
    runLaunchctl ({ "bootstrap", getGuiDomainTarget(), agentFile.getFullPathName() });

    return true;
}

}

#endif
