using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UnSospiro
{
    [TestClass]
    public class InterpreterTest
    {
        private const string exePath = "../build/unsospiro.exe";

        //[TestMethod]
        //public void Test_Add()
        //{
        //    ProcessStartInfo info = new ProcessStartInfo(exePath);
        //    info.RedirectStandardInput = true;
        //    info.RedirectStandardOutput = true;
        //    Process p = Process.Start(info);
        //
        //    StreamWriter writer = p.StandardInput;
        //    writer.WriteLine("print (2 + 3);");
        //    writer.Close();
        //
        //    string result = p.StandardOutput.ReadToEnd();
        //    p.WaitForExit();
        //    
        //    // TODO: MainProgram writes "> " before every input line so it doesn't work :/
        //    //Assert.AreEqual(result, "5");
        //}
    }
}