using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Diagnostics;

namespace UnSospiro
{
    [TestClass]
    public class InterpreterTest
    {
        private const string exePath = "unsospiro.exe"; // Current directory is the one that contains the executable.

        [TestMethod]
        public void Test_Add_1()
        {
            // Current directory is the one that contains the executable.
            string scriptFile = "../tests/test_add_1.txt";
            Assert.IsTrue(File.Exists(scriptFile));

            Process p = Process.Start(exePath, scriptFile);
            p.WaitForExit();
            int exitCode = p.ExitCode;

            Assert.AreEqual(exitCode, 0);
        }

        [TestMethod]
        public void Test_Add_2()
        {
            string scriptFile = "../tests/test_add_2.txt";
            Assert.IsTrue(File.Exists(scriptFile));

            Process p = Process.Start(exePath, scriptFile);
            p.WaitForExit();
            int exitCode = p.ExitCode;

            // Source code is erroneous by intention.
            Assert.IsTrue(exitCode == 65 || exitCode == 70);
        }

        [TestMethod]
        public void Test_This()
        {
            string scriptFile = "../tests/test_this.txt";
            Assert.IsTrue(File.Exists(scriptFile));

            Process p = Process.Start(exePath, scriptFile);
            p.WaitForExit();
            int exitCode = p.ExitCode;

            Assert.AreEqual(exitCode, 0);
        }
    }
}