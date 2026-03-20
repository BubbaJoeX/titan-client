using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;

using Autodesk.Maya.OpenMaya;
using Autodesk.Maya;

[assembly: System.Runtime.Versioning.SupportedOSPlatformAttribute("windows")]
[assembly: ExtensionPlugin(typeof(metadata.DockPlugin), "Any")]
[assembly: MPxCommandClass(typeof(metadata.Dock), "mdexplorer")]

namespace metadata
{
    // This command class will stay around
    public class Dock : MPxCommand, IMPxCommand
    {
        // Objects to keep around
        MainForm wnd;

        override public void doIt(MArgList args)
        {
			wnd = new MainForm();
            wnd.ShowDialog();
			wnd = null;
        }

        ~Dock()
        {
            wnd = null;
        }
    }

    // This class is instantiated by Maya once and kept alive for the duration of the session.
    public class DockPlugin : IExtensionPlugin
    {
        bool IExtensionPlugin.InitializePlugin()
        {
            return true;
        }

        bool IExtensionPlugin.UninitializePlugin()
        {
            return true;
        }

        string IExtensionPlugin.GetMayaDotNetSdkBuildVersion()
        {
            String version = "201353";
            return version;
        }
    }
}

