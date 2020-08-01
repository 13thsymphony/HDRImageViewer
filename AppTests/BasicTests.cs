using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Microsoft.VisualStudio.TestTools.UnitTesting;
using OpenQA.Selenium.Appium.Windows;
using OpenQA.Selenium.Remote;

namespace AppTests
{
    [TestClass]
    public class BasicTests : HdrImageViewerTestSession
    {
        [ClassInitialize]
        public static void ClassInitialize(TestContext context)
        {
            // Create session to launch app window.
            Setup(context);
        }

        [ClassCleanup]
        public static void ClassCleanup()
        {
            TearDown();
        }

        [TestInitialize]
        public void Clear()
        {
        }

        [TestMethod]
        public void TestMethod1()
        {
            session.FindElementByXPath("/Button[@ClassName=\"Button\"][@Name=\"Open\"]").Click();
        }
    }
}
