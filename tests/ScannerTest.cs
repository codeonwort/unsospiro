using Microsoft.VisualStudio.TestTools.UnitTesting;
using UnSospiro;

namespace Project.tests
{
    [TestClass]
    public class ScannerTest
    {
        [TestMethod]
        public void Test_Scanner()
        {
            MainProgram main = new MainProgram();
            // TODO: Take an abstract listener instead of concrete type?
            Scanner scanner = new Scanner("var a = 5;", main);
            var tokens = scanner.ScanTokens();
            Assert.AreEqual(tokens[0].type, TokenType.VAR);
            Assert.AreEqual(tokens[1].type, TokenType.IDENTIFIER);
            Assert.AreEqual(tokens[2].type, TokenType.EQUAL);
            Assert.AreEqual(tokens[3].type, TokenType.NUMBER);
        }
    }
}
