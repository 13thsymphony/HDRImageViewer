
using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using OpenQA.Selenium.Appium;
using OpenQA.Selenium.Appium.Windows;
using OpenQA.Selenium.Remote;

namespace AppTests
{
    public class HdrImageViewerTestSession
    {
        private const string WindowsApplicationDriverUrl = "http://127.0.0.1:4723";
        private const string AppId = "24700lepusmagnum.HDRandWCGImageViewer_35x0ett40cyc6!App";

        protected static WindowsDriver<WindowsElement> session;

            public static void Setup(TestContext context)
        {
            // Launch application if it is not yet launched
            if (session == null)
            {
                // Create a new session to bring up an instance of the application.
                // Note: Multiple app windows (instances) share the same process Id.
                AppiumOptions appCapabilities = new AppiumOptions();
                appCapabilities.AddAdditionalCapability("app", AppId);
                appCapabilities.AddAdditionalCapability("deviceName", "WindowsPC");
                session = new WindowsDriver<WindowsElement>(new Uri(WindowsApplicationDriverUrl), appCapabilities);
                Assert.IsNotNull(session);

                // Set implicit timeout to 1.5 seconds to make element search to retry every 500 ms for at most three times
                session.Manage().Timeouts().ImplicitWait = TimeSpan.FromSeconds(1.5);
            }
        }

        public static void TearDown()
        {
            // Close the application and delete the session
            if (session != null)
            {
                session.Quit();
                session = null;
            }
        }
    }
}
